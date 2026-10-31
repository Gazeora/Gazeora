// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/capture/frame_dump.h"

#include <windows.h>
#include <d3dcompiler.h>
#include <emmintrin.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gazeora {

namespace {

enum class Layout { BGRA8, RGBA8, RGB10A2 };

// What every pass shares: a full-screen triangle, the constants and the samplers.
const char kCommonShader[] = R"(
cbuffer Constants : register(b0) {
    float4 lens;    // k1, k2, image scale, chroma correction
    float4 layout;  // pass (0 scale, 1 warp, 2 warp a pair), lens separation, eye aspect,
                    // source aspect
    float4 stereo;  // depth, pop-out, DepthEncoding, an average from earlier frames (0/1)
};
SamplerState linearClamp : register(s0);
SamplerState pointClamp : register(s1);
struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Vertex vs(uint id : SV_VertexID) {
    Vertex v;
    v.uv = float2((id << 1) & 2, id & 2);
    v.position = float4(v.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return v;
}
)";

// Scaling samples the source bilinearly. At exactly half size (4K to 1080p) every output
// pixel lands between four source pixels: a 2x2 box filter.
//
// For VR the output is two eye views side by side. Each output pixel is measured from its
// lens center (in units of half the eye's height) and pushed outwards by the distortion
// polynomial before sampling, which squeezes the picture into a barrel; the viewer's
// lenses stretch it back flat. Red and blue get slightly different amounts to cancel the
// lenses' color fringes. A pair of views (3D) has each eye's own in its half of the source.
const char kScaleShader[] = R"(
Texture2D source : register(t0);
float Channel(float2 p, float channelScale, int channel, bool right) {
    float r2 = dot(p, p);
    float2 q = p * (1 + lens.x * r2 + lens.y * r2 * r2) * channelScale;
    float2 uv = 0.5 + q / (2 * lens.z) * float2(1 / layout.w, 1);
    if (any(uv < 0) || any(uv > 1)) return 0;
    if (layout.x == 2) uv.x = (uv.x + (right ? 1 : 0)) / 2;
    return source.Sample(linearClamp, uv)[channel];
}
float4 ps(Vertex v) : SV_Target {
    if (layout.x == 0) return float4(source.Sample(linearClamp, v.uv).rgb, 1);
    bool right = v.uv.x >= 0.5;
    float2 eye = float2(frac(v.uv.x * 2), v.uv.y);
    float center = right ? layout.y : 1 - layout.y;
    float2 p = (eye - float2(center, 0.5)) * float2(layout.z, 1) * 2;
    return float4(Channel(p, 1 - lens.w, 0, right), Channel(p, 1, 1, right),
                  Channel(p, 1 + lens.w, 2, right), 1);
}
)";

// The hook's depth words (RGBA8, a byte a channel) as depth values 0..1, near things widened
// by 2 pixels each side and 1 up and down: a game blends an object's edge pixels with what's
// behind (antialiasing), and TAA may put its picture a pixel off its depth, so edge pixels can
// have the background's depth. Shifted with the background, they'd tear off the object's side;
// widened, they go with it. Which end is near comes from the last frames' average; before
// there is one, a guess could widen the background instead, so the first frame isn't widened.
const char kDecodeDepthShader[] = R"(
Texture2D raw : register(t0);
Texture2D<float2> focus : register(t1);
float Decode(int2 at) {
    uint4 b = uint4(raw.Load(int3(at, 0)) * 255 + 0.5);
    uint bits = b.r | (b.g << 8) | (b.b << 16) | (b.a << 24);
    return saturate(stereo.z == 2 ? (bits & 0xFFFFFF) / 16777215.0 : asfloat(bits));
}
float ps(Vertex v) : SV_Target {
    uint width, height;
    raw.GetDimensions(width, height);
    int2 at = int2(v.position.xy);
    if (stereo.w == 0) return Decode(at);
    bool reversed = focus.Load(int3(0, 0, 0)).x < 0.5;
    int2 last = int2(width, height) - 1;
    float nearest = reversed ? 0 : 1;
    [unroll] for (int dy = -1; dy <= 1; ++dy) {
        [unroll] for (int dx = -2; dx <= 2; ++dx) {
            float d = Decode(clamp(at + int2(dx, dy), int2(0, 0), last));
            nearest = reversed ? max(nearest, d) : min(nearest, d);
        }
    }
    return nearest;
}
)";

