#include "gazeora/host/stream/stream_frame_sender.h"

#include <utility>

namespace gazeora {

void StreamFrameSender::SetVrLayout(const VrLayout& vr) {
    m_vr = vr;
    m_reader.SetVrLayout(vr);
}

bool StreamFrameSender::Send(ID3D11Device* device, ID3D11Texture2D* frame, const DepthFrame* depth,
                             std::wstring& error) {
    if (m_latestSent && m_vr == m_sentLayout) {
        m_sink.RepeatFrame();
        return true;
    }
    m_latestSent = false;
    bool sent = false;
    if (m_sink.WantsTextures() && device != m_noTextures.Get()) {
        ID3D11Texture2D* composed = m_reader.Compose(device, frame, error, depth);
        if (!composed) {
            return false;  // reading it back would fail the same way
        }
        sent = SendTexture(device, composed, error);
        if (!sent) {
            m_noTextures = device;  // this GPU can't convert: don't draw every frame twice
        }
    }
    // Without the GPU path (or the sink wanting textures), as pixels.
    if (!sent && !SendPixels(device, frame, depth, error)) {
        return false;
    }
    error.clear();
    m_latestSent = true;
    m_sentLayout = m_vr;
    return true;
}

bool StreamFrameSender::SendTexture(ID3D11Device* device, ID3D11Texture2D* composed,
                                    std::wstring& error) {
    UINT width = 0, height = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12 =
        m_converter.Convert(device, composed, width, height, error);
    if (!nv12) {
        return false;
    }
    m_stereo = m_reader.Stereo();
    m_sink.PushTexture(width, height, std::move(nv12), m_stereo);
    return true;
}

bool StreamFrameSender::SendPixels(ID3D11Device* device, ID3D11Texture2D* frame,
                                   const DepthFrame* depth, std::wstring& error) {
    UINT width = 0, height = 0;
    if (!m_reader.Read(device, frame, width, height, m_pixels, error, depth)) {
        return false;
    }
    m_stereo = m_reader.Stereo();
    m_sink.PushFrame(width, height, m_pixels, m_stereo);
    return true;
}

}  // namespace gazeora
