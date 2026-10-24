#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "gazeora/host/capture/depth_frame.h"
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
    // When the PC does the lens correction (VrLayout::WarpOnPc), frames come out side by
    // side and lens-corrected for a VR viewer, at the layout's screen shape within the max
    // size (1920x1080 if none is set). With 3D on and the frame's depth at hand, each eye gets
    // its own view; without the PC's warp, the two views come out side by side in the frame's
    // own shape, each squeezed to half its width.
    void SetVrLayout(const VrLayout& vr) { m_vr = vr; }

    // `depth`, the game's depth buffer for `texture`, is for 3D; without it frames are 2D.
    bool Read(ID3D11Device* device, ID3D11Texture2D* texture, UINT& width, UINT& height,
              std::vector<uint8_t>& bgra, std::wstring& error, const DepthFrame* depth = nullptr);
    // Read's GPU half: draws the frame as it'll be streamed (scaled, warped, in 3D) and
    // returns the texture holding it, the reader's own or `texture` itself when nothing had to
    // be drawn. It stays valid until the next call. Null with `error` set if it can't be.
    ID3D11Texture2D* Compose(ID3D11Device* device, ID3D11Texture2D* texture, std::wstring& error,
                             const DepthFrame* depth = nullptr);
    // Whether the last frame read came out as a pair of views side by side.
    bool Stereo() const { return m_stereo; }

private:
    // What a GPU pass draws.
    enum class Pass { Scale, Warp, WarpPair, DecodeDepth, MeanDepth, Stereo };

    bool CreateShaders();
    // Draws a full-screen pass into `target` (width x height), reading `views` from t0 on.
    void Draw(ID3D11DeviceContext* context, Pass pass, ID3D11RenderTargetView* target, UINT width,
              UINT height, std::initializer_list<ID3D11ShaderResourceView*> views);
    // The view of `texture`, for reading it in a pass. Null if it can't be read in one.
    ID3D11ShaderResourceView* SourceView(ID3D11Texture2D* texture,
                                         const D3D11_TEXTURE2D_DESC& source);
    // Makes m_scaled width x height, in the format for `source`.
    bool PrepareTarget(const D3D11_TEXTURE2D_DESC& source, UINT width, UINT height);
    // Draws the texture `view` is of into m_scaled at width x height: scaled (Pass::Scale),
    // or split and warped for VR (Pass::Warp, or Pass::WarpPair for a pair of views). False if
    // the GPU path is unavailable.
    bool Render(ID3D11DeviceContext* context, ID3D11ShaderResourceView* view,
                const D3D11_TEXTURE2D_DESC& source, UINT width, UINT height, Pass pass);
    // Draws the two views of `color`'s texture side by side, at eyeWidth x height each: into m_pair
    // to be warped, or straight into m_scaled. False if the depth can't be used.
    bool RenderStereo(ID3D11DeviceContext* context, ID3D11ShaderResourceView* color,
                      const D3D11_TEXTURE2D_DESC& source, const DepthFrame& depth, UINT eyeWidth,
                      UINT height, bool forWarp);
    // The game's depth as floats with their average (m_depth, m_mean[m_meanIndex]).
    bool PrepareDepth(ID3D11DeviceContext* context, const DepthFrame& depth);
    void SetConstants(ID3D11DeviceContext* context, float mode, float sourceAspect, float eyeAspect,
                      const DepthFrame* depth);

    UINT m_maxWidth = 0;
    UINT m_maxHeight = 0;
    VrLayout m_vr;
    bool m_stereo = false;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_staging;
    D3D11_TEXTURE2D_DESC m_stagingDesc = {};

    // The GPU passes, created on first use for m_device.
    bool m_shaderFailed = false;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;  // scale and warp
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_decodeShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_meanShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_stereoShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pointSampler;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constants;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_scaled;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_scaledView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_source;  // the texture m_sourceView is for
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sourceView;

    // 3D: the views side by side before they're warped (m_pair), the depth (as the hook sent
    // it, then as floats with a full mip chain for the average), and the scene's average depth
    // and the depth at the middle of the view over the last frames, in two 1x1 textures
    // written in turn.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pair;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pairTarget;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pairView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_rawDepth;  // the texture m_rawDepthView is for
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_rawDepthView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depth;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_depthTarget;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_mean[2];
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_meanTarget[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_meanView[2];
    int m_meanIndex = 0;       // the one holding the latest average
    bool m_meanValid = false;  // whether it holds one yet
};

// Writes a tightly packed BGRA8 picture as a 32-bit BMP.
bool SaveBgraAsBmp(const std::wstring& path, UINT width, UINT height,
                   const std::vector<uint8_t>& bgra, std::wstring& error);

// Reads `texture` back from the GPU and writes it as a 32-bit BMP.
bool SaveTextureAsBmp(ID3D11Device* device, ID3D11Texture2D* texture, const std::wstring& path,
                      std::wstring& error);

}  // namespace gazeora
