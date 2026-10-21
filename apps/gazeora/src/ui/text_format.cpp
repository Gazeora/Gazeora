#include "ui/text_format.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cwchar>

#include "gazeora/common/shared.h"
#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/library/game_library.h"

namespace gazeora::app {

std::string U8(const std::wstring& text) { return WideToUtf8(text); }

std::string RelativeTime(int64_t when) {
    if (when <= 0) {
        return "Never played";
    }
    int64_t ago = UnixNow() - when;
    char text[64];
    if (ago < 60) {
        return "Just now";
    } else if (ago < 3600) {
        snprintf(text, sizeof(text), "%lld min ago", ago / 60);
    } else if (ago < 86400) {
        snprintf(text, sizeof(text), "%lld h ago", ago / 3600);
    } else if (ago < 2 * 86400) {
        return "Yesterday";
    } else if (ago < 30 * 86400) {
        snprintf(text, sizeof(text), "%lld days ago", ago / 86400);
    } else {
        time_t t = time_t(when);
        tm local;
        localtime_s(&local, &t);
        strftime(text, sizeof(text), "%d %b %Y", &local);
    }
    return text;
}

std::string DateText(int64_t when) {
    if (when <= 0) {
        return "-";
    }
    time_t t = time_t(when);
    tm local;
    localtime_s(&local, &t);
    char text[64];
    strftime(text, sizeof(text), "%d %b %Y, %H:%M", &local);
    return text;
}

std::string PlaytimeText(int64_t seconds) {
    char text[32];
    if (seconds <= 0) {
        return "No playtime yet";
    } else if (seconds < 60) {
        return "< 1 min played";
    } else if (seconds < 3600) {
        snprintf(text, sizeof(text), "%lld min played", seconds / 60);
    } else {
        snprintf(text, sizeof(text), "%.1f h played", double(seconds) / 3600.0);
    }
    return text;
}

std::string ClockText(int64_t seconds) {
    char text[32];
    snprintf(text, sizeof(text), "%02lld:%02lld:%02lld", seconds / 3600, (seconds / 60) % 60,
             seconds % 60);
    return text;
}

// Badge text for a session that hasn't finished.
const char* SessionLabel(GameSession::State state) {
    switch (state) {
        case GameSession::State::Launching:
            return "LAUNCHING";
        case GameSession::State::WaitingForGame:
            return "STARTING";
        default:
            return "RUNNING";
    }
}

const char* HookStateText(LONG state) {
    switch (state) {
        case kHookNotLoaded:
            return "Waiting for hook";
        case kHookInitializing:
            return "Hook initializing";
        case kHookInstalled:
            return "Hooked, waiting for the first frame";
        case kHookCapturing:
            return "Capturing";
        case kHookError:
            return "Hook error";
        default:
            return "Unknown";
    }
}

// "3% lost, 3.0 Mbit/s" while players report loss or the stream has backed off because of
// it; empty on a clean link.
std::string StreamHealth(const FrameStreamServer& stream) {
    float loss = stream.PacketLoss();
    uint32_t bitrate = stream.Bitrate();
    if (loss < 0.005f && bitrate >= stream.FullBitrate()) {
        return {};
    }
    char text[96];
    snprintf(text, sizeof(text), "%.0f%% lost, bitrate lowered to %.1f Mbit/s", loss * 100,
             bitrate / 1e6);
    return text;
}

bool EndsWithI(const std::wstring& text, const wchar_t* suffix) {
    size_t n = wcslen(suffix);
    return text.size() >= n && _wcsicmp(text.c_str() + text.size() - n, suffix) == 0;
}

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(tolower(c)); });
    return text;
}

}  // namespace gazeora::app
