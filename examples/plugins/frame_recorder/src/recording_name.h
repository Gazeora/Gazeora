// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <filesystem>
#include <string>

namespace frame_recorder {

// Names a new recording after the time it starts, with the codec's id as its extension:
// "Gazeora 2026-11-01 14-03-22.h264". A name that's taken gets " (2)", " (3)" and so on.
class RecordingName {
public:
    static std::filesystem::path In(const std::filesystem::path& folder,
                                    const std::string& codecId);
};

}  // namespace frame_recorder
