// D3D11 helpers shared by the tests. Everything runs on WARP so the tests don't
// depend on the machine's GPU.
#pragma once

#include <windows.h>
#include <d3d11_4.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace gazeora::test {

struct WarpDevice {
    Microsoft::WRL::ComPtr<ID3D11Device5> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context;
};

// Creates a D3D11.4 device on WARP. Members stay null on failure.
inline WarpDevice CreateWarpDevice() {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    WarpDevice warp;
    if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                    D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                    &device, nullptr, &context))) {
        device.As(&warp.device);
        context.As(&warp.context);
    }
    return warp;
}

// Creates a width x height texture filled with `pixels` (4 bytes each, tightly packed).
inline Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture(ID3D11Device* device, UINT width,
                                                             UINT height, DXGI_FORMAT format,
                                                             const std::vector<uint32_t>& pixels) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = {pixels.data(), width * 4, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    device->CreateTexture2D(&desc, &data, &texture);
    return texture;
}

// Reads a 4-byte-per-pixel texture back to the CPU, tightly packed.
inline std::vector<uint32_t> ReadTexture(ID3D11Device* device, ID3D11Texture2D* texture) {
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) {
        return {};
    }
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    context->CopyResource(staging.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return {};
    }
    std::vector<uint32_t> pixels(size_t(desc.Width) * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y) {
        memcpy(pixels.data() + size_t(y) * desc.Width,
               static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch,
               size_t(desc.Width) * 4);
    }
    context->Unmap(staging.Get(), 0);
    return pixels;
}

}  // namespace gazeora::test
