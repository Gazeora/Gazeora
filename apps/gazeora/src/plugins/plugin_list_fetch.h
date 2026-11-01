// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <future>
#include <string>
#include <vector>

#include "plugin_manager/interfaces/plugin_index.h"

namespace gazeora::app {

// The plugin list from the web, fetched on a thread of its own, so plugins can be installed and
// removed while it loads.
class PluginListFetch {
public:
    explicit PluginListFetch(IPluginIndex& index) : m_index(index) {}
    ~PluginListFetch();
    PluginListFetch(const PluginListFetch&) = delete;
    PluginListFetch& operator=(const PluginListFetch&) = delete;

    // Starts a fetch, unless one is running.
    void Start();
    bool Running() const { return m_work.valid(); }
    // On the UI thread: true once a fetch finished, with Listings() or Error() set.
    bool Poll();

    const std::vector<PluginListing>& Listings() const { return m_listings; }
    // Why the last fetch failed, UTF-8; empty if it didn't.
    const std::string& Error() const { return m_error; }

private:
    struct Result {
        std::vector<PluginListing> listings;
        std::string error;
    };

    IPluginIndex& m_index;
    std::future<Result> m_work;
    std::vector<PluginListing> m_listings;
    std::string m_error;
};

}  // namespace gazeora::app
