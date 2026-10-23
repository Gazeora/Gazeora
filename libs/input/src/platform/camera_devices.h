#pragma once

#include <windows.h>
#include <mfidl.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "gazeora/input/models/camera_info.h"

namespace gazeora::input {

// The video capture devices Media Foundation sees. Needs COM and Media Foundation started.
class CameraDevices {
public:
    static std::vector<CameraInfo> List();
    // The device with `id`, or the first one for an empty id. Throws InputError if it's
    // missing or Windows won't let it be used.
    static Microsoft::WRL::ComPtr<IMFMediaSource> Activate(const std::wstring& id);
};

}  // namespace gazeora::input
