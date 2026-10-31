// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Per-pixel work on the CPU: the hook's row conversion for back buffers read back on the CPU
// (D3D8, D3D9, OpenGL fallback), and the BGRA8 to NV12 conversion of every streamed frame.
#include <gtest/gtest.h>

#include <vector>

#include "bench_util.h"
#include "gazeora/hook/core/pixel_convert.h"
#include "gazeora/host/encode/color_convert.h"

namespace gazeora::bench {
namespace {

TEST(Convert, HookRowsPerLayout) {
    using hook::PixelLayout;
    struct Layout {
        PixelLayout layout;
        const char* name;
    };
    const Layout layouts[] = {
        {PixelLayout::BGRA8, "BGRA8"},     {PixelLayout::BGRX8, "BGRX8"},
        {PixelLayout::RGBA8, "RGBA8"},     {PixelLayout::RGBX8, "RGBX8"},
        {PixelLayout::RGB10A2, "RGB10A2"}, {PixelLayout::BGR10A2, "BGR10A2"},
        {PixelLayout::B5G6R5, "B5G6R5"},   {PixelLayout::B5G5R5X1, "B5G5R5X1"},
    };
    const Size size = k1080p;
    PrintHeader("hook::ConvertRow, whole 1080p frame");
    for (const Layout& l : layouts) {
        uint32_t bpp = hook::SourceBytesPerPixel(l.layout);
        std::vector<uint8_t> src(size_t(size.width) * size.height * bpp);
        std::vector<uint8_t> dst(size_t(size.width) * size.height * 4);
        FillNoise(src);
        Stats s = Measure(30, [&] {
            for (uint32_t y = 0; y < size.height; ++y) {
                hook::ConvertRow(l.layout, src.data() + size_t(y) * size.width * bpp,
                                 dst.data() + size_t(y) * size.width * 4, size.width);
            }
            Consume(dst.data());
        });
        Print(l.name, s,
              std::string(hook::NeedsConversion(l.layout) ? "" : "(copy) ") +
                  Throughput(double(src.size()), s.median));
    }
}

TEST(Convert, BgraToNv12) {
    PrintHeader("BgraToNv12 (every streamed frame)");
    for (Size size : {k720p, k1080p, k1440p}) {
        std::vector<uint8_t> bgra, nv12;
        FillScene(bgra, size.width, size.height, 0);
        // Paced at 60 fps like the stream, so the CPU time includes what threads burn while
        // waiting for the next frame. Only the conversion itself is timed.
        constexpr int kFrames = 60;
        Pacer pacer;
        std::vector<double> times;
        double cpuStart = 0, next = QpcSeconds();
        for (int i = -3; i < kFrames; ++i) {
            if (i == 0) cpuStart = CpuSeconds();
            double start = QpcSeconds();
            BgraToNv12(bgra.data(), size.width, size.width, size.height, nv12);
            Consume(nv12.data());
            if (i >= 0) times.push_back(QpcSeconds() - start);
            pacer.SleepUntil(next += 1.0 / 60);
        }
        double cpuPerFrame = (CpuSeconds() - cpuStart) / kFrames;
        Stats s = Stats::Of(times);
        Print(size.name, s,
              Throughput(double(bgra.size()), s.median) + ", CPU " + FormatSeconds(cpuPerFrame) +
                  "/frame");
    }
}

}  // namespace
}  // namespace gazeora::bench
