// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace frame_recorder {

// One recording on disk: each frame's NAL units, start codes included, one after another: a raw
// H.264 or H.265 stream (Annex B).
class AnnexBFile {
public:
    bool Open(const std::filesystem::path& path);
    bool Write(const uint8_t* data, size_t size);
    void Close();
    bool IsOpen() const;

private:
    std::ofstream m_stream;
};

}  // namespace frame_recorder