// Where the scene is: the frame's average depth (the top of the mip chain) and the depth
// around the middle of the view, where a third-person game has its character (a mip level
// about an eighth of the frame across, sampled between its middle texels).
//
// The middle's depth is where the views converge, so every change to it moves the whole
// scene in or out. It has to hold still while the picture merely lives: a character breathing
// or turning in a menu, a camera swaying, things passing the middle. So it only follows a
// change of more than 15% (in 1/distance), and then slowly, over about a second and a half;
// the average, which only tells the depth order apart, eases in faster.
const char kMeanDepthShader[] = R"(
Texture2D<float> depth : register(t0);
Texture2D<float2> previous : register(t1);
float2 ps(Vertex v) : SV_Target {
    uint width, height, levels;
    depth.GetDimensions(0, width, height, levels);
    float2 now = float2(depth.Load(int3(0, 0, levels - 1)),
                        depth.SampleLevel(linearClamp, float2(0.5, 0.5), max(int(levels) - 4, 0)));
    if (stereo.w == 0) return now;
    float2 before = previous.Load(int3(0, 0, 0));
    float mean = lerp(before.x, now.x, 0.1);
    bool reversed = mean < 0.5;
    float wBefore = reversed ? before.y : 1 - before.y;
    float wNow = reversed ? now.y : 1 - now.y;
    float w = abs(wNow - wBefore) > 0.15 * wBefore ? lerp(wBefore, wNow, 0.03) : wBefore;
    return float2(mean, reversed ? w : 1 - w);
}
)";

// Two views from one picture and its depth, side by side.
//
// Games store depth so that it falls off as 1/distance: reversed (1 near, 0 far, most games
// now) or, nearly enough, 1 minus that. Mostly far is 0 in one, 1 in the other, so the
// average tells them apart. 1/distance (w, to an unknown scale) is what the gap between two
// eyes' views goes by too: like a pair of eyes converged on the subject in the middle of the
// view, a thing's parallax is depth * (1 - w / w at the subject). The subject sits at the
// screen and stays put; things farther go in, up to `depth` (a share of the eye's width) far
// away; things nearer come out, up to pop-out * depth. A subject that's sky or far away
// doesn't push the screen past half the scene's average.
//
// Each eye shifts the picture by half the parallax, its own way. An output pixel shows the
// scene point x0 whose shift lands on it, x0 + side * parallax(x0) / 2 = x, found by marching
// the parallax from nearest to farthest: the first one that fits is the surface in front.
// Where the march crosses a jump in depth instead, this eye sees behind something, where the
// game drew nothing: the farther surface fills it, the background stretched over the gap
// rather than the near thing's edge smeared out.
const char kStereoShader[] = R"(
Texture2D color : register(t0);
Texture2D<float> depth : register(t1);
Texture2D<float2> focus : register(t2);
static const int kSteps = 32;
float Parallax(float2 uv, bool reversed, float screenW) {
    float d = depth.SampleLevel(pointClamp, uv, 0);
    float w = reversed ? d : 1 - d;
    return clamp(stereo.x * (1 - w / screenW), -stereo.x * stereo.y, stereo.x);
}
float4 ps(Vertex v) : SV_Target {
    float side = v.uv.x >= 0.5 ? 1 : -1;
    float2 uv = float2(frac(v.uv.x * 2), v.uv.y);
    float2 scene = focus.Load(int3(0, 0, 0));
    bool reversed = scene.x < 0.5;
    float meanW = reversed ? scene.x : 1 - scene.x;
    float centerW = reversed ? scene.y : 1 - scene.y;
    float screenW = max(max(centerW, 0.5 * meanW), 1e-6);
    float nearest = -stereo.x * stereo.y, farthest = stereo.x;
    float step = (farthest - nearest) / kSteps;
    float before = nearest;
    float gapBefore = Parallax(uv - float2(side * nearest / 2, 0), reversed, screenW) - nearest;
    float found = farthest;
    [loop] for (int i = 1; i <= kSteps; ++i) {
        float p = nearest + step * i;
        float gap = Parallax(uv - float2(side * p / 2, 0), reversed, screenW) - p;
        if (gap <= 0) {
            float surfaceBefore = gapBefore + before, surfaceAfter = gap + p;
            if (abs(surfaceAfter - surfaceBefore) > 2 * step) {
                found = surfaceAfter > surfaceBefore ? p : before;  // the farther one
            } else {
                found = lerp(before, p, gapBefore / max(gapBefore - gap, 1e-6));
            }
            break;
        }
        before = p;
        gapBefore = gap;
    }
    return float4(color.SampleLevel(linearClamp, uv - float2(side * found / 2, 0), 0).rgb, 1);
}
)";

