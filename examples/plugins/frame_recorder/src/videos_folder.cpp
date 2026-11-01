// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "videos_folder.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include "utf8_path.h"

namespace frame_recorder {

namespace {

constexpr wchar_t kSubfolder[] = L"Gazeora";

}  // namespace

std::string VideosFolder::Gazeora() {
    PWSTR videos = nullptr;
    std::string folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &videos))) {
        folder = Utf8Path::Of(std::filesystem::path(videos) / kSubfolder);
    }
    CoTaskMemFree(videos);  // even when it failed
    return folder;
}

}  // namespace frame_recorder
