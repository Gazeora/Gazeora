// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Text the launcher shows: times, durations, states. Pure functions of their inputs.
#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

#include "game_session.h"

namespace gazeora {
class FrameStreamServer;
}

namespace gazeora::app {

std::string U8(const std::wstring& text);

std::string RelativeTime(int64_t when);  // "3 days ago"
std::string DateText(int64_t when);      // "21 Oct 2026, 21:30"
std::string PlaytimeText(int64_t seconds);
std::string ClockText(int64_t seconds);  // "01:02:03"

// Badge text for a session that hasn't finished.
const char* SessionLabel(GameSession::State state);
const char* HookStateText(LONG state);

// "3% lost, 3.0 Mbit/s" while players report loss or the stream has backed off because of
// it; empty on a clean link.
std::string StreamHealth(const FrameStreamServer& stream);

bool EndsWithI(const std::wstring& text, const wchar_t* suffix);
std::string Lower(std::string text);

}  // namespace gazeora::app