bool IsSrgb(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

// What the passes draw into for a source of `format`. sRGB in, sRGB out: the bytes stay
// gamma-encoded, filtered in linear light.
DXGI_FORMAT TargetFormat(DXGI_FORMAT format) {
    return IsSrgb(format) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
}

bool LayoutFor(DXGI_FORMAT format, Layout& layout) {
    switch (format) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            layout = Layout::BGRA8;
            return true;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            layout = Layout::RGBA8;
            return true;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            layout = Layout::RGB10A2;
            return true;
        default:
            return false;
    }
}

// One 32-bit pixel to BGRA8 with opaque alpha. 10-bit channels keep their top 8 bits.
inline uint32_t ToBgra(Layout layout, uint32_t v) {
    switch (layout) {
        case Layout::BGRA8:
            return v | 0xFF000000;
        case Layout::RGBA8:
            return (v & 0x0000FF00) | ((v & 0xFF) << 16) | ((v >> 16) & 0xFF) | 0xFF000000;
        case Layout::RGB10A2:
            // Red from bits 2-9, green from 12-19, blue from 22-29.
            return ((v << 14) & 0x00FF0000) | ((v >> 4) & 0x0000FF00) | ((v >> 22) & 0xFF) |
                   0xFF000000;
    }
    return v;
}

// The same as ToBgra for four pixels, with SSE2 (every x64 CPU has it).
template <Layout layout>
inline __m128i ToBgra4(__m128i v) {
    const __m128i alpha = _mm_set1_epi32(int(0xFF000000));
    const __m128i low8 = _mm_set1_epi32(0xFF);
    if (layout == Layout::BGRA8) return _mm_or_si128(v, alpha);
    if (layout == Layout::RGBA8) {
        __m128i g = _mm_and_si128(v, _mm_set1_epi32(0x0000FF00));
        __m128i r = _mm_slli_epi32(_mm_and_si128(v, low8), 16);
        __m128i b = _mm_and_si128(_mm_srli_epi32(v, 16), low8);
        return _mm_or_si128(_mm_or_si128(g, r), _mm_or_si128(b, alpha));
    }
    __m128i r = _mm_and_si128(_mm_slli_epi32(v, 14), _mm_set1_epi32(0x00FF0000));
    __m128i g = _mm_and_si128(_mm_srli_epi32(v, 4), _mm_set1_epi32(0x0000FF00));
    __m128i b = _mm_and_si128(_mm_srli_epi32(v, 22), low8);
    return _mm_or_si128(_mm_or_si128(r, g), _mm_or_si128(b, alpha));
}

template <Layout layout>
void ConvertRowAs(const uint8_t* src, uint8_t* dst, UINT width) {
    UINT x = 0;
    for (; x + 4 <= width; x += 4, src += 16, dst += 16) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), ToBgra4<layout>(v));
    }
    for (; x < width; ++x, src += 4, dst += 4) {
        uint32_t v;
        memcpy(&v, src, 4);
        v = ToBgra(layout, v);
        memcpy(dst, &v, 4);
    }
}

// Every streamed frame passes through here, so the format is picked once per row, not per
// pixel, and four pixels are converted at a time.
void ConvertRow(Layout layout, const uint8_t* src, uint8_t* dst, UINT width) {
    switch (layout) {
        case Layout::BGRA8:
            ConvertRowAs<Layout::BGRA8>(src, dst, width);
            break;
        case Layout::RGBA8:
            ConvertRowAs<Layout::RGBA8>(src, dst, width);
            break;
        case Layout::RGB10A2:
            ConvertRowAs<Layout::RGB10A2>(src, dst, width);
            break;
    }
}

