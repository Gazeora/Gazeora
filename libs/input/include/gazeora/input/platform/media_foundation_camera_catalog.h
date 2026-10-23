#pragma once

#include "gazeora/input/interfaces/camera_catalog.h"

namespace gazeora::input {

// Adapter pattern: the PC's webcams through Media Foundation.
class MediaFoundationCameraCatalog final : public ICameraCatalog {
public:
    MediaFoundationCameraCatalog();
    ~MediaFoundationCameraCatalog() override;

    MediaFoundationCameraCatalog(const MediaFoundationCameraCatalog&) = delete;
    MediaFoundationCameraCatalog& operator=(const MediaFoundationCameraCatalog&) = delete;

    std::vector<CameraInfo> List() const override;
    std::unique_ptr<ICameraSource> Open(const std::wstring& id) const override;

private:
    bool m_started;  // Media Foundation, for listing cameras
};

}  // namespace gazeora::input
