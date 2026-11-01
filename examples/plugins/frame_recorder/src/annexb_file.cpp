// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "annexb_file.h"

namespace frame_recorder {

bool AnnexBFile::Open(const std::filesystem::path& path) {
    Close();
    m_stream.open(path, std::ios::binary | std::ios::trunc);
    return m_stream.is_open();
}

bool AnnexBFile::Write(const uint8_t* data, size_t size) {
    m_stream.write(reinterpret_cast<const char*>(data), std::streamsize(size));
    return bool(m_stream);
}

void AnnexBFile::Close() {
    if (m_stream.is_open()) {
        m_stream.close();
    }
    m_stream.clear();
}

bool AnnexBFile::IsOpen() const { return m_stream.is_open(); }

}  // namespace frame_recorder
