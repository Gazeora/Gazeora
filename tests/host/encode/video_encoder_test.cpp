// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include <windows.h>
#include <d3d11.h>
#include <gtest/gtest.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <initguid.h>
#include <wmcodecdsp.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/encode/color_convert.h"
#include "gazeora/host/encode/gpu_nv12_converter.h"
#include "gazeora/host/encode/video_encoder.h"
#include "gazeora/host/stream/rtp_video.h"

using Microsoft::WRL::ComPtr;

namespace gazeora {
namespace {

// The conversion as plain per-pixel arithmetic. BgraToNv12 works on many pixels at a time and
// must give exactly these bytes.
std::vector<uint8_t> ReferenceNv12(const uint8_t* bgra, uint32_t stride, uint32_t width,
                                   uint32_t height) {
    std::vector<uint8_t> nv12(size_t(width) * height * 3 / 2);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t* p = bgra + (size_t(y) * stride + x) * 4;
            nv12[size_t(y) * width + x] =
                uint8_t(16 + ((47 * p[2] + 157 * p[1] + 16 * p[0] + 128) >> 8));
        }
    }
    uint8_t* uv = nv12.data() + size_t(width) * height;
    for (uint32_t y = 0; y < height; y += 2) {
        for (uint32_t x = 0; x < width; x += 2) {
            int sum[3] = {};
            for (uint32_t dy = 0; dy < 2; ++dy) {
                for (uint32_t dx = 0; dx < 2; ++dx) {
                    const uint8_t* p = bgra + (size_t(y + dy) * stride + x + dx) * 4;
                    for (int c = 0; c < 3; ++c) sum[c] += p[c];
                }
            }
            int b = sum[0], g = sum[1], r = sum[2];
            uv[size_t(y / 2) * width + x] =
                uint8_t(128 + ((-26 * r - 87 * g + 112 * b + 512) >> 10));
            uv[size_t(y / 2) * width + x + 1] =
                uint8_t(128 + ((112 * r - 102 * g - 10 * b + 512) >> 10));
        }
    }
    return nv12;
}

TEST(BgraToNv12Test, MatchesPerPixelArithmeticAtEveryWidth) {
    // Even widths from 2 to 70 cover every remainder of the many-pixels-at-a-time loops;
    // the stride leaves padding at the end of each source row.
    std::vector<uint8_t> bgra(80 * 6 * 4);
    uint32_t seed = 11;
    for (uint8_t& b : bgra) b = uint8_t((seed = seed * 1664525u + 1013904223u) >> 24);
    std::vector<uint8_t> nv12;
    for (uint32_t width = 2; width <= 70; width += 2) {
        for (uint32_t stride : {width, width + 3}) {
            BgraToNv12(bgra.data(), stride, width, 6, nv12);
            ASSERT_EQ(nv12, ReferenceNv12(bgra.data(), stride, width, 6))
                << "width " << width << ", stride " << stride;
        }
    }
}

TEST(BgraToNv12Test, MapsColorsToStudioRangeBt709) {
    // 2x2 of one color: Y for each pixel, then one U/V pair.
    auto convert = [](uint8_t b, uint8_t g, uint8_t r) {
        // Built up front: GCC 13 at -O3 warns (falsely) about insert() of an initializer list.
        std::vector<uint8_t> bgra = {b, g, r, 255, b, g, r, 255, b, g, r, 255, b, g, r, 255};
        std::vector<uint8_t> nv12;
        BgraToNv12(bgra.data(), 2, 2, 2, nv12);
        EXPECT_EQ(nv12.size(), 6u);
        return nv12;
    };
    std::vector<uint8_t> white = convert(255, 255, 255);
    EXPECT_NEAR(white[0], 235, 1);
    EXPECT_NEAR(white[4], 128, 1);
    EXPECT_NEAR(white[5], 128, 1);
    std::vector<uint8_t> black = convert(0, 0, 0);
    EXPECT_EQ(black[0], 16);
    EXPECT_EQ(black[4], 128);
    std::vector<uint8_t> red = convert(0, 0, 255);  // BT.709: Y 63, U 102, V 240
    EXPECT_NEAR(red[0], 63, 1);
    EXPECT_NEAR(red[4], 102, 1);
    EXPECT_NEAR(red[5], 240, 1);
}

