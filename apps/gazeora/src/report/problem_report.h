// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>
#include <utility>

namespace gazeora::app {

// What a problem report already knows: the Gazeora version and the PC it runs on.
class ProblemReport final {
public:
    ProblemReport(std::string version, std::string system)
        : m_version(std::move(version)), m_system(std::move(system)) {}

    const std::string& Version() const { return m_version; }
    const std::string& System() const { return m_system; }

private:
    std::string m_version;
    std::string m_system;
};

}  // namespace gazeora::app
