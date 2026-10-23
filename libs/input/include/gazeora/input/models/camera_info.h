#pragma once

#include <string>

namespace gazeora::input {

// A camera connected to this PC.
struct CameraInfo {
    std::wstring id;    // the device's symbolic link: stable across restarts
    std::wstring name;  // for people
};

}  // namespace gazeora::input
