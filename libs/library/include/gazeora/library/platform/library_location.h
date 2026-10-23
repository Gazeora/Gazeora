#pragma once

#include <string>

namespace gazeora {

// Where library.json lives.
class LibraryLocation final {
public:
    LibraryLocation() = delete;

    // %APPDATA%\Gazeora\library.json, or %GAZEORA_LIBRARY% if set.
    static std::wstring DefaultPath();
};

}  // namespace gazeora
