#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

namespace gazeora {

// Reads an NV12 texture back into memory, laid out as BgraToNv12 writes it: for encoders
// that take no textures (software ones, or a GPU encoder on another adapter). Still cheaper
// than reading the frame back as BGRA, at less than half the bytes and no CPU conversion.
// Keeps its staging texture between calls.
class Nv12Readback {
public:
    // The top-left width x height (even) of `nv12` into `out`.
    bool Read(ID3D11Texture2D* nv12, uint32_t width, uint32_t height, std::vector<uint8_t>& out,
              std::wstring& error);

private:
    bool PrepareStaging(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source);

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_staging;
    UINT m_width = 0;
    UINT m_height = 0;
};

}  // namespace gazeora
