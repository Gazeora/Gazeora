#pragma once

#include <string>

namespace gazeora {

// Splitting Windows paths as text.
class PathText final {
public:
    PathText() = delete;

    // "C:\Games\Foo\foo_game.exe" -> "foo_game".
    static std::wstring Stem(const std::wstring& path) {
        size_t slash = path.find_last_of(L"\\/");
        std::wstring file = slash == std::wstring::npos ? path : path.substr(slash + 1);
        size_t dot = file.find_last_of(L'.');
        return dot == std::wstring::npos ? file : file.substr(0, dot);
    }
};

}  // namespace gazeora