bool WriteBmp(const std::wstring& path, UINT width, UINT height, const std::vector<uint8_t>& bgra,
              std::wstring& error) {
    BITMAPINFOHEADER info = {};
    info.biSize = sizeof(info);
    info.biWidth = LONG(width);
    info.biHeight = -LONG(height);  // top-down
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;

    BITMAPFILEHEADER file = {};
    file.bfType = 0x4D42;  // "BM"
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = DWORD(file.bfOffBits + bgra.size());

    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) {
        error = L"cannot open " + path + L" for writing";
        return false;
    }
    fwrite(&file, sizeof(file), 1, f);
    fwrite(&info, sizeof(info), 1, f);
    fwrite(bgra.data(), 1, bgra.size(), f);
    fclose(f);
    return true;
}

}  // namespace

bool TextureReader::CreateShaders() {
    if (m_pixelShader) {
        return true;
    }
    if (m_shaderFailed) {
        return false;
    }
    auto compile = [](const char* body, const char* entry, const char* target,
                      ComPtr<ID3DBlob>& blob) {
        std::string source = std::string(kCommonShader) + body;
        return SUCCEEDED(D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr, entry,
                                    target, 0, 0, &blob, nullptr));
    };
    auto pixelShader = [&](const ComPtr<ID3DBlob>& blob, ComPtr<ID3D11PixelShader>& shader) {
        return SUCCEEDED(m_device->CreatePixelShader(blob->GetBufferPointer(),
                                                     blob->GetBufferSize(), nullptr, &shader));
    };
    ComPtr<ID3DBlob> vs, ps, decode, mean, stereo;
    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    D3D11_SAMPLER_DESC pointSampler = sampler;
    pointSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    D3D11_BUFFER_DESC constants = {};
    constants.ByteWidth = 48;
    constants.Usage = D3D11_USAGE_DEFAULT;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    m_shaderFailed = !compile("", "vs", "vs_4_0", vs) ||
                     !compile(kScaleShader, "ps", "ps_4_0", ps) ||
                     !compile(kDecodeDepthShader, "ps", "ps_4_0", decode) ||
                     !compile(kMeanDepthShader, "ps", "ps_4_0", mean) ||
                     !compile(kStereoShader, "ps", "ps_4_0", stereo) ||
                     FAILED(m_device->CreateVertexShader(
                         vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_vertexShader)) ||
                     !pixelShader(ps, m_pixelShader) || !pixelShader(decode, m_decodeShader) ||
                     !pixelShader(mean, m_meanShader) || !pixelShader(stereo, m_stereoShader) ||
                     FAILED(m_device->CreateSamplerState(&sampler, &m_sampler)) ||
                     FAILED(m_device->CreateSamplerState(&pointSampler, &m_pointSampler)) ||
                     FAILED(m_device->CreateBuffer(&constants, nullptr, &m_constants));
    if (m_shaderFailed) {
        m_pixelShader.Reset();
        return false;
    }
    return true;
}

void TextureReader::SetConstants(ID3D11DeviceContext* context, float mode, float sourceAspect,
                                 float eyeAspect, const DepthFrame* depth) {
    float constants[12] = {m_vr.k1,
                           m_vr.k2,
                           std::max(m_vr.imageScale, 0.01f),
                           m_vr.chromaCorrection,
                           mode,
                           m_vr.lensSeparation,
                           eyeAspect,
                           sourceAspect,
                           m_vr.stereoDepth,
                           std::clamp(m_vr.stereoPopOut, 0.0f, 1.0f),
                           depth ? float(depth->encoding) : 0.0f,
                           m_meanValid ? 1.0f : 0.0f};
    context->UpdateSubresource(m_constants.Get(), 0, nullptr, constants, 0, 0);
}

