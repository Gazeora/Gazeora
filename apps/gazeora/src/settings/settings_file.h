#pragma once

#include <string>

namespace gazeora::app {

// Where the launcher keeps its files: next to the library file, in %APPDATA%\Gazeora.
class SettingsFile {
public:
    static std::wstring NextToLibrary(const wchar_t* name);
};

}  // namespace gazeora::app
