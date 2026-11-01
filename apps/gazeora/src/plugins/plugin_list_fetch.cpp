// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugins/plugin_list_fetch.h"

#include <chrono>
#include <exception>

namespace gazeora::app {

PluginListFetch::~PluginListFetch() {
    if (m_work.valid()) {
        m_work.wait();
    }
}

void PluginListFetch::Start() {
    if (Running()) {
        return;
    }
    m_work = std::async(std::launch::async, [this] {
        Result result;
        try {
            result.listings = m_index.Fetch();
        } catch (const std::exception& e) {
            result.error = e.what();
        }
        return result;
    });
}

bool PluginListFetch::Poll() {
    if (!m_work.valid() || m_work.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return false;
    }
    Result result = m_work.get();
    m_error = std::move(result.error);
    if (m_error.empty()) {
        m_listings = std::move(result.listings);
    }
    return true;
}

}  // namespace gazeora::app
