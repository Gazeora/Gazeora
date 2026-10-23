// Picks the game's main depth buffer, for 3D from depth: of the depth buffers a frame draws
// with, the one with the most draws among those with the back buffer's shape. Shadow maps,
// reflection and UI buffers have shapes of their own, or far fewer draws.
#pragma once

#include <windows.h>

#include <cstdint>
#include <unordered_map>

namespace gazeora::hook {

class DepthPicker {
public:
    struct Candidate {
        UINT width = 0;
        UINT height = 0;
        bool usable = false;  // single-sampled, in a format the copy handles
    };

    // A depth buffer drawn with this frame, identified by its address. Seeing it again
    // changes nothing.
    void See(const void* id, const Candidate& candidate);
    bool Seen(const void* id) const { return m_frame.count(id) != 0; }
    // Draws made with `id` this frame; ignored for one not seen.
    void Count(const void* id, uint32_t draws);

    // A buffer replaces the one picked only after being the better one this many frames in
    // a row, about half a second: a menu drawing with two buffers by turns mustn't swap the
    // 3D between them.
    static constexpr int kFramesToSwitch = 15;

    // At the end of a frame: the main depth buffer, nullptr if none qualifies. The one
    // picked stays until another has clearly more draws (half as many again) for
    // kFramesToSwitch frames, or stays even when it isn't drawn with for a while. Starts the
    // next frame's count.
    const void* Pick(UINT backBufferWidth, UINT backBufferHeight);

    void Clear();

private:
    struct Entry {
        Candidate candidate;
        uint64_t draws = 0;
    };

    std::unordered_map<const void*, Entry> m_frame;
    const void* m_picked = nullptr;
    const void* m_challenger = nullptr;  // the better one lately, and for how many frames
    int m_challengerFrames = 0;
};

}  // namespace gazeora::hook
