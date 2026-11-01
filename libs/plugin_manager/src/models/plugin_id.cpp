// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_manager/models/plugin_id.h"

#include <cwctype>

namespace gazeora {

namespace {

constexpr size_t kMaxIdLength = 64;
constexpr size_t kMaxFileNameLength = 128;
constexpr wchar_t kDllExtension[] = L".dll";
constexpr size_t kDllExtensionLength = 4;
constexpr wchar_t kUnsafeNameChars[] = L"\\/:*?\"<>|";
constexpr wchar_t kParentFolder[] = L"..";
constexpr wchar_t kReplacement = L'-';

bool IsIdCharacter(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-' || c == L'_';
}

bool IsAlphanumeric(wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'); }

std::wstring Lower(std::wstring text) {
    for (wchar_t& c : text) {
        c = wchar_t(towlower(c));
    }
    return text;
}

std::wstring FileName(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring WithoutDll(const std::wstring& name) {
    bool dll = name.size() > kDllExtensionLength &&
               Lower(name.substr(name.size() - kDllExtensionLength)) == kDllExtension;
    return dll ? name.substr(0, name.size() - kDllExtensionLength) : name;
}

}  // namespace

bool PluginId::IsValid(const std::wstring& id) {
    if (id.empty() || id.size() > kMaxIdLength) {
        return false;
    }
    for (wchar_t c : id) {
        if (!IsIdCharacter(c)) {
            return false;
        }
    }
    return true;
}

std::wstring PluginId::OfPath(const std::wstring& path) {
    return Lower(WithoutDll(FileName(path)));
}

std::wstring PluginId::ForPickedFile(const std::wstring& path) {
    std::wstring id = OfPath(path);
    bool usable = false;
    for (wchar_t& c : id) {
        c = IsIdCharacter(c) ? c : kReplacement;
        usable = usable || IsAlphanumeric(c);
    }
    if (id.size() > kMaxIdLength) {
        id.resize(kMaxIdLength);
    }
    return usable ? id : std::wstring();
}

bool PluginId::IsSafeDllName(const std::wstring& fileName) {
    return fileName.size() > kDllExtensionLength && fileName.size() <= kMaxFileNameLength &&
           fileName.find_first_of(kUnsafeNameChars) == std::wstring::npos &&
           fileName.find(kParentFolder) == std::wstring::npos &&
           WithoutDll(fileName).size() + kDllExtensionLength == fileName.size();
}

}  // namespace gazeora
