// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::hook {

void SetStatus(const char* fmt, ...) {
    char buf[sizeof(SharedHeader::status)];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    OutputDebugStringA("[gazeora_hook] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
    if (SharedHeader* shared = Shared()) {
        memcpy(shared->status, buf, sizeof(buf));
    }
}

}  // namespace gazeora::hook
