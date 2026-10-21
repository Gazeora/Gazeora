// VideoEncoder through Media Foundation: the GPU's hardware encoder when the driver offers
// one, a software encoder otherwise (Microsoft's for H.264; for H.265 only if one is
// installed, such as the HEVC Video Extensions).
#include "gazeora/host/encode/video_encoder.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mftransform.h>
#include <strmif.h>
#include <wrl/client.h>

// Instantiated here rather than linked: MinGW has no import library for these GUIDs.
#include <codecapi.h>
#include <initguid.h>
#include <wmcodecdsp.h>

#include <algorithm>
#include <cstring>

// MinGW's strmif.h declares ICodecAPI without the uuid that IID_PPV_ARGS needs.
#ifdef __MINGW32__
__CRT_UUID_DECL(ICodecAPI, 0x901db4c7, 0x31ce, 0x41a2, 0x85, 0xdc, 0x8f, 0xa0, 0xbf, 0x41, 0xb8,
                0xda)
#endif

using Microsoft::WRL::ComPtr;

namespace gazeora {

namespace {

constexpr uint32_t kTargetSlices = 24;

void SetUInt32(ICodecAPI* codec, const GUID& property, uint32_t value) {
    VARIANT v = {};
    v.vt = VT_UI4;
    v.ulVal = value;
    codec->SetValue(&property, &v);
}

void SetBool(ICodecAPI* codec, const GUID& property, bool value) {
    VARIANT v = {};
    v.vt = VT_BOOL;
    v.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    codec->SetValue(&property, &v);
}

std::wstring HrText(const wchar_t* what, HRESULT hr) {
    wchar_t text[96];
    swprintf(text, 96, L"%ls failed (0x%08lX)", what, static_cast<unsigned long>(hr));
    return text;
}

const GUID& Subtype(VideoCodec codec) {
    return codec == VideoCodec::H265 ? MFVideoFormat_HEVC : MFVideoFormat_H264;
}

// The registered encoders from NV12 to `codec` that match `flags`. Release each, and free
// the array with CoTaskMemFree.
UINT32 FindEncoders(VideoCodec codec, UINT32 flags, IMFActivate*** activates) {
    MFT_REGISTER_TYPE_INFO input = {MFMediaType_Video, MFVideoFormat_NV12};
    MFT_REGISTER_TYPE_INFO output = {MFMediaType_Video, Subtype(codec)};
    UINT32 count = 0;
    if (FAILED(MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags | MFT_ENUM_FLAG_SORTANDFILTER, &input,
                         &output, activates, &count))) {
        *activates = nullptr;
        return 0;
    }
    return count;
}

class MediaFoundationEncoder final : public VideoEncoder {
public:
    MediaFoundationEncoder() = default;
    MediaFoundationEncoder(const MediaFoundationEncoder&) = delete;
    MediaFoundationEncoder& operator=(const MediaFoundationEncoder&) = delete;
    ~MediaFoundationEncoder() override { Close(); }

    bool Open(const Settings& settings, std::wstring& error) override;
    void Close() override;
    bool IsOpen() const override { return m_transform != nullptr; }
    const Settings& Current() const override { return m_settings; }
    const std::wstring& Name() const override { return m_name; }
    bool IsHardware() const override { return m_hardware; }
    void SetBitrate(uint32_t bitrate) override;
    void RequestKeyFrame() override { m_keyFrameRequested = true; }
    bool Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                std::wstring& error) override;

private:
    bool Configure(IMFTransform* transform, uint32_t fps, uint32_t bitrate, uint32_t keyInterval,
                   std::wstring& error);
    // Tries the registered encoders that match `flags` in order; true once one is set up.
    bool OpenRegistered(UINT32 flags, uint32_t fps, uint32_t bitrate, uint32_t keyInterval,
                        std::wstring& error);
    // Reads every finished frame (sync encoders) or the one just signaled (async ones).
    bool ReadOutput(std::vector<Frame>& out, bool& needMoreInput, std::wstring& error);
    // Async encoders: handles one event. False on failure.
    bool PumpEvent(std::vector<Frame>& out, bool& gotOutput, std::wstring& error);

    ComPtr<IMFTransform> m_transform;
    ComPtr<ICodecAPI> m_codecApi;
    ComPtr<IMFMediaEventGenerator> m_events;  // async encoders only
    Settings m_settings;
    VideoCodec m_codec = VideoCodec::H264;
    std::wstring m_name;
    bool m_hardware = false;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    int64_t m_frameDuration = 0;
    bool m_providesSamples = false;
    uint32_t m_outputSize = 0;
    int m_needInput = 0;  // async: inputs the encoder has asked for
    bool m_keyFrameRequested = false;
};

}  // namespace

