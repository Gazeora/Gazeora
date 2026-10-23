#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gazeora/input/interfaces/camera_source.h"
#include "gazeora/input/models/camera_info.h"

namespace gazeora::input {

// Factory pattern: the cameras connected to this PC, and opening one.
class ICameraCatalog {
public:
    virtual ~ICameraCatalog() = default;

    virtual std::vector<CameraInfo> List() const = 0;
    // An empty id opens the first camera. Throws InputError if it can't be opened.
    virtual std::unique_ptr<ICameraSource> Open(const std::wstring& id) const = 0;
};

}  // namespace gazeora::input
