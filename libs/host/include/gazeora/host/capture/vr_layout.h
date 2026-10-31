// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <algorithm>
#include <cstdint>

namespace gazeora {

// How to lay the stream out for a phone in a VR viewer (Cardboard style): the picture is
// shown once per eye, side by side, and pre-distorted so the viewer's lenses straighten it
// out again.
//
// The Gazeora app on the phone does that itself as it shows each frame (lensOnPhone): the PC
// sends the picture flat, every pixel of the stream for the game, and the phone draws it into
// both eyes at its own screen's shape. The lens settings travel with every frame (an SEI
// message, stream_view_info.h), so changes show at once. For other players the PC does it:
// frames then go out at the screen's shape, already split and warped.
struct VrLayout {
    bool enabled = false;
    bool lensOnPhone = true;
    // Phone screen width / height in landscape, when the PC does the lens correction. The
    // stream gets the same shape, so the player fills the screen and the lens centers land
    // where they should.
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

    // 3D from the game's depth buffer (D3D11 and D3D12 games): each eye gets its own view of
    // the scene, the picture shifted sideways by each pixel's depth. Frames without depth
    // (the idle picture, other APIs) stay 2D.
    bool stereo = false;
    // The subject in the middle of the view sits at the screen. How deep the 3D is: how much
    // the two views differ for things far away, as a share of an eye's width. About a degree
    // in a Cardboard-style viewer at the default; more strains the eyes.
    float stereoDepth = 0.012f;
    // How far things nearer than the subject may come out of the screen, as a share of
    // stereoDepth.
    float stereoPopOut = 0.5f;

    bool operator==(const VrLayout& other) const {
        return enabled == other.enabled && lensOnPhone == other.lensOnPhone &&
               screenAspect == other.screenAspect && k1 == other.k1 && k2 == other.k2 &&
               imageScale == other.imageScale && lensSeparation == other.lensSeparation &&
               chromaCorrection == other.chromaCorrection && stereo == other.stereo &&
               stereoDepth == other.stereoDepth && stereoPopOut == other.stereoPopOut;
    }
    bool operator!=(const VrLayout& other) const { return !(*this == other); }

    // Whether the PC splits and warps the frames itself.
    bool WarpOnPc() const { return enabled && !lensOnPhone; }

    // The size of the side-by-side frame the PC warps: the screen's shape within
    // maxWidth x maxHeight, in even pixels for the encoder.
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
