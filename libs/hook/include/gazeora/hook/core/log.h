// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::hook {

// Writes a status line to the debugger output and to SharedHeader::status, where
// the host displays it.
void SetStatus(const char* fmt, ...);

}  // namespace gazeora::hook
