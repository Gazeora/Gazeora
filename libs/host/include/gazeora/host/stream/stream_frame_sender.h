#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

#include "gazeora/host/capture/depth_frame.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/capture/vr_layout.h"
#include "gazeora/host/encode/gpu_nv12_converter.h"
#include "gazeora/host/stream/live_frame_sink.h"

namespace gazeora {

// Facade pattern: hands the game's captured frames to the stream the cheapest way there is.
// A frame goes as an NV12 texture that never leaves the GPU when the sink takes textures and
// the GPU can convert it, read back as BGRA8 otherwise. A frame sent before goes as a repeat:
// the stream encodes the picture it already holds again, with nothing read or converted.
// Use from the capture thread.
class StreamFrameSender {
public:
    explicit StreamFrameSender(LiveFrameSink& sink) : m_sink(sink) {}

    void SetMaxSize(UINT maxWidth, UINT maxHeight) { m_reader.SetMaxSize(maxWidth, maxHeight); }
    void SetVrLayout(const VrLayout& vr);

    // The game drew a new frame into the capture's texture, whether it's sent or not.
    void FrameCaptured() { m_latestSent = false; }
    // Sends the capture's latest frame, `frame` with its `depth`: as a repeat when it went out
    // already with the same layout. False with `error` set if it couldn't be.
    bool Send(ID3D11Device* device, ID3D11Texture2D* frame, const DepthFrame* depth,
              std::wstring& error);
    // Whether the last frame sent was a pair of views side by side.
    bool Stereo() const { return m_stereo; }

private:
    bool SendTexture(ID3D11Device* device, ID3D11Texture2D* composed, std::wstring& error);
    bool SendPixels(ID3D11Device* device, ID3D11Texture2D* frame, const DepthFrame* depth,
                    std::wstring& error);

    LiveFrameSink& m_sink;
    TextureReader m_reader;
    GpuNv12Converter m_converter;
    Microsoft::WRL::ComPtr<ID3D11Device> m_noTextures;  // the device the converter failed on
    std::vector<uint8_t> m_pixels;                      // swapped with the sink's older buffers
    VrLayout m_vr;
    VrLayout m_sentLayout;      // what the latest frame went out with
    bool m_latestSent = false;  // the capture's latest frame went out
    bool m_stereo = false;
};

}  // namespace gazeora
