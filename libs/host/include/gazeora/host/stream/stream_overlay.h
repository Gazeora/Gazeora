// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "gazeora/host/stream/frame_mailbox.h"
#include "gazeora/host/stream/head_pose_monitor.h"

namespace gazeora {

// Streaming details drawn into the frames themselves, so everyone watching sees them, in any
// player: which details, where and how big.
struct OverlaySettings {
    enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

    bool enabled = false;
    Corner corner = Corner::TopLeft;
    int textSize = 100;  // percent of the usual size, a 40th of the frame's height
    int opacity = 60;    // the panel behind the text, percent

    // The details.
    bool protocol = true;
    bool viewers = true;
    bool codec = true;
    bool resolution = true;
    bool frameRate = true;
    bool bitrate = true;
    bool loss = true;
    bool encoder = false;
    bool clock = false;  // this PC's time, to the second
    // Where the phone says the head points, and what head tracking does with it, while it's on.
    bool head = true;
    std::wstring text;  // a line of the user's own; none if empty

    bool operator==(const OverlaySettings& other) const;
    bool operator!=(const OverlaySettings& other) const { return !(*this == other); }
};

// How the stream stands, for the overlay.
struct OverlayStats {
    std::string protocol;  // e.g. "RTSP over TCP"
    int viewers = 0;
    std::string codec;  // e.g. "H.265"
    uint32_t width = 0;
    uint32_t height = 0;
    float frameRate = 0;
    uint32_t bitrate = 0;  // bits/s
    float loss = 0;        // 0..1
    std::wstring encoder;
    SYSTEMTIME time = {};
    HeadReading head;
    // What head tracking does, e.g. "turning the game"; empty while it's off.
    std::wstring headStatus;
};

// The overlay's lines of text, from the details `settings` picks; empty if none.
std::vector<std::wstring> OverlayLines(const OverlaySettings& settings, const OverlayStats& stats);

// How a picture is laid out, for where the overlay goes.
enum class OverlayLayout {
    Flat,    // one picture
    VrEyes,  // split and warped for VR by the PC
    Pair,    // two views side by side, each squeezed to half the width (3D)
};

// Draws the overlay's lines onto pictures: white text on a translucent dark panel, in a
// corner. The panel is made with GDI and kept until the text or its look changes. In VR
// layout it's drawn into each eye's half, away from the lens edges, where the warp bends the
// picture most; in a pair of views, into each, squeezed like them. Use from one thread.
class OverlayRenderer {
public:
    OverlayRenderer() = default;
    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    void Draw(const std::vector<std::wstring>& lines, const OverlaySettings& settings,
              OverlayLayout layout, Picture& picture);

private:
    // The panel, premultiplied BGRA, for `lines` at `fontPixels`.
    void Render(const std::vector<std::wstring>& lines, int fontPixels, int opacity);
    // Blends the panel onto the picture in the corner of the region, taking every `step`th
    // column of it (2 squeezes it to half its width).
    void Blend(Picture& picture, uint32_t left, uint32_t top, uint32_t width, uint32_t height,
               OverlaySettings::Corner corner, int margin, uint32_t step = 1) const;

    std::vector<std::wstring> m_lines;
    int m_fontPixels = 0;
    int m_opacity = -1;
    std::vector<uint8_t> m_panel;
    uint32_t m_panelWidth = 0;
    uint32_t m_panelHeight = 0;
};

}  // namespace gazeora
