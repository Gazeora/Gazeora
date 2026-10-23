#pragma once

#include "gazeora/input/models/camera_frame.h"

namespace gazeora::input {

// An open camera. Read throws InputError if the camera stops working.
class ICameraSource {
public:
    virtual ~ICameraSource() = default;

    // Waits for the next picture. False if none came this time (try again).
    virtual bool Read(CameraFrame& frame) = 0;
};

}  // namespace gazeora::input
