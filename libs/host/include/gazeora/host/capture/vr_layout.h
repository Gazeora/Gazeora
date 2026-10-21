#pragma once

#include <algorithm>
#include <cstdint>

namespace gazeora {

// How to lay the stream out for a phone in a VR viewer (Cardboard style): the picture is
// shown once per eye, side by side, and pre-distorted so the viewer's lenses straighten it
// out again.
struct VrLayout {
    bool enabled = false;
    // Phone screen width / height in landscape. The stream gets the same shape, so the
    // player fills the screen and the lens centers land where they should.
    float screenAspect = 2.0f;
    // Lens distortion, as r' = r * (1 + k1 r^2 + k2 r^4) with r = 1 at the top of an eye's
    // view. The viewer's lenses pincushion; this barrels the picture by the same amount.
    float k1 = 0.34f;
    float k2 = 0.55f;
    // How tall the game picture is in each eye, relative to the eye's view.
    float imageScale = 0.7f;
    // Distance between the lens centers, as a fraction of the screen width. 0.5 puts each
    // center in the middle of its half.
    float lensSeparation = 0.5f;
    // The lenses magnify blue a little more than red, which fringes edges with color. This
    // draws blue that much smaller and red that much larger to cancel it.
    float chromaCorrection = 0.0f;

    bool operator==(const VrLayout& other) const {
        return enabled == other.enabled && screenAspect == other.screenAspect && k1 == other.k1 &&
               k2 == other.k2 && imageScale == other.imageScale &&
               lensSeparation == other.lensSeparation && chromaCorrection == other.chromaCorrection;
    }
    bool operator!=(const VrLayout& other) const { return !(*this == other); }

    // The size of the side-by-side frame: the screen's shape within maxWidth x maxHeight,
    // in even pixels for the JPEG encoder.
    void FrameSize(uint32_t maxWidth, uint32_t maxHeight, uint32_t& width, uint32_t& height) const {
        float aspect = std::max(screenAspect, 0.1f);
        width = maxWidth;
        height = uint32_t(float(maxWidth) / aspect + 0.5f);
        if (height > maxHeight) {
            height = maxHeight;
            width = uint32_t(float(maxHeight) * aspect + 0.5f);
        }
        width = std::max(2u, width & ~1u);
        height = std::max(2u, height & ~1u);
    }
};

}  // namespace gazeora