ComPtr<IMFTransform> CreateDecoder(VideoCodec codec) {
    ComPtr<IMFTransform> decoder;
    if (codec == VideoCodec::H264) {
        CoCreateInstance(CLSID_CMSH264DecoderMFT, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&decoder));
        return decoder;
    }
    MFT_REGISTER_TYPE_INFO input = {MFMediaType_Video, MFVideoFormat_HEVC};
    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    if (SUCCEEDED(MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER, &input, nullptr,
                            &activates, &count))) {
        for (UINT32 i = 0; i < count; ++i) {
            if (!decoder) activates[i]->ActivateObject(IID_PPV_ARGS(&decoder));
            activates[i]->Release();
        }
        CoTaskMemFree(activates);
    }
    return decoder;
}

// Decodes Annex B access units; returns the decoded NV12 frames' average luma.
std::vector<double> DecodeAverageLuma(ComPtr<IMFTransform> decoder, VideoCodec codec,
                                      const std::vector<std::string>& accessUnits) {
    std::vector<double> luma;
    ComPtr<IMFMediaType> input;
    MFCreateMediaType(&input);
    input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input->SetGUID(MF_MT_SUBTYPE,
                   codec == VideoCodec::H265 ? MFVideoFormat_HEVC : MFVideoFormat_H264);
    // Microsoft's H.265 decoder wants the size up front; the H.264 one finds it in the stream.
    MFSetAttributeSize(input.Get(), MF_MT_FRAME_SIZE, 320, 240);
    EXPECT_TRUE(SUCCEEDED(decoder->SetInputType(0, input.Get(), 0)));
    auto setNv12Output = [&] {
        for (DWORD i = 0;; ++i) {
            ComPtr<IMFMediaType> type;
            if (FAILED(decoder->GetOutputAvailableType(0, i, &type))) return;
            GUID subtype;
            type->GetGUID(MF_MT_SUBTYPE, &subtype);
            if (subtype == MFVideoFormat_NV12) {
                decoder->SetOutputType(0, type.Get(), 0);
                return;
            }
        }
    };
    // The decoded frame size, from the output type; it can be padded beyond the picture
    // (H.265 to whole coding blocks), so only the picture's 320x240 is averaged.
    UINT32 width = 0, height = 0;
    auto readSize = [&] {
        ComPtr<IMFMediaType> type;
        if (SUCCEEDED(decoder->GetOutputCurrentType(0, &type))) {
            MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
        }
    };
    setNv12Output();
    readSize();

    auto drain = [&] {
        for (;;) {
            MFT_OUTPUT_STREAM_INFO info = {};
            decoder->GetOutputStreamInfo(0, &info);
            ComPtr<IMFSample> sample;
            ComPtr<IMFMediaBuffer> buffer;
            MFCreateSample(&sample);
            MFCreateMemoryBuffer(std::max<DWORD>(info.cbSize, 4 << 20), &buffer);
            sample->AddBuffer(buffer.Get());
            MFT_OUTPUT_DATA_BUFFER output = {};
            output.pSample = sample.Get();
            DWORD status = 0;
            HRESULT hr = decoder->ProcessOutput(0, 1, &output, &status);
            if (output.pEvents) output.pEvents->Release();
            if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
                setNv12Output();
                readSize();
                continue;
            }
            if (FAILED(hr)) return;
            BYTE* data;
            DWORD length;
            buffer->Lock(&data, nullptr, &length);
            UINT32 rows = std::min<UINT32>(height, 240), columns = std::min<UINT32>(width, 320);
            if (width > 0 && size_t(width) * rows <= length) {  // the Y plane, row by row
                double sum = 0;
                for (UINT32 y = 0; y < rows; ++y) {
                    for (UINT32 x = 0; x < columns; ++x) sum += data[size_t(y) * width + x];
                }
                luma.push_back(sum / (double(rows) * columns));
            }
            buffer->Unlock();
        }
    };
    int64_t time = 0;
    for (const std::string& unit : accessUnits) {
        ComPtr<IMFSample> sample;
        ComPtr<IMFMediaBuffer> buffer;
        MFCreateSample(&sample);
        MFCreateMemoryBuffer(DWORD(unit.size()), &buffer);
        BYTE* data;
        buffer->Lock(&data, nullptr, nullptr);
        memcpy(data, unit.data(), unit.size());
        buffer->Unlock();
        buffer->SetCurrentLength(DWORD(unit.size()));
        sample->AddBuffer(buffer.Get());
        sample->SetSampleTime(time += 333333);
        HRESULT hr = decoder->ProcessInput(0, sample.Get(), 0);
        EXPECT_TRUE(SUCCEEDED(hr)) << std::hex << hr;
        drain();
    }
    decoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    drain();
    return luma;
}

class VideoEncoderTest : public ::testing::TestWithParam<std::tuple<VideoCodec, bool>> {
protected:
    void SetUp() override {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ASSERT_TRUE(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)));
    }
    void TearDown() override {
        MFShutdown();
        CoUninitialize();
    }
};

