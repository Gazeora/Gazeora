// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/host/stream/stream_overlay.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>

#include "gazeora/host/stream/camera_overlay_line.h"

namespace gazeora {

namespace {

constexpr wchar_t kSeparator[] = L"  ·  ";  // a middle dot between details on a line
// In VR layout, how far into each eye the panel sits: the lens warp bends the picture most
// near the edges, and the corners are black.
constexpr float kVrInsetX = 0.2f;
constexpr float kVrInsetY = 0.24f;

std::wstring Widen(const std::string& text) { return std::wstring(text.begin(), text.end()); }

// Joins the parts that aren't empty.
std::wstring Line(std::initializer_list<std::wstring> parts) {
    std::wstring line;
    for (const std::wstring& part : parts) {
        if (!part.empty()) {
            line += (line.empty() ? L"" : kSeparator) + part;
        }
    }
    return line;
}

}  // namespace

bool OverlaySettings::operator==(const OverlaySettings& other) const {
    return enabled == other.enabled && corner == other.corner && textSize == other.textSize &&
           opacity == other.opacity && protocol == other.protocol && viewers == other.viewers &&
           codec == other.codec && resolution == other.resolution && frameRate == other.frameRate &&
           bitrate == other.bitrate && loss == other.loss && encoder == other.encoder &&
           clock == other.clock && head == other.head && camera == other.camera &&
           text == other.text;
}

namespace {

// The head's line, while head tracking is on.
std::wstring HeadLine(const OverlayStats& stats) {
    if (stats.headStatus.empty()) {
        return {};
    }
    if (!stats.head.fresh) {
        return L"Head: nothing from the phone";
    }
    wchar_t buffer[64];
    swprintf(buffer, 64, L"Head: yaw %.0f°, pitch %.0f°", double(stats.head.yaw),
             double(stats.head.pitch));
    wchar_t rate[32];
    swprintf(rate, 32, L"%d/s", stats.head.perSecond);
    return Line({buffer, rate, stats.headStatus});
}

}  // namespace

std::vector<std::wstring> OverlayLines(const OverlaySettings& settings, const OverlayStats& stats) {
    wchar_t buffer[64];
    std::wstring viewers;
    if (settings.viewers) {
        swprintf(buffer, 64, L"%d %ls", stats.viewers, stats.viewers == 1 ? L"viewer" : L"viewers");
        viewers = buffer;
    }
    std::wstring resolution;
    if (settings.resolution && stats.width && stats.height) {
        swprintf(buffer, 64, L"%u×%u", stats.width, stats.height);
        resolution = buffer;
    }
    std::wstring frameRate;
    if (settings.frameRate) {
        swprintf(buffer, 64, L"%.0f fps", double(stats.frameRate));
        frameRate = buffer;
    }
    std::wstring bitrate;
    if (settings.bitrate) {
        swprintf(buffer, 64, L"%.1f Mbit/s", stats.bitrate / 1e6);
        bitrate = buffer;
    }
    std::wstring loss;
    if (settings.loss) {
        swprintf(buffer, 64, L"%.1f%% lost", double(stats.loss) * 100.0);
        loss = buffer;
    }
    std::wstring clock;
    if (settings.clock) {
        swprintf(buffer, 64, L"%02u:%02u:%02u", stats.time.wHour, stats.time.wMinute,
                 stats.time.wSecond);
        clock = buffer;
    }

    std::vector<std::wstring> lines;
    for (const std::wstring& line :
         {Line({settings.protocol ? Widen(stats.protocol) : L"", viewers}),
          Line({settings.codec ? Widen(stats.codec) : L"", resolution, frameRate}),
          Line({bitrate, loss}), settings.encoder ? stats.encoder : std::wstring(), clock,
          settings.head ? HeadLine(stats) : std::wstring(),
          settings.camera ? CameraOverlayLine::From(stats.camera) : std::wstring(), settings.text}) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

void OverlayRenderer::Draw(const std::vector<std::wstring>& lines, const OverlaySettings& settings,
                           OverlayLayout layout, Picture& picture) {
    if (lines.empty() || !picture.Complete() || picture.width < 16 || picture.height < 16) {
        return;
    }
    int fontPixels = std::max(10, int(picture.height / 40.0f * settings.textSize / 100.0f));
    int opacity = std::clamp(settings.opacity, 0, 100);
    if (lines != m_lines || fontPixels != m_fontPixels || opacity != m_opacity) {
        Render(lines, fontPixels, opacity);
        m_lines = lines;
        m_fontPixels = fontPixels;
        m_opacity = opacity;
    }
    if (m_panel.empty()) {
        return;
    }
    int margin = fontPixels;
    uint32_t eye = picture.width / 2;
    if (layout == OverlayLayout::Flat) {
        Blend(picture, 0, 0, picture.width, picture.height, settings.corner, margin);
        return;
    }
    if (layout == OverlayLayout::Pair) {
        // In the same place in both views: at the screen's depth.
        for (uint32_t left : {0u, eye}) {
            Blend(picture, left, 0, eye, picture.height, settings.corner, margin, 2);
        }
        return;
    }
    // The same panel in both eyes, so it's seen in one place in the headset.
    uint32_t insetX = uint32_t(eye * kVrInsetX), insetY = uint32_t(picture.height * kVrInsetY);
    for (uint32_t left : {0u, eye}) {
        Blend(picture, left + insetX, insetY, eye - 2 * insetX, picture.height - 2 * insetY,
              settings.corner, margin / 2);
    }
}

void OverlayRenderer::Render(const std::vector<std::wstring>& lines, int fontPixels, int opacity) {
    m_panel.clear();
    m_panelWidth = m_panelHeight = 0;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        return;
    }
    // Greyscale smoothing rather than ClearType: the text's coverage becomes the alpha.
    HFONT font = CreateFontW(-fontPixels, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    TEXTMETRICW metrics = {};
    GetTextMetricsW(dc, &metrics);
    const int lineHeight = metrics.tmHeight;
    const int padding = std::max(4, fontPixels / 2);
    int textWidth = 0;
    for (const std::wstring& line : lines) {
        SIZE size = {};
        GetTextExtentPoint32W(dc, line.c_str(), int(line.size()), &size);
        textWidth = std::max<int>(textWidth, size.cx);
    }
    const int width = textWidth + 2 * padding;
    const int height = lineHeight * int(lines.size()) + 2 * padding;

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap && bits) {
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        memset(bits, 0, size_t(width) * height * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        for (size_t i = 0; i < lines.size(); ++i) {
            TextOutW(dc, padding, padding + lineHeight * int(i), lines[i].c_str(),
                     int(lines[i].size()));
        }
        GdiFlush();

        // White text over the dark panel, premultiplied: what's drawn shows where the text
        // covers, the panel where it doesn't.
        const float panel = opacity / 100.0f;
        m_panel.resize(size_t(width) * height * 4);
        const uint8_t* coverage = static_cast<const uint8_t*>(bits);
        for (size_t i = 0; i < size_t(width) * height; ++i) {
            float text = coverage[i * 4 + 1] / 255.0f;  // green: greyscale, all the same
            float alpha = text + panel * (1.0f - text);
            uint8_t white = uint8_t(text * 255.0f + 0.5f);
            m_panel[i * 4 + 0] = white;
            m_panel[i * 4 + 1] = white;
            m_panel[i * 4 + 2] = white;
            m_panel[i * 4 + 3] = uint8_t(alpha * 255.0f + 0.5f);
        }
        m_panelWidth = uint32_t(width);
        m_panelHeight = uint32_t(height);
        SelectObject(dc, oldBitmap);
    }
    if (bitmap) {
        DeleteObject(bitmap);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
}

void OverlayRenderer::Blend(Picture& picture, uint32_t left, uint32_t top, uint32_t width,
                            uint32_t height, OverlaySettings::Corner corner, int margin,
                            uint32_t step) const {
    const uint32_t shownWidth = m_panelWidth / step;
    int marginX = margin / int(step);
    if (width < shownWidth + 2 * uint32_t(marginX) ||
        height < m_panelHeight + 2 * uint32_t(margin)) {
        margin = marginX = 0;  // too small for the margin: as much of the panel as fits
    }
    const bool right = corner == OverlaySettings::Corner::TopRight ||
                       corner == OverlaySettings::Corner::BottomRight;
    const bool bottom = corner == OverlaySettings::Corner::BottomLeft ||
                        corner == OverlaySettings::Corner::BottomRight;
    const uint32_t panelWidth = std::min(shownWidth, width);
    const uint32_t panelHeight = std::min(m_panelHeight, height);
    const uint32_t x = left + (right ? width - panelWidth - uint32_t(marginX) : uint32_t(marginX));
    const uint32_t y = top + (bottom ? height - panelHeight - uint32_t(margin) : uint32_t(margin));
    for (uint32_t row = 0; row < panelHeight && y + row < picture.height; ++row) {
        const uint8_t* source = &m_panel[size_t(row) * m_panelWidth * 4];
        uint8_t* target = &picture.bgra[(size_t(y + row) * picture.width + x) * 4];
        for (uint32_t column = 0; column < panelWidth && x + column < picture.width; ++column) {
            const uint32_t keep = 255 - source[3];
            for (int c = 0; c < 3; ++c) {
                target[c] = uint8_t(source[c] + (target[c] * keep + 127) / 255);
            }
            source += 4 * step;
            target += 4;
        }
    }
}

}  // namespace gazeora