void TextureReader::Draw(ID3D11DeviceContext* context, Pass pass, ID3D11RenderTargetView* target,
                         UINT width, UINT height,
                         std::initializer_list<ID3D11ShaderResourceView*> views) {
    ID3D11PixelShader* shader = pass == Pass::DecodeDepth ? m_decodeShader.Get()
                                : pass == Pass::MeanDepth ? m_meanShader.Get()
                                : pass == Pass::Stereo    ? m_stereoShader.Get()
                                                          : m_pixelShader.Get();
    ID3D11ShaderResourceView* bound[3] = {};
    UINT count = 0;
    for (ID3D11ShaderResourceView* view : views) {
        if (count < 3) bound[count++] = view;
    }
    D3D11_VIEWPORT viewport = {0, 0, float(width), float(height), 0, 1};
    ID3D11SamplerState* samplers[2] = {m_sampler.Get(), m_pointSampler.Get()};
    ID3D11Buffer* constantBuffer = m_constants.Get();
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(shader, nullptr, 0);
    context->PSSetShaderResources(0, 3, bound);
    context->PSSetSamplers(0, 2, samplers);
    context->PSSetConstantBuffers(0, 1, &constantBuffer);
    context->RSSetState(nullptr);
    context->RSSetViewports(1, &viewport);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->OMSetRenderTargets(1, &target, nullptr);
    context->Draw(3, 0);
    // Leave nothing bound: the next pass may write what this one read.
    ID3D11ShaderResourceView* none[3] = {};
    context->PSSetShaderResources(0, 3, none);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

ID3D11ShaderResourceView* TextureReader::SourceView(ID3D11Texture2D* texture,
                                                    const D3D11_TEXTURE2D_DESC& source) {
    if (m_source.Get() != texture) {
        m_source.Reset();
        m_sourceView.Reset();
        if (!(source.BindFlags & D3D11_BIND_SHADER_RESOURCE) ||
            FAILED(m_device->CreateShaderResourceView(texture, nullptr, &m_sourceView))) {
            return nullptr;
        }
        m_source = texture;
    }
    return m_sourceView.Get();
}

bool TextureReader::PrepareTarget(const D3D11_TEXTURE2D_DESC& source, UINT width, UINT height) {
    DXGI_FORMAT format = TargetFormat(source.Format);
    D3D11_TEXTURE2D_DESC scaled = {};
    if (m_scaled) {
        m_scaled->GetDesc(&scaled);
    }
    if (m_scaled && scaled.Width == width && scaled.Height == height && scaled.Format == format) {
        return true;
    }
    scaled = {};
    scaled.Width = width;
    scaled.Height = height;
    scaled.MipLevels = 1;
    scaled.ArraySize = 1;
    scaled.Format = format;
    scaled.SampleDesc.Count = 1;
    scaled.Usage = D3D11_USAGE_DEFAULT;
    // Also read in a pass: GpuNv12Converter takes it from here.
    scaled.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    m_scaled.Reset();
    m_scaledView.Reset();
    if (FAILED(m_device->CreateTexture2D(&scaled, nullptr, &m_scaled)) ||
        FAILED(m_device->CreateRenderTargetView(m_scaled.Get(), nullptr, &m_scaledView))) {
        m_scaled.Reset();
        return false;
    }
    return true;
}

bool TextureReader::Render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* view,
                           const D3D11_TEXTURE2D_DESC& source, UINT width, UINT height, Pass pass) {
    if (!view || !PrepareTarget(source, width, height)) {
        return false;
    }
    float sourceAspect = float(source.Width) / float(source.Height);
    if (pass == Pass::WarpPair) {
        sourceAspect /= 2;  // each eye's view is half of it
    }
    float mode = pass == Pass::Scale ? 0.0f : pass == Pass::Warp ? 1.0f : 2.0f;
    SetConstants(context, mode, sourceAspect, float(width) / 2 / float(height), nullptr);
    Draw(context, pass, m_scaledView.Get(), width, height, {view});
    return true;
}

