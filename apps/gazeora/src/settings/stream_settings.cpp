#include "settings/stream_settings.h"

#include <windows.h>
#include <shlobj.h>

#include <cwchar>

#include "gazeora/library/game_library.h"
#include "platform/shell.h"

namespace gazeora::app {

namespace {

std::wstring NextToLibrary(const wchar_t* name) {
    std::wstring library = GameLibrary::DefaultPath();
    return library.substr(0, library.find_last_of(L"\\/") + 1) + name;
}

}  // namespace

StreamSettings::StreamSettings() : m_path(NextToLibrary(L"stream.ini")) {}

std::wstring StreamSettings::LogPath() { return NextToLibrary(L"stream.log"); }

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
    vr.screenAspect = Read(L"screen_aspect", vr.screenAspect);
    vr.k1 = Read(L"k1", vr.k1);
    vr.k2 = Read(L"k2", vr.k2);
    vr.imageScale = Read(L"image_scale", vr.imageScale);
    vr.lensSeparation = Read(L"lens_separation", vr.lensSeparation);
    vr.chromaCorrection = Read(L"chroma_correction", vr.chromaCorrection);
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

}  // namespace gazeora::app
