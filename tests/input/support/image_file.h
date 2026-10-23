#pragma once

#include <string>

#include "gazeora/input/models/camera_frame.h"

namespace gazeora::input::testing {

// Pictures on disk as camera frames, decoded by WIC.
class ImageFile {
public:
    // Throws InputError if the file can't be decoded.
    static CameraFrame Load(const std::wstring& path);
    // A file in tests/input/data.
    static std::wstring TestData(const wchar_t* name);
};

}  // namespace gazeora::input::testing
