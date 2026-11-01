// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <cstdint>

namespace gazeora {

// Where an output reports the loss its players see, so the stream backs off: the
// BitrateController, or a plugin host's loss reporter.
class LossReportSink {
public:
    virtual ~LossReportSink() = default;

    // `source` identifies a player (its address, or its connection); a newer report from the
    // same one replaces the older.
    virtual void Report(uint64_t source, float fractionLost, uint64_t nowMs) = 0;
};

}  // namespace gazeora
