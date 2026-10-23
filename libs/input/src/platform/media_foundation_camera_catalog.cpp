#include "gazeora/input/platform/media_foundation_camera_catalog.h"

#include <mfapi.h>

#include "platform/camera_devices.h"
#include "platform/media_foundation_camera.h"

namespace gazeora::input {

MediaFoundationCameraCatalog::MediaFoundationCameraCatalog()
    : m_started(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {}

MediaFoundationCameraCatalog::~MediaFoundationCameraCatalog() {
    if (m_started) {
        MFShutdown();
    }
}

std::vector<CameraInfo> MediaFoundationCameraCatalog::List() const { return CameraDevices::List(); }

std::unique_ptr<ICameraSource> MediaFoundationCameraCatalog::Open(const std::wstring& id) const {
    return std::make_unique<MediaFoundationCamera>(id);
}

}  // namespace gazeora::input
