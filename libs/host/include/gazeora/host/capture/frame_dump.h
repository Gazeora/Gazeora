#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

#include "gazeora/host/capture/vr_layout.h"

namespace gazeora {

// Reads textures back from the GPU as tightly packed BGRA8 (alpha forced to 255).
// Supports 8-bit BGRA/RGBA and 10-bit RGB formats. Keeps its GPU resources between
// calls, so reading every frame of a stream doesn't reallocate them.
class TextureReader {
public:
    // Frames larger than this are scaled down on the GPU (keeping the aspect ratio) before
    // they're read back, which is far cheaper than reading and scaling them on the CPU.
    // 0 x 0, the default, reads at full size.
    void SetMaxSize(UINT maxWidth, UINT maxHeight) {
        m_maxWidth = maxWidth;
        m_maxHeight = maxHeight;
    }
    // When enabled, frames come out side by side and lens-corrected for a VR viewer, at
    // the layout's screen shape within the max size (1920x1080 if none is set).
    void SetVrLayout(const VrLayout& vr) { m_vr = vr; }

    bool Read(ID3D11Device* device, ID3D11Texture2D* texture, UINT& width, UINT& height,
              std::vector<uint8_t>& bgra, std::wstring& error);

private:
    // Draws `texture` into m_scaled at width x height, flat or for VR. False if the GPU
    // path is unavailable.
    bool Render(ID3D11DeviceContext* context, ID3D11Texture2D* texture,
                const D3D11_TEXTURE2D_DESC& source, UINT width, UINT height, bool vr);

    UINT m_maxWidth = 0;
    UINT m_maxHeight = 0;
    VrLayout m_vr;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_staging;
    D3D11_TEXTURE2D_DESC m_stagingDesc = {};

    // The GPU pass, created on first use for m_device.
    bool m_shaderFailed = false;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constants;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_scaled;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_scaledView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_source;  // the texture m_sourceView is for
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sourceView;
};

// Reads `texture` back from the GPU and writes it as a 32-bit BMP.
bool SaveTextureAsBmp(ID3D11Device* device, ID3D11Texture2D* texture, const std::wstring& path,
                      std::wstring& error);

}  // namespace gazeora
