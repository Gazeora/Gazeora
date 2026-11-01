// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "utf8_path.h"

#include <windows.h>

namespace frame_recorder {

std::filesystem::path Utf8Path::From(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring wide(size_t(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), wide.data(), size);
    return std::filesystem::path(wide);
}

std::string Utf8Path::Of(const std::filesystem::path& path) {
    const std::wstring& wide = path.native();
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0,
                                         nullptr, nullptr);
    std::string utf8(size_t(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), utf8.data(), size, nullptr,
                        nullptr);
    return utf8;
}

}  // namespace frame_recorder
