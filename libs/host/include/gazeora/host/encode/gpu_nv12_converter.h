#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <string>

#include "gazeora/host/encode/nv12_texture_pool.h"

namespace gazeora {

// Converts a frame to NV12 on the GPU, so it can go to a GPU encoder without leaving the
// GPU: the bytes BgraToNv12 would give for the frame read back (BT.709, limited range, each
// chroma sample the mean of a 2x2 block). Takes 8-bit BGRA/RGBA (sRGB or not) and 10-bit
// RGB, as TextureReader does. Use from the thread that draws the frames.
class GpuNv12Converter {
public:
    // `source` as NV12, at its size rounded down to even (width x height), in a texture from
    // the pool. Null with `error` set if this device or format can't be converted here.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Convert(ID3D11Device* device,
                                                    ID3D11Texture2D* source, UINT& width,
                                                    UINT& height, std::wstring& error);

private:
    // How the source's channels are stored, as the shader reads them back to bytes.
    enum class Encoding : UINT { Unorm8 = 0, Srgb8 = 1, Unorm10 = 2 };

    static bool EncodingFor(DXGI_FORMAT format, Encoding& encoding);
    // Shaders and constants for `device`, made on first use.
    bool Prepare(ID3D11Device* device, std::wstring& error);
    ID3D11ShaderResourceView* SourceView(ID3D11Texture2D* source);
    void Draw(ID3D11DeviceContext* context, ID3D11PixelShader* shader,
              ID3D11RenderTargetView* target, UINT width, UINT height);

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_lumaShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_chromaShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constants;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_source;  // the texture m_sourceView is for
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sourceView;
    bool m_failed = false;  // for m_device
    Nv12TexturePool m_pool;
};

}  // namespace gazeora