TEST_P(VideoEncoderTest, EncodesFramesThatSurviveRtpAndDecode) {
    const VideoCodec codec = std::get<0>(GetParam());
    const bool hardware = std::get<1>(GetParam());
    const bool hevc = codec == VideoCodec::H265;
    std::unique_ptr<VideoEncoder> encoderPtr = CreateVideoEncoder();
    VideoEncoder& encoder = *encoderPtr;
    std::wstring error;
    if (hevc && !VideoEncoder::Available(codec)) {
        GTEST_SKIP() << "no H.265 encoder on this machine";
    }
    VideoEncoder::Settings settings;
    settings.codec = codec;
    settings.width = 320;
    settings.height = 240;
    settings.bitrate = 2000000;
    settings.preferHardware = hardware;
    bool opened = encoder.Open(settings, error);
    if (!opened && hevc && !hardware) {
        GTEST_SKIP() << "no software H.265 encoder on this machine";
    }
    ASSERT_TRUE(opened) << Utf8::FromWide(error);
    if (hardware != encoder.IsHardware()) {
        GTEST_SKIP() << "no " << (hardware ? "hardware " : "software ") << CodecName(codec)
                     << " encoder on this machine";
    }
    EXPECT_EQ(encoder.Current().codec, codec);

    // Mid grey, then lighter: the decoded luma must follow.
    std::vector<uint8_t> bgra(320 * 240 * 4), nv12;
    std::vector<VideoEncoder::Frame> frames;
    for (int i = 0; i < 10; ++i) {
        uint8_t level = i < 5 ? 100 : 200;
        for (size_t p = 0; p < bgra.size(); ++p) bgra[p] = (p % 4 == 3) ? 255 : level;
        BgraToNv12(bgra.data(), 320, 320, 240, nv12);
        ASSERT_TRUE(encoder.Encode(nv12.data(), int64_t(i) * 333333, frames, error))
            << Utf8::FromWide(error);
    }
    ASSERT_GE(frames.size(), 9u) << "an encoder may hold back one frame, not more";
    EXPECT_TRUE(frames[0].keyFrame);
    std::vector<std::string> nals;
    SplitAnnexB(frames[0].annexB, nals);
    bool vps = !hevc, sps = false, pps = false, idr = false;
    for (const std::string& nal : nals) {
        uint8_t type = hevc ? RtpPayloadFormat::For(VideoCodec::H265).NalType(nal)
                            : RtpPayloadFormat::For(VideoCodec::H264).NalType(nal);
        vps |= hevc && type == kHevcVps;
        sps |= type == (hevc ? uint8_t(kHevcSps) : uint8_t(kNalSps));
        pps |= type == (hevc ? uint8_t(kHevcPps) : uint8_t(kNalPps));
        idr |= RtpPayloadFormat::For(codec).IsKeyFrame(nal);
    }
    EXPECT_TRUE(vps && sps && pps && idr) << "the first frame must be decodable on its own";
    // Slicing, so a lost packet costs a strip rather than the frame. Checked where it's known
    // to be honored; other encoders may keep their own slicing.
    if (encoder.Name().find(L"NVIDIA") != std::wstring::npos) {
        int slices = 0;
        for (const std::string& nal : nals) {
            slices += hevc ? RtpPayloadFormat::For(VideoCodec::H265).NalType(nal) < kHevcVps
                           : (RtpPayloadFormat::For(VideoCodec::H264).NalType(nal) == kNalSlice ||
                              RtpPayloadFormat::For(VideoCodec::H264).NalType(nal) == kNalIdrSlice);
        }
        EXPECT_GE(slices, 8);
    }
    for (size_t i = 1; i < frames.size(); ++i) {
        EXPECT_GT(frames[i].time, frames[i - 1].time);
    }

    // Through RTP and back.
    std::vector<std::string> accessUnits;
    uint16_t seq = 0;
    for (const VideoEncoder::Frame& frame : frames) {
        SplitAnnexB(frame.annexB, nals);
        std::vector<std::string> packets;
        RtpPayloadFormat::For(codec).Packetize(nals, 0, 0, 1400, seq, packets);
        RtpVideoDepacketizer depacketizer(codec);
        std::string annexB;
        for (const std::string& packet : packets) {
            ASSERT_TRUE(depacketizer.Add(reinterpret_cast<const uint8_t*>(packet.data()) + 12,
                                         packet.size() - 12, annexB));
        }
        accessUnits.push_back(annexB);
    }
    ComPtr<IMFTransform> decoder = CreateDecoder(codec);
    if (!decoder) {
        GTEST_SKIP() << "encoded and packetized, but there's no " << CodecName(codec)
                     << " decoder on this machine to check the picture";
    }
    std::vector<double> luma = DecodeAverageLuma(decoder, codec, accessUnits);
    ASSERT_GE(luma.size(), 8u);
    // BT.709 studio range: 100 -> 16 + 100 * 219 / 255 = 102, 200 -> 188.
    EXPECT_NEAR(luma.front(), 102, 4);
    EXPECT_NEAR(luma.back(), 188, 4);
}

