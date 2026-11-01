// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include "gazeora/host/stream/loss_report_sink.h"
#include "gazeora/plugin_api/gazeora_plugin.h"

namespace gazeora {

// Adapter pattern: the LossReportSink an output in a bundled plugin reports to during one
// send, handing each report to Gazeora's loss reporter.
class LossForwarder final : public LossReportSink {
public:
    // `reporter` outlives the forwarder: both last one send.
    explicit LossForwarder(const GazeoraLossReporterV2& reporter) : m_reporter(reporter) {}

    void Report(uint64_t source, float fractionLost, uint64_t) override {
        if (m_reporter.report) {
            m_reporter.report(m_reporter.context, source, fractionLost);
        }
    }

private:
    const GazeoraLossReporterV2& m_reporter;
};

}  // namespace gazeora
