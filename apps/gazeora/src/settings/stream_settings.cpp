// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "settings/stream_settings.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwchar>
#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/host/stream/frame_stream.h"
#include "platform/shell.h"
#include "settings/settings_file.h"

namespace gazeora::app {

StreamSettings::StreamSettings() : m_path(SettingsFile::NextToLibrary(L"stream.ini")) {}

std::wstring StreamSettings::LogPath() { return SettingsFile::NextToLibrary(L"stream.log"); }

float StreamSettings::Read(const wchar_t* key, float fallback) const {
    wchar_t text[64];
    GetPrivateProfileStringW(L"vr", key, L"", text, 64, m_path.c_str());
    return text[0] ? float(_wtof(text)) : fallback;
}

void StreamSettings::Write(const wchar_t* key, float value) const {
    wchar_t text[64];
    swprintf(text, 64, L"%.4g", value);
    WritePrivateProfileStringW(L"vr", key, text, m_path.c_str());
}

void StreamSettings::EnsureFolder() const {
    SHCreateDirectoryExW(nullptr, FolderOf(m_path).c_str(), nullptr);
}

VrLayout StreamSettings::LoadVrLayout() const {
    VrLayout vr;
    vr.enabled = Read(L"enabled", 1) != 0;  // on unless turned off
    vr.lensOnPhone = Read(L"lens_on_phone", 1) != 0;
    vr.screenAspect = Read(L"screen_aspect", vr.screenAspect);
    vr.k1 = Read(L"k1", vr.k1);
    vr.k2 = Read(L"k2", vr.k2);
    vr.imageScale = Read(L"image_scale", vr.imageScale);
    vr.lensSeparation = Read(L"lens_separation", vr.lensSeparation);
    vr.chromaCorrection = Read(L"chroma_correction", vr.chromaCorrection);
    vr.stereo = Read(L"stereo", 0) != 0;
    // Keys of their own since the 3D converges on the subject: the older strengths were for
    // a stronger effect.
    vr.stereoDepth = Read(L"stereo_amount", vr.stereoDepth);
    vr.stereoPopOut = Read(L"stereo_front", vr.stereoPopOut);
    return vr;
}

void StreamSettings::SaveVrLayout(const VrLayout& vr) const {
    EnsureFolder();
    Write(L"enabled", vr.enabled ? 1.0f : 0.0f);
    Write(L"screen_aspect", vr.screenAspect);
    Write(L"k1", vr.k1);
    Write(L"k2", vr.k2);
    Write(L"image_scale", vr.imageScale);
    Write(L"lens_separation", vr.lensSeparation);
    Write(L"chroma_correction", vr.chromaCorrection);
    Write(L"lens_on_phone", vr.lensOnPhone ? 1.0f : 0.0f);
    Write(L"stereo", vr.stereo ? 1.0f : 0.0f);
    Write(L"stereo_amount", vr.stereoDepth);
    Write(L"stereo_front", vr.stereoPopOut);
}

VideoCodec StreamSettings::LoadCodec() const {
    wchar_t text[16];
    GetPrivateProfileStringW(L"stream", L"codec", L"h264", text, 16, m_path.c_str());
    return _wcsicmp(text, L"h265") == 0 ? VideoCodec::H265 : VideoCodec::H264;
}

void StreamSettings::SaveCodec(VideoCodec codec) const {
    EnsureFolder();
    WritePrivateProfileStringW(L"stream", L"codec", codec == VideoCodec::H265 ? L"h265" : L"h264",
                               m_path.c_str());
}

uint32_t StreamSettings::LoadMaxBitrate() const {
    UINT mbits = GetPrivateProfileIntW(L"stream", L"max_bitrate_mbps", 0, m_path.c_str());
    return mbits ? std::clamp(mbits, 2u, 100u) * 1000000 : FrameStreamServer::kDefaultMaxBitrate;
}

void StreamSettings::SaveMaxBitrate(uint32_t bitrate) const {
    EnsureFolder();
    WritePrivateProfileStringW(L"stream", L"max_bitrate_mbps",
                               std::to_wstring(bitrate / 1000000).c_str(), m_path.c_str());
}

uint32_t StreamSettings::LoadFrameRate() const {
    UINT fps = GetPrivateProfileIntW(L"stream", L"fps", 0, m_path.c_str());
    return fps ? std::clamp(fps, FrameStreamServer::kMinFrameRate, FrameStreamServer::kMaxFrameRate)
               : FrameStreamServer::kDefaultFrameRate;
}

void StreamSettings::SaveFrameRate(uint32_t fps) const {
    EnsureFolder();
    WritePrivateProfileStringW(L"stream", L"fps", std::to_wstring(fps).c_str(), m_path.c_str());
}

StreamProtocol StreamSettings::LoadProtocol() const {
    wchar_t text[32];
    GetPrivateProfileStringW(L"stream", L"protocol", L"", text, 32, m_path.c_str());
    std::wstring wide(text);
    StreamProtocol protocol = StreamProtocol::RtspTcp;
    ParseProtocolId(Utf8::FromWide(wide), protocol);  // ids are ASCII
    return protocol;
}

namespace {

// The [overlay] switches, by key, with where each one lives.
struct OverlaySwitch {
    const wchar_t* key;
    bool OverlaySettings::* field;
};
constexpr OverlaySwitch kOverlaySwitches[] = {
    {L"enabled", &OverlaySettings::enabled},
    {L"protocol", &OverlaySettings::protocol},
    {L"viewers", &OverlaySettings::viewers},
    {L"codec", &OverlaySettings::codec},
    {L"resolution", &OverlaySettings::resolution},
    {L"frame_rate", &OverlaySettings::frameRate},
    {L"bitrate", &OverlaySettings::bitrate},
    {L"loss", &OverlaySettings::loss},
    {L"encoder", &OverlaySettings::encoder},
    {L"clock", &OverlaySettings::clock},
    {L"head", &OverlaySettings::head},
    {L"camera", &OverlaySettings::camera},
};
constexpr const wchar_t* kCorners[] = {L"top_left", L"top_right", L"bottom_left", L"bottom_right"};

}  // namespace

OverlaySettings StreamSettings::LoadOverlay() const {
    OverlaySettings overlay;
    const wchar_t* file = m_path.c_str();
    for (const OverlaySwitch& one : kOverlaySwitches) {
        overlay.*one.field =
            GetPrivateProfileIntW(L"overlay", one.key, overlay.*one.field ? 1 : 0, file) != 0;
    }
    wchar_t text[512];
    GetPrivateProfileStringW(L"overlay", L"corner", L"", text, 512, file);
    for (int i = 0; i < 4; ++i) {
        if (_wcsicmp(text, kCorners[i]) == 0) {
            overlay.corner = OverlaySettings::Corner(i);
        }
    }
    overlay.textSize = std::clamp(
        int(GetPrivateProfileIntW(L"overlay", L"text_size", overlay.textSize, file)), 50, 300);
    overlay.opacity = std::clamp(
        int(GetPrivateProfileIntW(L"overlay", L"opacity", overlay.opacity, file)), 0, 100);
    GetPrivateProfileStringW(L"overlay", L"text", L"", text, 512, file);
    overlay.text = text;
    return overlay;
}

void StreamSettings::SaveOverlay(const OverlaySettings& overlay) const {
    EnsureFolder();
    const wchar_t* file = m_path.c_str();
    for (const OverlaySwitch& one : kOverlaySwitches) {
        WritePrivateProfileStringW(L"overlay", one.key, overlay.*one.field ? L"1" : L"0", file);
    }
    WritePrivateProfileStringW(L"overlay", L"corner", kCorners[int(overlay.corner)], file);
    WritePrivateProfileStringW(L"overlay", L"text_size", std::to_wstring(overlay.textSize).c_str(),
                               file);
    WritePrivateProfileStringW(L"overlay", L"opacity", std::to_wstring(overlay.opacity).c_str(),
                               file);
    WritePrivateProfileStringW(L"overlay", L"text", overlay.text.c_str(), file);
}

void StreamSettings::SaveProtocol(StreamProtocol protocol) const {
    EnsureFolder();
    std::string id = ProtocolId(protocol);
    WritePrivateProfileStringW(L"stream", L"protocol", std::wstring(id.begin(), id.end()).c_str(),
                               m_path.c_str());
}

}  // namespace gazeora::app
