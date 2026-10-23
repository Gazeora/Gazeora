#pragma once

#include <windows.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <string>

#include "gazeora/input/interfaces/camera_source.h"
#include "platform/com_scope.h"
#include "platform/media_foundation_scope.h"

namespace gazeora::input {

// Adapter pattern: a webcam read through a Media Foundation source reader, which converts
// whatever the camera sends into 32-bit RGB. Use it on the thread that created it.
class MediaFoundationCamera final : public ICameraSource {
public:
    // Throws InputError if the camera can't be opened.
    explicit MediaFoundationCamera(const std::wstring& id);
    ~MediaFoundationCamera() override;

    MediaFoundationCamera(const MediaFoundationCamera&) = delete;
    MediaFoundationCamera& operator=(const MediaFoundationCamera&) = delete;

    bool Read(CameraFrame& frame) override;

private:
    void CreateReader();
    void ChooseFormat();
    // The fastest format, nearest 640x480 among equals.
    Microsoft::WRL::ComPtr<IMFMediaType> BestNativeType() const;
    void ReadFormat();
    CameraFrame Copy(IMFMediaBuffer* buffer) const;
    CameraFrame CopyRows(const BYTE* firstRow, LONG pitch) const;

    // Declared first: the scopes outlive the Media Foundation objects below.
    ComScope m_com;
    MediaFoundationScope m_mediaFoundation;
    Microsoft::WRL::ComPtr<IMFMediaSource> m_source;
    Microsoft::WRL::ComPtr<IMFSourceReader> m_reader;
    UINT32 m_width = 0;
    UINT32 m_height = 0;
    LONG m_stride = 0;  // negative for pictures stored bottom row first
};

}  // namespace gazeora::input
