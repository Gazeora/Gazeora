// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <stdexcept>
#include <string>

namespace gazeora {

// Listing, installing or removing a plugin failed; the message (UTF-8) says why, for the UI.
class PluginManagerError : public std::runtime_error {
public:
    explicit PluginManagerError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace gazeora
