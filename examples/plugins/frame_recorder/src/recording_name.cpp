// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "recording_name.h"

#include <ctime>

namespace frame_recorder {

namespace {

constexpr char kTimeFormat[] = "Gazeora %Y-%m-%d %H-%M-%S";
constexpr size_t kMaxStemSize = 64;

std::string Stem() {
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_s(&local, &now);
    char stem[kMaxStemSize] = {};
    std::strftime(stem, sizeof(stem), kTimeFormat, &local);
    return stem;
}

}  // namespace

std::filesystem::path RecordingName::In(const std::filesystem::path& folder,
                                        const std::string& codecId) {
    // Codec ids are a-z, 0-9 and '-' (docs/plugins.md), so safe in a file name.
    const std::string stem = Stem();
    const std::string extension = "." + codecId;
    std::filesystem::path path = folder / (stem + extension);
    for (int copy = 2; std::filesystem::exists(path); ++copy) {
        path = folder / (stem + " (" + std::to_string(copy) + ")" + extension);
    }
    return path;
}

}  // namespace frame_recorder
