#include "platform/media_foundation_camera.h"

#include <mfapi.h>
#include <mferror.h>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "gazeora/input/exceptions/input_error.h"
#include "platform/camera_devices.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::input {

namespace {

// Enough for hand tracking (the models look at 192 and 224 pixel squares) and cheap to
// convert; most webcams offer it.
constexpr UINT64 kWantedPixels = 640 * 480;
// More pictures a second keep tracking close to the hand between them.
constexpr UINT32 kWantedFrameRate = 30;
constexpr DWORD kVideoStream = DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
constexpr DWORD kStopFlags = DWORD(MF_SOURCE_READERF_ERROR) | DWORD(MF_SOURCE_READERF_ENDOFSTREAM);
constexpr int kBytesPerPixel = CameraFrame::kBytesPerPixel;

void Check(HRESULT hr, const wchar_t* message) {
    if (FAILED(hr)) {
        throw InputError(message);
    }
}

UINT64 PixelsOf(IMFMediaType* type) {
    UINT32 width = 0, height = 0;
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
    return UINT64(width) * height;
}

UINT64 Distance(UINT64 a, UINT64 b) { return a > b ? a - b : b - a; }

// Whole frames per second, up to what tracking uses: faster isn't worth bigger pictures.
UINT32 FrameRateOf(IMFMediaType* type) {
    UINT32 numerator = 0, denominator = 1;
    MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator, &denominator);
    return std::min(numerator / std::max(denominator, 1u), kWantedFrameRate);
}

}  // namespace

MediaFoundationCamera::MediaFoundationCamera(const std::wstring& id) {
    m_source = CameraDevices::Activate(id);
    try {
        CreateReader();
        ChooseFormat();
        ReadFormat();
    } catch (...) {
        m_source->Shutdown();
        throw;
    }
}

MediaFoundationCamera::~MediaFoundationCamera() {
    m_reader.Reset();
    m_source->Shutdown();
}

void MediaFoundationCamera::CreateReader() {
    ComPtr<IMFAttributes> attributes;
    Check(MFCreateAttributes(&attributes, 1), L"The camera couldn't be set up.");
    Check(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE),
          L"The camera couldn't be set up.");
    Check(MFCreateSourceReaderFromMediaSource(m_source.Get(), attributes.Get(), &m_reader),
          L"The camera couldn't be read. Is another app using it?");
}

void MediaFoundationCamera::ChooseFormat() {
    if (ComPtr<IMFMediaType> native = BestNativeType()) {
        m_reader->SetCurrentMediaType(kVideoStream, nullptr, native.Get());
    }
    ComPtr<IMFMediaType> rgb;
    Check(MFCreateMediaType(&rgb), L"The camera couldn't be set up.");
    rgb->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    rgb->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    Check(m_reader->SetCurrentMediaType(kVideoStream, nullptr, rgb.Get()),
          L"The camera's picture format isn't supported.");
}

ComPtr<IMFMediaType> MediaFoundationCamera::BestNativeType() const {
    ComPtr<IMFMediaType> best, type;
    UINT32 bestRate = 0;
    UINT64 bestDistance = ULLONG_MAX;
    for (DWORD i = 0; SUCCEEDED(m_reader->GetNativeMediaType(kVideoStream, i, &type)); ++i) {
        UINT32 rate = FrameRateOf(type.Get());
        UINT64 distance = Distance(PixelsOf(type.Get()), kWantedPixels);
        if (rate > bestRate || (rate == bestRate && distance < bestDistance)) {
            best = type;
            bestRate = rate;
            bestDistance = distance;
        }
        type.Reset();
    }
    return best;
}

void MediaFoundationCamera::ReadFormat() {
    ComPtr<IMFMediaType> type;
    Check(m_reader->GetCurrentMediaType(kVideoStream, &type), L"The camera couldn't be set up.");
    Check(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &m_width, &m_height),
          L"The camera didn't say its picture size.");
    UINT32 stride = 0;
    if (SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
        m_stride = LONG(stride);
    } else {
        Check(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1, m_width, &m_stride),
              L"The camera's picture layout isn't supported.");
    }
}

bool MediaFoundationCamera::Read(CameraFrame& frame) {
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    ComPtr<IMFSample> sample;
    Check(m_reader->ReadSample(kVideoStream, 0, nullptr, &flags, &timestamp, &sample),
          L"The camera stopped sending pictures.");
    if (flags & kStopFlags) {
        throw InputError(L"The camera stopped sending pictures.");
    }
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
        ReadFormat();
    }
    ComPtr<IMFMediaBuffer> buffer;
    if (!sample || FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
        return false;
    }
    frame = Copy(buffer.Get());
    return true;
}

CameraFrame MediaFoundationCamera::Copy(IMFMediaBuffer* buffer) const {
    ComPtr<IMF2DBuffer> buffer2d;
    BYTE* firstRow = nullptr;
    LONG pitch = 0;
    if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&buffer2d))) &&
        SUCCEEDED(buffer2d->Lock2D(&firstRow, &pitch))) {
        CameraFrame frame = CopyRows(firstRow, pitch);
        buffer2d->Unlock2D();
        return frame;
    }
    BYTE* data = nullptr;
    Check(buffer->Lock(&data, nullptr, nullptr), L"A camera picture couldn't be read.");
    firstRow = m_stride < 0 ? data + size_t(std::abs(m_stride)) * (m_height - 1) : data;
    CameraFrame frame = CopyRows(firstRow, m_stride);
    buffer->Unlock();
    return frame;
}

CameraFrame MediaFoundationCamera::CopyRows(const BYTE* firstRow, LONG pitch) const {
    size_t rowBytes = size_t(m_width) * kBytesPerPixel;
    std::vector<uint8_t> pixels(rowBytes * m_height);
    for (UINT32 row = 0; row < m_height; ++row) {
        std::memcpy(pixels.data() + row * rowBytes, firstRow + LONG(row) * pitch, rowBytes);
    }
    return CameraFrame(int(m_width), int(m_height), std::move(pixels));
}

}  // namespace gazeora::input
