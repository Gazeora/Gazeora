// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <stdexcept>
#include <string>

namespace gazeora {

// Checking for, downloading or starting an update failed; the message says why, for the UI.
class UpdateError : public std::runtime_error {
public:
    explicit UpdateError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace gazeora
