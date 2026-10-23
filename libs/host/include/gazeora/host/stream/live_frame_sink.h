#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace gazeora {

// Where the game's frames go to be streamed: FrameStreamServer, or a test double.
class LiveFrameSink {
public:
    virtual ~LiveFrameSink() = default;

    // False while nobody watches; frames pushed then are wasted work.
    virtual bool WantsFrames() const = 0;
    // Whether frames may come as NV12 textures (PushTexture) rather than BGRA8 in memory.
    virtual bool WantsTextures() = 0;

    // One tightly packed BGRA8 frame; `bgra` is swapped with an older buffer for reuse.
    virtual void PushFrame(uint32_t width, uint32_t height, std::vector<uint8_t>& bgra,
                           bool stereo = false) = 0;
    // One NV12 frame on the GPU (GpuNv12Converter), width x height (even).
    virtual void PushTexture(uint32_t width, uint32_t height,
                             Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12,
                             bool stereo = false) = 0;
    // The last frame pushed, again, as the game hasn't drawn a new one.
    virtual void RepeatFrame() = 0;
};

}  // namespace gazeora
