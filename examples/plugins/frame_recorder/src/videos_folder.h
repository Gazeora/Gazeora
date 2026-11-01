// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace frame_recorder {

// Where recordings go until the player picks a folder.
class VideosFolder {
public:
    // "<the user's Videos folder>\Gazeora" (UTF-8), or "" if Windows doesn't say where that is.
    static std::string Gazeora();
};

}  // namespace frame_recorder
