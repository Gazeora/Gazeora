// Stream settings, kept in stream.ini next to the library file so they're easy to edit by
// hand: the codec in [stream], the VR viewer in [vr].
#pragma once

#include <string>
#include <utility>

#include "gazeora/host/capture/vr_layout.h"
#include "gazeora/host/encode/video_codec.h"

namespace gazeora::app {

class StreamSettings {
public:
    // The default location: next to GameLibrary::DefaultPath().
    StreamSettings();
    explicit StreamSettings(std::wstring path) : m_path(std::move(path)) {}

    VrLayout LoadVrLayout() const;
    void SaveVrLayout(const VrLayout& vr) const;
    // "h264" (the default) or "h265".
    VideoCodec LoadCodec() const;
    void SaveCodec(VideoCodec codec) const;

    const std::wstring& Path() const { return m_path; }
    // What players did, rewritten at every start: for working out why a stream stalled.
    static std::wstring LogPath();

private:
    float Read(const wchar_t* key, float fallback) const;
    void Write(const wchar_t* key, float value) const;
    void EnsureFolder() const;

    std::wstring m_path;
};

// Distortion of common phone viewers. Most cheap viewers are close to one of the two
// Cardboard versions; the sliders take it from there.
struct LensPreset {
    const char* name;
    float k1, k2;
};
inline const LensPreset kLensPresets[] = {
    {"Google Cardboard (2015)", 0.34f, 0.55f},
    {"Google Cardboard (2014)", 0.441f, 0.156f},
    {"No lens correction", 0.0f, 0.0f},
};

struct ScreenPreset {
    const char* name;
    float aspect;
};
inline const ScreenPreset kScreenPresets[] = {
    {"16:9", 16.0f / 9}, {"18:9", 18.0f / 9},   {"19.5:9", 19.5f / 9},
    {"20:9", 20.0f / 9}, {"21:9", 21.0f / 9},
};

}  // namespace gazeora::app