bool TextureReader::PrepareDepth(ID3D11DeviceContext* context, const DepthFrame& depth) {
    D3D11_TEXTURE2D_DESC raw;
    depth.texture->GetDesc(&raw);
    if (m_rawDepth.Get() != depth.texture) {
        m_rawDepth.Reset();
        m_rawDepthView.Reset();
        if (!(raw.BindFlags & D3D11_BIND_SHADER_RESOURCE) ||
            FAILED(m_device->CreateShaderResourceView(depth.texture, nullptr, &m_rawDepthView))) {
            return false;
        }
        m_rawDepth = depth.texture;
    }
    UINT width = std::min(depth.width, raw.Width), height = std::min(depth.height, raw.Height);
    D3D11_TEXTURE2D_DESC desc = {};
    if (m_depth) {
        m_depth->GetDesc(&desc);
    }
    if (!m_depth || desc.Width != width || desc.Height != height) {
        m_depth.Reset();
        m_depthTarget.Reset();
        m_depthView.Reset();
        m_meanValid = false;
        desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 0;  // all of them, down to the average
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_depth)) ||
            FAILED(m_device->CreateRenderTargetView(m_depth.Get(), nullptr, &m_depthTarget)) ||
            FAILED(m_device->CreateShaderResourceView(m_depth.Get(), nullptr, &m_depthView))) {
            m_depth.Reset();
            return false;
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (m_mean[i]) {
            continue;
        }
        D3D11_TEXTURE2D_DESC one = {};
        one.Width = one.Height = one.MipLevels = one.ArraySize = 1;
        one.Format = DXGI_FORMAT_R32G32_FLOAT;  // the average depth and the middle's
        one.SampleDesc.Count = 1;
        one.Usage = D3D11_USAGE_DEFAULT;
        one.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(m_device->CreateTexture2D(&one, nullptr, &m_mean[i])) ||
            FAILED(m_device->CreateRenderTargetView(m_mean[i].Get(), nullptr, &m_meanTarget[i])) ||
            FAILED(m_device->CreateShaderResourceView(m_mean[i].Get(), nullptr, &m_meanView[i]))) {
            m_mean[i].Reset();
            return false;
        }
    }

    SetConstants(context, 0, 0, 0, &depth);
    Draw(context, Pass::DecodeDepth, m_depthTarget.Get(), width, height,
         {m_rawDepthView.Get(), m_meanView[m_meanIndex].Get()});
    context->GenerateMips(m_depthView.Get());
    int next = 1 - m_meanIndex;
    Draw(context, Pass::MeanDepth, m_meanTarget[next].Get(), 1, 1,
         {m_depthView.Get(), m_meanView[m_meanIndex].Get()});
    m_meanIndex = next;
    m_meanValid = true;
    return true;
}

bool TextureReader::RenderStereo(ID3D11DeviceContext* context, ID3D11ShaderResourceView* color,
                                 const D3D11_TEXTURE2D_DESC& source, const DepthFrame& depth,
                                 UINT eyeWidth, UINT height, bool forWarp) {
    if (!color || eyeWidth == 0 || height == 0 || !PrepareDepth(context, depth)) {
        return false;
    }
    UINT width = eyeWidth * 2;
    ID3D11RenderTargetView* target = nullptr;
    if (forWarp) {
        DXGI_FORMAT format = TargetFormat(source.Format);
        D3D11_TEXTURE2D_DESC pair = {};
        if (m_pair) {
            m_pair->GetDesc(&pair);
        }
        if (!m_pair || pair.Width != width || pair.Height != height || pair.Format != format) {
            m_pair.Reset();
            m_pairTarget.Reset();
            m_pairView.Reset();
            pair = {};
            pair.Width = width;
            pair.Height = height;
            pair.MipLevels = 1;
            pair.ArraySize = 1;
            pair.Format = format;
            pair.SampleDesc.Count = 1;
            pair.Usage = D3D11_USAGE_DEFAULT;
            pair.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(m_device->CreateTexture2D(&pair, nullptr, &m_pair)) ||
                FAILED(m_device->CreateRenderTargetView(m_pair.Get(), nullptr, &m_pairTarget)) ||
                FAILED(m_device->CreateShaderResourceView(m_pair.Get(), nullptr, &m_pairView))) {
                m_pair.Reset();
                return false;
            }
        }
        target = m_pairTarget.Get();
    } else {
        if (!PrepareTarget(source, width, height)) {
            return false;
        }
        target = m_scaledView.Get();
    }
    SetConstants(context, 0, 0, 0, &depth);
    Draw(context, Pass::Stereo, target, width, height,
         {color, m_depthView.Get(), m_meanView[m_meanIndex].Get()});
    return true;
}

