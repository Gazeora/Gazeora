// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <filesystem>
#include <string>

namespace frame_recorder {

// Gazeora passes paths as UTF-8; Windows opens files by UTF-16 paths.
class Utf8Path {
public:
    static std::filesystem::path From(const std::string& utf8);
    static std::string Of(const std::filesystem::path& path);
};

}  // namespace frame_recorder
