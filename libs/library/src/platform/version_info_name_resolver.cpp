#include "gazeora/library/platform/version_info_name_resolver.h"

#include <windows.h>

#include <cwchar>
#include <vector>

#include "util/path_text.h"

namespace gazeora {

namespace {

constexpr size_t kKeyLength = 64;
constexpr wchar_t kWhitespace[] = L" \t";

struct Translation {
    WORD language;
    WORD codePage;
};

// Some publishers leave whitespace around the description.
std::wstring Trimmed(const std::wstring& text) {
    size_t first = text.find_first_not_of(kWhitespace);
    size_t last = text.find_last_not_of(kWhitespace);
    return first == std::wstring::npos ? std::wstring() : text.substr(first, last - first + 1);
}

}  // namespace

std::wstring VersionInfoNameResolver::NameFor(const std::wstring& exePath) const {
    std::wstring description = FileDescription(exePath);
    return description.empty() ? PathText::Stem(exePath) : description;
}

std::wstring VersionInfoNameResolver::FileDescription(const std::wstring& exePath) const {
    DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), nullptr);
    std::vector<BYTE> data(size);
    if (size == 0 || !GetFileVersionInfoW(exePath.c_str(), 0, size, data.data())) {
        return {};
    }
    Translation* translations = nullptr;
    UINT bytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translations), &bytes) ||
        bytes < sizeof(Translation)) {
        return {};
    }
    wchar_t key[kKeyLength];
    swprintf(key, kKeyLength, L"\\StringFileInfo\\%04x%04x\\FileDescription",
             translations[0].language, translations[0].codePage);
    wchar_t* value = nullptr;
    UINT chars = 0;
    if (!VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &chars) || chars <= 1) {
        return {};
    }
    return Trimmed(std::wstring(value, chars - 1));
}

}  // namespace gazeora