std::unique_ptr<VideoEncoder> CreateVideoEncoder() {
    return std::make_unique<MediaFoundationEncoder>();
}

bool VideoEncoder::Available(VideoCodec codec) {
    if (codec == VideoCodec::H264) {
        return true;  // Microsoft's software encoder ships with Windows
    }
    IMFActivate** activates = nullptr;
    UINT32 count = FindEncoders(codec, MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SYNCMFT |
                                           MFT_ENUM_FLAG_ASYNCMFT,
                                &activates);
    for (UINT32 i = 0; i < count; ++i) {
        activates[i]->Release();
    }
    CoTaskMemFree(activates);
    return count > 0;
}

bool MediaFoundationEncoder::Open(const Settings& settings, std::wstring& error) {
    Close();
    const VideoCodec codec = settings.codec;
    const uint32_t fps = settings.fps, bitrate = settings.bitrate;
    const uint32_t keyInterval = settings.keyInterval;
    m_settings = settings;
    m_codec = codec;
    m_width = m_settings.width = settings.width & ~1u;
    m_height = m_settings.height = settings.height & ~1u;
    m_frameDuration = 10000000 / std::max(1u, fps);
    if (m_width < 16 || m_height < 16) {
        error = L"frame too small to encode";
        return false;
    }

    if (settings.preferHardware && OpenRegistered(MFT_ENUM_FLAG_HARDWARE, fps, bitrate, keyInterval, error)) {
        m_hardware = true;
        return true;
    }
    if (codec == VideoCodec::H265) {
        // No H.265 encoder comes with Windows; use an installed software one if there is.
        if (!OpenRegistered(MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT, fps, bitrate,
                            keyInterval, error)) {
            if (error.empty()) error = L"no H.265 encoder on this PC";
            return false;
        }
        return true;
    }

    if (!m_transform) {
        ComPtr<IMFTransform> transform;
        HRESULT hr = CoCreateInstance(CLSID_CMSH264EncoderMFT, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&transform));
        if (FAILED(hr)) {
            error = HrText(L"creating the H.264 encoder", hr);
            return false;
        }
        if (!Configure(transform.Get(), fps, bitrate, keyInterval, error)) {
            return false;
        }
        m_name = L"Microsoft H.264 software encoder";
    }
    return true;
}

bool MediaFoundationEncoder::OpenRegistered(UINT32 flags, uint32_t fps, uint32_t bitrate,
                                  uint32_t keyInterval, std::wstring& error) {
    IMFActivate** activates = nullptr;
    UINT32 count = FindEncoders(m_codec, flags, &activates);
    for (UINT32 i = 0; i < count; ++i) {
        ComPtr<IMFTransform> transform;
        if (!m_transform && SUCCEEDED(activates[i]->ActivateObject(IID_PPV_ARGS(&transform))) &&
            Configure(transform.Get(), fps, bitrate, keyInterval, error)) {
            wchar_t* name = nullptr;
            UINT32 length = 0;
            if (SUCCEEDED(
                    activates[i]->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &name, &length))) {
                m_name = name;
                CoTaskMemFree(name);
            }
        }
        if (!m_transform) {
            activates[i]->ShutdownObject();
        }
        activates[i]->Release();
    }
    CoTaskMemFree(activates);
    if (m_transform) {
        error.clear();
    }
    return m_transform != nullptr;
}

