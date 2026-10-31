// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>

namespace gazeora {

// The wall clock, injected so tests can fix the time.
class IClock {
public:
    virtual ~IClock() = default;

    virtual int64_t NowUnixSeconds() const = 0;
};

}  // namespace gazeora
