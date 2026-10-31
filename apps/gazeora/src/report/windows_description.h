// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

namespace gazeora::app {

// This PC's Windows version and processor, and which build of Gazeora runs on it, for a
// problem report: "Windows 10.0.26200, ARM64 PC, x64 build".
std::string DescribeWindows();

}  // namespace gazeora::app
