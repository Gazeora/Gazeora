#pragma once

#include <windows.h>

#include <string>

namespace gazeora {

// Converts between UTF-16 (Windows) and UTF-8 (files, JSON, the UI). Stateless.
class Utf8 final {
public:
    Utf8() = delete;

    static std::string FromWide(const std::wstring& text) {
        if (text.empty()) {
            return {};
        }
        int bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0,
                                        nullptr, nullptr);
        std::string result(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), &result[0], bytes, nullptr,
                            nullptr);
        return result;
    }

    static std::wstring ToWide(const std::string& text) {
        if (text.empty()) {
            return {};
        }
        int chars = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
        std::wstring result(chars, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), &result[0], chars);
        return result;
    }
};

}  // namespace gazeora
