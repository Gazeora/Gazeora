// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>

#include "gazeora/plugin_api/gazeora_plugin.h"

namespace frame_recorder {

// Where a file's first frame starts: after the access unit delimiters Gazeora's frames begin
// with, as VLC 3 doesn't recognize a raw stream that starts with one. Later delimiters stay.
class FileStart {
public:
    // The frame's bytes from its first NAL unit that isn't a delimiter, by the frame's codec.
    static GazeoraBytesV2 Of(const GazeoraFrameV2& frame);

private:
    static size_t StartCodeSize(const uint8_t* data, size_t size);
    static size_t NextStartCode(const uint8_t* data, size_t size, size_t from);
};

}  // namespace frame_recorder
