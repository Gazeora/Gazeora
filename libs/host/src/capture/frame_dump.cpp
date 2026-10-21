#include "gazeora/host/capture/frame_dump.h"

#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gazeora {

namespace {

enum class Layout { BGRA8, RGBA8, RGB10A2 };

// A full-screen triangle sampling the source bilinearly. At exactly half size (4K to
// 1080p) every output pixel lands between four source pixels: a 2x2 box filter.
//
// For VR the output is two eye views side by side. Each output pixel is measured from its
// lens center (in units of half the eye's height) and pushed outwards by the distortion
// polynomial before sampling, which squeezes the picture into a barrel; the viewer's
// lenses stretch it back flat. Red and blue get slightly different amounts to cancel the
// lenses' color fringes.
const char kScaleShader[] = R"(
Texture2D source : register(t0);
SamplerState linearClamp : register(s0);
cbuffer Constants : register(b0) {
    float4 lens;    // k1, k2, image scale, chroma correction
    float4 layout;  // VR on (0/1), lens separation, eye aspect, source aspect
};
struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Vertex vs(uint id : SV_VertexID) {
    Vertex v;
    v.uv = float2((id << 1) & 2, id & 2);
    v.position = float4(v.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return v;
}
float Channel(float2 p, float channelScale, int channel) {
    float r2 = dot(p, p);
    float2 q = p * (1 + lens.x * r2 + lens.y * r2 * r2) * channelScale;
    float2 uv = 0.5 + q / (2 * lens.z) * float2(1 / layout.w, 1);
    if (any(uv < 0) || any(uv > 1)) return 0;
    return source.Sample(linearClamp, uv)[channel];
}
float4 ps(Vertex v) : SV_Target {
    if (layout.x == 0) return float4(source.Sample(linearClamp, v.uv).rgb, 1);
    bool right = v.uv.x >= 0.5;
    float2 eye = float2(frac(v.uv.x * 2), v.uv.y);
    float center = right ? layout.y : 1 - layout.y;
    float2 p = (eye - float2(center, 0.5)) * float2(layout.z, 1) * 2;
    return float4(Channel(p, 1 - lens.w, 0), Channel(p, 1, 1), Channel(p, 1 + lens.w, 2), 1);
}
)";

bool IsSrgb(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

void ConvertRow(Layout layout, const uint8_t* src, uint8_t* dst, UINT width) {
    for (UINT x = 0; x < width; ++x, src += 4, dst += 4) {
        switch (layout) {
            case Layout::BGRA8:
                dst[0] = src[0], dst[1] = src[1], dst[2] = src[2];
                break;
            case Layout::RGBA8:
                dst[0] = src[2], dst[1] = src[1], dst[2] = src[0];
                break;
            case Layout::RGB10A2: {
                uint32_t v;
                memcpy(&v, src, 4);
                dst[2] = uint8_t((v & 0x3FF) >> 2);
                dst[1] = uint8_t(((v >> 10) & 0x3FF) >> 2);
                dst[0] = uint8_t(((v >> 20) & 0x3FF) >> 2);
                break;
            }
        }
        dst[3] = 255;
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

bool TextureReader::Render(ID3D11DeviceContext* context, ID3D11Texture2D* texture,
                           const D3D11_TEXTURE2D_DESC& source, UINT width, UINT height, bool vr) {
    if (m_shaderFailed) {
        return false;
    }
    if (!m_pixelShader) {
        ComPtr<ID3DBlob> vs, ps;
        D3D11_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        D3D11_BUFFER_DESC constants = {};
        constants.ByteWidth = 32;
        constants.Usage = D3D11_USAGE_DEFAULT;
        constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        m_shaderFailed =
            FAILED(D3DCompile(kScaleShader, sizeof(kScaleShader) - 1, nullptr, nullptr, nullptr,
                              "vs", "vs_4_0", 0, 0, &vs, nullptr)) ||
            FAILED(D3DCompile(kScaleShader, sizeof(kScaleShader) - 1, nullptr, nullptr, nullptr,
                              "ps", "ps_4_0", 0, 0, &ps, nullptr)) ||
            FAILED(m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(),
                                                nullptr, &m_vertexShader)) ||
            FAILED(m_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr,
                                               &m_pixelShader)) ||
            FAILED(m_device->CreateSamplerState(&sampler, &m_sampler)) ||
            FAILED(m_device->CreateBuffer(&constants, nullptr, &m_constants));
        if (m_shaderFailed) {
            m_pixelShader.Reset();
            return false;
        }
    }

    // sRGB in, sRGB out: the bytes stay gamma-encoded, filtered in linear light.
    DXGI_FORMAT format =
        IsSrgb(source.Format) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
    D3D11_TEXTURE2D_DESC scaled = {};
    if (m_scaled) {
        m_scaled->GetDesc(&scaled);
    }
    if (!m_scaled || scaled.Width != width || scaled.Height != height || scaled.Format != format) {
        scaled = {};
        scaled.Width = width;
        scaled.Height = height;
        scaled.MipLevels = 1;
        scaled.ArraySize = 1;
        scaled.Format = format;
        scaled.SampleDesc.Count = 1;
        scaled.Usage = D3D11_USAGE_DEFAULT;
        scaled.BindFlags = D3D11_BIND_RENDER_TARGET;
        m_scaled.Reset();
        m_scaledView.Reset();
        if (FAILED(m_device->CreateTexture2D(&scaled, nullptr, &m_scaled)) ||
            FAILED(m_device->CreateRenderTargetView(m_scaled.Get(), nullptr, &m_scaledView))) {
            m_scaled.Reset();
            return false;
        }
    }
    if (m_source.Get() != texture) {
        m_sourceView.Reset();
        if (!(source.BindFlags & D3D11_BIND_SHADER_RESOURCE) ||
            FAILED(m_device->CreateShaderResourceView(texture, nullptr, &m_sourceView))) {
            m_source.Reset();
            return false;
        }
        m_source = texture;
    }

    float constants[8] = {m_vr.k1,
                          m_vr.k2,
                          std::max(m_vr.imageScale, 0.01f),
                          m_vr.chromaCorrection,
                          vr ? 1.0f : 0.0f,
                          m_vr.lensSeparation,
                          float(width) / 2 / float(height),
                          float(source.Width) / float(source.Height)};
    context->UpdateSubresource(m_constants.Get(), 0, nullptr, constants, 0, 0);

    D3D11_VIEWPORT viewport = {0, 0, float(width), float(height), 0, 1};
    ID3D11ShaderResourceView* view = m_sourceView.Get();
    ID3D11Buffer* constantBuffer = m_constants.Get();
    ID3D11SamplerState* sampler = m_sampler.Get();
    ID3D11RenderTargetView* target = m_scaledView.Get();
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, &view);
    context->PSSetSamplers(0, 1, &sampler);
    context->PSSetConstantBuffers(0, 1, &constantBuffer);
    context->RSSetState(nullptr);
    context->RSSetViewports(1, &viewport);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->OMSetRenderTargets(1, &target, nullptr);
    context->Draw(3, 0);
    // Leave nothing bound between frames.
    ID3D11ShaderResourceView* noView = nullptr;
    context->PSSetShaderResources(0, 1, &noView);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    return true;
}

bool TextureReader::Read(ID3D11Device* device, ID3D11Texture2D* texture, UINT& width, UINT& height,
                         std::vector<uint8_t>& bgra, std::wstring& error) {
    if (!device || !texture) {
        error = L"no frame captured yet";
        return false;
    }

    D3D11_TEXTURE2D_DESC source;
    texture->GetDesc(&source);
    Layout layout;
    if (!LayoutFor(source.Format, layout)) {
        error = L"readback does not support DXGI format " + std::to_wstring(source.Format);
        return false;
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

    ID3D11Texture2D* readFrom = texture;
    D3D11_TEXTURE2D_DESC desc = source;
    if (m_vr.enabled) {
        uint32_t vrWidth, vrHeight;
        m_vr.FrameSize(m_maxWidth ? m_maxWidth : 1920, m_maxHeight ? m_maxHeight : 1080, vrWidth,
                       vrHeight);
        // A flat frame in the headset would be worse than none, so there's no CPU fallback.
        if (!Render(context.Get(), texture, source, vrWidth, vrHeight, true)) {
            error = L"the VR layout needs a GPU that runs D3D11 shaders";
            return false;
        }
        readFrom = m_scaled.Get();
        m_scaled->GetDesc(&desc);
        layout = Layout::BGRA8;
    } else if (m_maxWidth && m_maxHeight &&
               (source.Width > m_maxWidth || source.Height > m_maxHeight)) {
        double scale =
            std::min(double(m_maxWidth) / source.Width, double(m_maxHeight) / source.Height);
        UINT scaledWidth = std::max(1u, UINT(source.Width * scale));
        UINT scaledHeight = std::max(1u, UINT(source.Height * scale));
        // Without the GPU path, the full-size frame is read and the encoder scales it.
        if (Render(context.Get(), texture, source, scaledWidth, scaledHeight, false)) {
            readFrom = m_scaled.Get();
            m_scaled->GetDesc(&desc);
            layout = Layout::BGRA8;
        }
    }

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
    for (UINT y = 0; y < height; ++y) {
        ConvertRow(layout, static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch,
                   bgra.data() + size_t(y) * width * 4, width);
    }
    context->Unmap(m_staging.Get(), 0);
    return true;
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
