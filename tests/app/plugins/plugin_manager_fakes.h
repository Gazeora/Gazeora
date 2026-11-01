// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gazeora/update/interfaces/http_client.h"
#include "plugin_manager/exceptions/plugin_manager_error.h"
#include "plugin_manager/interfaces/plugin_index.h"
#include "plugins/plugin_activator.h"
#include "plugins/plugin_installer.h"

namespace gazeora::app {

// What every fake download writes.
inline constexpr char kBytes[] = "plugin bytes";

class FakeIndex final : public IPluginIndex {
public:
    std::vector<PluginListing> Fetch() override {
        ++fetches;
        if (fail) {
            throw PluginManagerError("offline");
        }
        return listings;
    }
    std::vector<PluginListing> listings;
    bool fail = false;
    int fetches = 0;
};

// Writes kBytes for every download.
class FakeHttp final : public IHttpClient {
public:
    std::string Get(const std::string&) override { return {}; }
    void Download(const std::string&, const std::wstring& path, const DownloadProgress&) override {
        std::ofstream(std::filesystem::path(path), std::ios::binary) << kBytes;
    }
};

class FakeInstaller final : public IPluginInstaller {
public:
    bool Install(const std::wstring& source, const std::wstring& id, const std::string& sha256,
                 std::wstring& error) override {
        installed.push_back(id);
        sources.push_back(source);
        hashes.push_back(sha256);
        error = refuse ? L"no permission" : L"";
        return !refuse;
    }
    bool Uninstall(const std::wstring& fileName, std::wstring& error) override {
        removed.push_back(fileName);
        error = refuse ? L"no permission" : L"";
        return !refuse;
    }
    std::vector<std::wstring> installed, sources, removed;
    std::vector<std::string> hashes;
    bool refuse = false;
};

// Puts changes into effect at once only when `live` is set; otherwise they wait for a restart.
class FakeActivator final : public IPluginActivator {
public:
    bool Activate(const std::wstring& fileName) override {
        activated.push_back(fileName);
        return live;
    }
    bool Deactivate(const std::wstring& fileName) override {
        deactivated.push_back(fileName);
        return live;
    }
    std::vector<std::wstring> activated, deactivated;
    bool live = false;
};

}  // namespace gazeora::app