bool MediaFoundationEncoder::Configure(IMFTransform* transform, uint32_t fps, uint32_t bitrate,
                            uint32_t keyInterval, std::wstring& error) {
    // Hardware encoders are asynchronous and refuse to work until unlocked.
    ComPtr<IMFAttributes> attributes;
    ComPtr<IMFMediaEventGenerator> events;
    UINT32 async = 0;
    if (SUCCEEDED(transform->GetAttributes(&attributes)) &&
        SUCCEEDED(attributes->GetUINT32(MF_TRANSFORM_ASYNC, &async)) && async) {
        attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
        if (FAILED(transform->QueryInterface(IID_PPV_ARGS(&events)))) {
            error = L"async encoder without events";
            return false;
        }
    }

    // Rate control and latency settings go in before the media types, which some encoders
    // read them from.
    ComPtr<ICodecAPI> codec;
    if (SUCCEEDED(transform->QueryInterface(IID_PPV_ARGS(&codec)))) {
        SetBool(codec.Get(), CODECAPI_AVLowLatencyMode, true);
        SetUInt32(codec.Get(), CODECAPI_AVEncCommonRateControlMode,
                  eAVEncCommonRateControlMode_CBR);
        SetUInt32(codec.Get(), CODECAPI_AVEncCommonMeanBitRate, bitrate);
        SetUInt32(codec.Get(), CODECAPI_AVEncMPVGOPSize, keyInterval);
        SetUInt32(codec.Get(), CODECAPI_AVEncMPVDefaultBPictureCount, 0);
        // Cut each frame into about kTargetSlices slices of whole macroblock rows, so a frame
        // is many small NAL units of a packet or few. A lost packet then costs one strip,
        // which the player fills in from the previous frame, instead of the whole frame.
        // FFmpeg, which VLC decodes with, ignores H.264 slices past the 32nd. NVIDIA's H.265
        // encoder ignores rows and takes a count of 32x32 coding blocks instead (mode 0).
        // Encoders that don't take either keep their own slicing.
        if (m_codec == VideoCodec::H265) {
            uint32_t blocks = ((m_width + 31) / 32) * ((m_height + 31) / 32);
            SetUInt32(codec.Get(), CODECAPI_AVEncSliceControlMode, 0);  // coding blocks
            SetUInt32(codec.Get(), CODECAPI_AVEncSliceControlSize,
                      (blocks + kTargetSlices - 1) / kTargetSlices);
        } else {
            uint32_t macroblockRows = (m_height + 15) / 16;
            SetUInt32(codec.Get(), CODECAPI_AVEncSliceControlMode, 2);  // macroblock rows
            SetUInt32(codec.Get(), CODECAPI_AVEncSliceControlSize,
                      (macroblockRows + kTargetSlices - 1) / kTargetSlices);
        }
    }

    ComPtr<IMFMediaType> output, input;
    HRESULT hr = MFCreateMediaType(&output);
    if (SUCCEEDED(hr)) {
        output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        output->SetGUID(MF_MT_SUBTYPE, Subtype(m_codec));
        output->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
        MFSetAttributeSize(output.Get(), MF_MT_FRAME_SIZE, m_width, m_height);
        MFSetAttributeRatio(output.Get(), MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(output.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        output->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        // H.264 High rather than Baseline: CABAC and 8x8 transforms, about 15% fewer bits for
        // the same picture. Every player in question decodes it. H.265 Main, 8-bit 4:2:0.
        output->SetUINT32(MF_MT_MPEG2_PROFILE, m_codec == VideoCodec::H265
                                                   ? UINT32(eAVEncH265VProfile_Main_420_8)
                                                   : UINT32(eAVEncH264VProfile_High));
        hr = transform->SetOutputType(0, output.Get(), 0);
    }
    if (FAILED(hr)) {
        error = HrText(m_codec == VideoCodec::H265 ? L"setting the H.265 output type"
                                                   : L"setting the H.264 output type",
                       hr);
        return false;
    }
    hr = MFCreateMediaType(&input);
    if (SUCCEEDED(hr)) {
        input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        MFSetAttributeSize(input.Get(), MF_MT_FRAME_SIZE, m_width, m_height);
        MFSetAttributeRatio(input.Get(), MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(input.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        input->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        input->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
        input->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
        hr = transform->SetInputType(0, input.Get(), 0);
    }
    if (FAILED(hr)) {
        error = HrText(L"setting the NV12 input type", hr);
        return false;
    }

    MFT_OUTPUT_STREAM_INFO info = {};
    transform->GetOutputStreamInfo(0, &info);
    m_providesSamples =
        info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES);
    m_outputSize = std::max<uint32_t>(info.cbSize, m_width * m_height * 3 / 2);

    transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    m_transform = transform;
    m_codecApi = codec;
    m_events = events;
    m_needInput = 0;
    m_keyFrameRequested = false;
    return true;
}

void MediaFoundationEncoder::Close() {
    if (m_transform) {
        m_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        m_transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        // Hardware encoders run threads of their own; shut them down before letting go.
        ComPtr<IMFShutdown> shutdown;
        if (m_events && SUCCEEDED(m_transform.As(&shutdown))) {
            shutdown->Shutdown();
        }
    }
    m_events.Reset();
    m_codecApi.Reset();
    m_transform.Reset();
    m_name.clear();
    m_hardware = false;
}

void MediaFoundationEncoder::SetBitrate(uint32_t bitrate) {
    if (m_codecApi) {
        SetUInt32(m_codecApi.Get(), CODECAPI_AVEncCommonMeanBitRate, bitrate);
    }
}

bool MediaFoundationEncoder::Encode(const uint8_t* nv12, int64_t time, std::vector<Frame>& out,
                         std::wstring& error) {
    if (!m_transform) {
        error = L"encoder not open";
        return false;
    }
    DWORD size = m_width * m_height * 3 / 2;
    ComPtr<IMFMediaBuffer> buffer;
    ComPtr<IMFSample> sample;
    BYTE* data = nullptr;
    if (FAILED(MFCreateMemoryBuffer(size, &buffer)) ||
        FAILED(buffer->Lock(&data, nullptr, nullptr))) {
        error = L"out of memory for the encoder input";
        return false;
    }
    memcpy(data, nv12, size);
    buffer->Unlock();
    buffer->SetCurrentLength(size);
    MFCreateSample(&sample);
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(time);
    sample->SetSampleDuration(m_frameDuration);
    if (m_keyFrameRequested && m_codecApi) {
        SetUInt32(m_codecApi.Get(), CODECAPI_AVEncVideoForceKeyFrame, 1);
        m_keyFrameRequested = false;
    }

    if (m_events) {
        // Wait until the encoder asks for input, taking any frame it finishes meanwhile.
        bool gotOutput = false;
        while (m_needInput == 0) {
            if (!PumpEvent(out, gotOutput, error)) return false;
        }
        --m_needInput;
        HRESULT hr = m_transform->ProcessInput(0, sample.Get(), 0);
        if (FAILED(hr)) {
            error = HrText(L"encoding", hr);
            return false;
        }
        // Then until this frame comes out, or the encoder wants the next one first.
        gotOutput = false;
        while (!gotOutput && m_needInput == 0) {
            if (!PumpEvent(out, gotOutput, error)) return false;
        }
        return true;
    }

    HRESULT hr = m_transform->ProcessInput(0, sample.Get(), 0);
    if (FAILED(hr)) {
        error = HrText(L"encoding", hr);
        return false;
    }
    bool needMoreInput = false;
    while (!needMoreInput) {
        if (!ReadOutput(out, needMoreInput, error)) return false;
    }
    return true;
}

bool MediaFoundationEncoder::PumpEvent(std::vector<Frame>& out, bool& gotOutput, std::wstring& error) {
    ComPtr<IMFMediaEvent> event;
    HRESULT hr = m_events->GetEvent(0, &event);  // blocks until the encoder has news
    if (FAILED(hr)) {
        error = HrText(L"waiting for the encoder", hr);
        return false;
    }
    MediaEventType type = MEUnknown;
    event->GetType(&type);
    if (type == METransformNeedInput) {
        ++m_needInput;
    } else if (type == METransformHaveOutput) {
        size_t before = out.size();
        bool needMoreInput = false;
        if (!ReadOutput(out, needMoreInput, error)) return false;
        gotOutput |= out.size() > before;
    }
    return true;
}

bool MediaFoundationEncoder::ReadOutput(std::vector<Frame>& out, bool& needMoreInput, std::wstring& error) {
    MFT_OUTPUT_DATA_BUFFER output = {};
    ComPtr<IMFSample> ownSample;
    if (!m_providesSamples) {
        ComPtr<IMFMediaBuffer> buffer;
        MFCreateSample(&ownSample);
        MFCreateMemoryBuffer(m_outputSize, &buffer);
        ownSample->AddBuffer(buffer.Get());
        output.pSample = ownSample.Get();
    }
    DWORD status = 0;
    HRESULT hr = m_transform->ProcessOutput(0, 1, &output, &status);
    if (output.pEvents) output.pEvents->Release();
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        needMoreInput = true;
        return true;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        // The encoder settled on its output format; take it as offered.
        ComPtr<IMFMediaType> type;
        if (SUCCEEDED(m_transform->GetOutputAvailableType(0, 0, &type))) {
            m_transform->SetOutputType(0, type.Get(), 0);
        }
        return true;
    }
    if (FAILED(hr)) {
        error = HrText(L"reading encoded frames", hr);
        return false;
    }
    ComPtr<IMFSample> sample;
    sample.Attach(m_providesSamples ? output.pSample : ownSample.Detach());
    ComPtr<IMFMediaBuffer> contiguous;
    BYTE* data = nullptr;
    DWORD length = 0;
    if (sample && SUCCEEDED(sample->ConvertToContiguousBuffer(&contiguous)) &&
        SUCCEEDED(contiguous->Lock(&data, nullptr, &length))) {
        Frame frame;
        frame.annexB.assign(reinterpret_cast<const char*>(data), length);
        contiguous->Unlock();
        sample->GetSampleTime(&frame.time);
        frame.keyFrame = MFGetAttributeUINT32(sample.Get(), MFSampleExtension_CleanPoint, 0) != 0;
        if (!frame.annexB.empty()) {
            out.push_back(std::move(frame));
        }
    }
    // Async encoders hand out one frame per HaveOutput event; sync ones are drained here.
    needMoreInput = m_events != nullptr;
    return true;
}

}  // namespace gazeora
