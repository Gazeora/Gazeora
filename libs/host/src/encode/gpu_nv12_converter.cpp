// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/encode/gpu_nv12_converter.h"

#include <d3d10.h>
#include <d3dcompiler.h>

#include <string>

using Microsoft::WRL::ComPtr;

namespace gazeora {

namespace {

// The source's pixels back to the bytes the CPU path reads, then BgraToNv12's integer
// arithmetic on them, so both paths give the same NV12. An sRGB view hands out linear
// light, encoded back here; 10-bit channels keep their top 8 bits.
const char kShader[] = R"(
cbuffer Constants : register(b0) { uint encoding; uint3 unused; };
Texture2D source : register(t0);
struct Vertex { float4 position : SV_Position; };
Vertex vs(uint id : SV_VertexID) {
    Vertex v;
    float2 uv = float2((id << 1) & 2, id & 2);
    v.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return v;
}
int3 Bytes(int2 at) {
    float3 c = saturate(source.Load(int3(at, 0)).rgb);
    if (encoding == 1) c = c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055;
    if (encoding == 2) return int3(uint3(c * 1023 + 0.5) >> 2);
    return int3(c * 255 + 0.5);
}
float luma(Vertex v) : SV_Target {
    int3 p = Bytes(int2(v.position.xy));
    return (16 + ((47 * p.r + 157 * p.g + 16 * p.b + 128) >> 8)) / 255.0;
}
float2 chroma(Vertex v) : SV_Target {
    int2 at = int2(v.position.xy) * 2;
    int3 s = Bytes(at) + Bytes(at + int2(1, 0)) + Bytes(at + int2(0, 1)) + Bytes(at + int2(1, 1));
    int u = 128 + ((-26 * s.r - 87 * s.g + 112 * s.b + 512) >> 10);
    int w = 128 + ((112 * s.r - 102 * s.g - 10 * s.b + 512) >> 10);
    return float2(u, w) / 255.0;
}
)";

constexpr UINT kConstantsSize = 16;

}  // namespace

ComPtr<ID3D11Texture2D> GpuNv12Converter::Convert(ID3D11Device* device, ID3D11Texture2D* source,
                                                  UINT& width, UINT& height, std::wstring& error) {
    if (!device || !source) {
        error = L"no frame to convert";
        return nullptr;
    }
    D3D11_TEXTURE2D_DESC desc;
    source->GetDesc(&desc);
    Encoding encoding;
    if (!EncodingFor(desc.Format, encoding)) {
        error = L"GPU conversion does not support DXGI format " + std::to_wstring(desc.Format);
        return nullptr;
    }
    ID3D11ShaderResourceView* view = Prepare(device, error) ? SourceView(source) : nullptr;
    width = desc.Width & ~1u;
    height = desc.Height & ~1u;
    const Nv12TexturePool::Slot* slot =
        view && width && height ? m_pool.Next(device, width, height) : nullptr;
    if (!slot) {
        if (error.empty()) error = L"the frame can't be converted on the GPU";
        return nullptr;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    UINT constants[kConstantsSize / sizeof(UINT)] = {UINT(encoding)};
    context->UpdateSubresource(m_constants.Get(), 0, nullptr, constants, 0, 0);
    context->PSSetShaderResources(0, 1, &view);
    Draw(context.Get(), m_lumaShader.Get(), slot->luma.Get(), width, height);
    Draw(context.Get(), m_chromaShader.Get(), slot->chroma.Get(), width / 2, height / 2);
    ID3D11ShaderResourceView* none = nullptr;
    context->PSSetShaderResources(0, 1, &none);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    // Submitted now: the encoder picks the texture up from another thread.
    context->Flush();
    return slot->texture;
}

bool GpuNv12Converter::EncodingFor(DXGI_FORMAT format, Encoding& encoding) {
    switch (format) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            encoding = Encoding::Unorm8;
            return true;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            encoding = Encoding::Srgb8;
            return true;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            encoding = Encoding::Unorm10;
            return true;
        default:
            return false;
    }
}

bool GpuNv12Converter::Prepare(ID3D11Device* device, std::wstring& error) {
    if (m_device.Get() != device) {
        *this = GpuNv12Converter();
        m_device = device;
        // The frames are encoded from another thread on the same device.
        ComPtr<ID3D10Multithread> multithread;
        m_failed = !Nv12TexturePool::Supported(device) ||
                   FAILED(device->QueryInterface(IID_PPV_ARGS(&multithread)));
        if (!m_failed) multithread->SetMultithreadProtected(TRUE);
    }
    if (m_failed || m_lumaShader) {
        if (m_failed) error = L"this GPU can't draw NV12 frames";
        return !m_failed;
    }
    auto compile = [&](const char* entry, const char* target, ComPtr<ID3DBlob>& blob) {
        return SUCCEEDED(D3DCompile(kShader, sizeof(kShader) - 1, nullptr, nullptr, nullptr, entry,
                                    target, 0, 0, &blob, nullptr));
    };
    ComPtr<ID3DBlob> vs, luma, chroma;
    D3D11_BUFFER_DESC constants = {};
    constants.ByteWidth = kConstantsSize;
    constants.Usage = D3D11_USAGE_DEFAULT;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    m_failed = !compile("vs", "vs_4_0", vs) || !compile("luma", "ps_4_0", luma) ||
               !compile("chroma", "ps_4_0", chroma) ||
               FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(),
                                                 nullptr, &m_vertexShader)) ||
               FAILED(device->CreatePixelShader(luma->GetBufferPointer(), luma->GetBufferSize(),
                                                nullptr, &m_lumaShader)) ||
               FAILED(device->CreatePixelShader(chroma->GetBufferPointer(), chroma->GetBufferSize(),
                                                nullptr, &m_chromaShader)) ||
               FAILED(device->CreateBuffer(&constants, nullptr, &m_constants));
    if (m_failed) {
        m_lumaShader.Reset();
        error = L"the NV12 shaders can't run on this GPU";
    }
    return !m_failed;
}

ID3D11ShaderResourceView* GpuNv12Converter::SourceView(ID3D11Texture2D* source) {
    if (m_source.Get() != source) {
        m_source.Reset();
        m_sourceView.Reset();
        D3D11_TEXTURE2D_DESC desc;
        source->GetDesc(&desc);
        if (!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) ||
            FAILED(m_device->CreateShaderResourceView(source, nullptr, &m_sourceView))) {
            return nullptr;
        }
        m_source = source;
    }
    return m_sourceView.Get();
}

void GpuNv12Converter::Draw(ID3D11DeviceContext* context, ID3D11PixelShader* shader,
                            ID3D11RenderTargetView* target, UINT width, UINT height) {
    D3D11_VIEWPORT viewport = {0, 0, float(width), float(height), 0, 1};
    ID3D11Buffer* constants = m_constants.Get();
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(shader, nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &constants);
    context->RSSetState(nullptr);
    context->RSSetViewports(1, &viewport);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->OMSetRenderTargets(1, &target, nullptr);
    context->Draw(3, 0);
}

}  // namespace gazeora