ID3D11Texture2D* TextureReader::Compose(ID3D11Device* device, ID3D11Texture2D* texture,
                                        std::wstring& error, const DepthFrame* depth) {
    m_stereo = false;
    if (!device || !texture) {
        error = L"no frame captured yet";
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC source;
    texture->GetDesc(&source);
    Layout layout;
    if (!LayoutFor(source.Format, layout)) {
        error = L"readback does not support DXGI format " + std::to_wstring(source.Format);
        return nullptr;
    }
    // The capture client recreates its device when the game moves to another adapter.
    if (m_device.Get() != device) {
        UINT maxWidth = m_maxWidth, maxHeight = m_maxHeight;
        VrLayout vr = m_vr;
        *this = TextureReader();
        SetMaxSize(maxWidth, maxHeight);
        SetVrLayout(vr);
        m_device = device;
    }
    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    // The frame's size flat: within the max size, keeping its shape.
    UINT flatWidth = source.Width, flatHeight = source.Height;
    bool scale =
        m_maxWidth && m_maxHeight && (source.Width > m_maxWidth || source.Height > m_maxHeight);
    if (scale) {
        double factor =
            std::min(double(m_maxWidth) / source.Width, double(m_maxHeight) / source.Height);
        flatWidth = std::max(1u, UINT(source.Width * factor));
        flatHeight = std::max(1u, UINT(source.Height * factor));
    }
    bool stereo = m_vr.enabled && m_vr.stereo && depth && depth->texture &&
                  depth->encoding != kDepthNone && depth->width > 0 && depth->height > 0;
    ID3D11ShaderResourceView* view = CreateShaders() ? SourceView(texture, source) : nullptr;

    if (m_vr.WarpOnPc()) {
        uint32_t vrWidth, vrHeight;
        m_vr.FrameSize(m_maxWidth ? m_maxWidth : 1920, m_maxHeight ? m_maxHeight : 1080, vrWidth,
                       vrHeight);
        m_stereo = stereo &&
                   RenderStereo(context.Get(), view, source, *depth, flatWidth, flatHeight, true);
        D3D11_TEXTURE2D_DESC pair = {};
        if (m_stereo) {
            m_pair->GetDesc(&pair);
        }
        // A flat frame in the headset would be worse than none, so there's no CPU fallback.
        if (!Render(context.Get(), m_stereo ? m_pairView.Get() : view, m_stereo ? pair : source,
                    vrWidth, vrHeight, m_stereo ? Pass::WarpPair : Pass::Warp)) {
            error = L"the VR layout needs a GPU that runs D3D11 shaders";
            m_stereo = false;
            return nullptr;
        }
        return m_scaled.Get();
    }
    if (stereo &&
        RenderStereo(context.Get(), view, source, *depth, flatWidth / 2, flatHeight, false)) {
        m_stereo = true;
        return m_scaled.Get();
    }
    // Without the GPU path, the full-size frame is read and the encoder scales it.
    if (scale && Render(context.Get(), view, source, flatWidth, flatHeight, Pass::Scale)) {
        return m_scaled.Get();
    }
    return texture;
}

bool TextureReader::Read(ID3D11Device* device, ID3D11Texture2D* texture, UINT& width, UINT& height,
                         std::vector<uint8_t>& bgra, std::wstring& error, const DepthFrame* depth) {
    ID3D11Texture2D* readFrom = Compose(device, texture, error, depth);
    if (!readFrom) {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc;
    readFrom->GetDesc(&desc);
    Layout layout;
    LayoutFor(desc.Format, layout);  // Compose only hands out formats it has a layout for
    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    if (!m_staging || memcmp(&desc, &m_stagingDesc, sizeof(desc))) {
        m_staging.Reset();
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &m_staging))) {
            error = L"failed to create staging texture";
            return false;
        }
        m_stagingDesc = desc;
    }
    context->CopyResource(m_staging.Get(), readFrom);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(m_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        error = L"failed to map staging texture";
        return false;
    }
    width = desc.Width;
    height = desc.Height;
    bgra.resize(size_t(width) * height * 4);
    // Two threads take 0.2 ms off at 1080p; more gain nothing, as most of Read is the GPU copy.
#pragma omp parallel for schedule(static) num_threads(2)
    for (int y = 0; y < int(height); ++y) {
        ConvertRow(layout, static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch,
                   bgra.data() + size_t(y) * width * 4, width);
    }
    context->Unmap(m_staging.Get(), 0);
    return true;
}

bool SaveBgraAsBmp(const std::wstring& path, UINT width, UINT height,
                   const std::vector<uint8_t>& bgra, std::wstring& error) {
    return WriteBmp(path, width, height, bgra, error);
}

bool SaveTextureAsBmp(ID3D11Device* device, ID3D11Texture2D* texture, const std::wstring& path,
                      std::wstring& error) {
    if (texture) {
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        Layout layout;
        if (!LayoutFor(desc.Format, layout)) {
            error = L"BMP export does not support DXGI format " + std::to_wstring(desc.Format);
            return false;
        }
    }
    TextureReader reader;
    UINT width = 0, height = 0;
    std::vector<uint8_t> bgra;
    if (!reader.Read(device, texture, width, height, bgra, error)) {
        return false;
    }
    return WriteBmp(path, width, height, bgra, error);
}

}  // namespace gazeora