INSTANTIATE_TEST_SUITE_P(CodecsAndEncoders, VideoEncoderTest,
                         ::testing::Combine(::testing::Values(VideoCodec::H264, VideoCodec::H265),
                                            ::testing::Values(false, true)),
                         [](const auto& info) {
                             return std::string(std::get<0>(info.param) == VideoCodec::H265
                                                    ? "H265"
                                                    : "H264") +
                                    (std::get<1>(info.param) ? "Hardware" : "Software");
                         });

// The GPU path end to end, on this machine's GPU: frames converted to NV12 there and encoded
// from the textures decode to the frames' brightness.
class VideoEncoderTextureTest : public ::testing::TestWithParam<VideoCodec> {
protected:
    void SetUp() override {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ASSERT_TRUE(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)));
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                          D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                          nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr);
    }
    void TearDown() override {
        device.Reset();
        MFShutdown();
        CoUninitialize();
    }

    ComPtr<ID3D11Device> device;
};

TEST_P(VideoEncoderTextureTest, EncodesNv12TexturesFromTheGpu) {
    const VideoCodec codec = GetParam();
    if (!device) {
        GTEST_SKIP() << "no hardware D3D11 device";
    }
    std::unique_ptr<VideoEncoder> encoder = CreateVideoEncoder();
    VideoEncoder::Settings settings;
    settings.codec = codec;
    settings.width = 320;
    settings.height = 240;
    settings.bitrate = 2000000;
    settings.device = device.Get();
    std::wstring error;
    if (!encoder->Open(settings, error)) {
        GTEST_SKIP() << "no GPU " << CodecName(codec)
                     << " encoder takes textures here: " << Utf8::FromWide(error);
    }
    EXPECT_TRUE(encoder->IsHardware());
    EXPECT_TRUE(encoder->Matches(codec, 320, 240, device.Get()));

    GpuNv12Converter converter;
    std::vector<VideoEncoder::Frame> frames;
    for (int i = 0; i < 10; ++i) {
        uint32_t level = i < 5 ? 100 : 200;
        std::vector<uint32_t> pixels(320 * 240, 0xFF000000 | level << 16 | level << 8 | level);
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 320;
        desc.Height = 240;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data = {pixels.data(), 320 * 4, 0};
        ComPtr<ID3D11Texture2D> frame;
        ASSERT_TRUE(SUCCEEDED(device->CreateTexture2D(&desc, &data, &frame)));
        UINT width = 0, height = 0;
        ComPtr<ID3D11Texture2D> nv12 =
            converter.Convert(device.Get(), frame.Get(), width, height, error);
        ASSERT_TRUE(nv12) << Utf8::FromWide(error);
        ASSERT_TRUE(encoder->EncodeTexture(nv12.Get(), int64_t(i) * 333333, frames, error))
            << Utf8::FromWide(error);
    }
    ASSERT_GE(frames.size(), 9u) << "an encoder may hold back one frame, not more";
    EXPECT_TRUE(frames[0].keyFrame);

    ComPtr<IMFTransform> decoder = CreateDecoder(codec);
    if (!decoder) {
        GTEST_SKIP() << "encoded, but there's no " << CodecName(codec) << " decoder to check it";
    }
    std::vector<std::string> accessUnits;
    for (const VideoEncoder::Frame& frame : frames) accessUnits.push_back(frame.annexB);
    std::vector<double> luma = DecodeAverageLuma(decoder, codec, accessUnits);
    ASSERT_GE(luma.size(), 8u);
    EXPECT_NEAR(luma.front(), 102, 4);
    EXPECT_NEAR(luma.back(), 188, 4);
}

INSTANTIATE_TEST_SUITE_P(Codecs, VideoEncoderTextureTest,
                         ::testing::Values(VideoCodec::H264, VideoCodec::H265),
                         [](const auto& info) {
                             return std::string(info.param == VideoCodec::H265 ? "H265" : "H264");
                         });

}  // namespace
}  // namespace gazeora
