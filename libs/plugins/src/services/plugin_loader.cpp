// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugins/services/plugin_loader.h"

#include "gazeora/plugin_api/gazeora_plugin.h"
#include "gazeora/plugin_kit/utils/plugin_text.h"
#include "services/plugin_registrar.h"

namespace gazeora {

namespace {

constexpr wchar_t kErrorSeparator[] = L"; ";

std::wstring FileName(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring Joined(const std::vector<std::wstring>& texts) {
    std::wstring joined;
    for (const std::wstring& text : texts) {
        joined += (joined.empty() ? L"" : kErrorSeparator) + text;
    }
    return joined;
}

}  // namespace

void PluginLoader::Count(const PluginCatalog& registered, PluginStatus& status) {
    status.gameStores = int(registered.gameStores.size());
    status.encoders = int(registered.encoderProviders.size());
    status.codecs = int(registered.codecs.size());
    status.protocols = int(registered.protocols.size());
    status.gamepadDrivers = int(registered.gamepadDrivers.size());
    status.uiPanels = int(registered.uiPanels.size());
}

PluginCatalog PluginLoader::Load(const std::vector<std::wstring>& paths) const {
    PluginCatalog catalog;
    for (const std::wstring& path : paths) {
        catalog.Append(LoadOne(path));
    }
    return catalog;
}

PluginCatalog PluginLoader::LoadOne(const std::wstring& path) const {
    PluginCatalog catalog;
    PluginStatus status;
    status.path = path;
    status.name = FileName(path);
    std::shared_ptr<const IPluginModule> module = m_modules.Load(path, status.error);
    if (module && ReadInfo(*module, status)) {
        auto registerPlugin = reinterpret_cast<GazeoraPluginRegisterFn>(
            module->Symbol(GAZEORA_PLUGIN_REGISTER_SYMBOL));
        PluginRegistrar registrar(module, m_settingsFolder);
        const GazeoraPluginHostV3 host = registrar.Host();
        status.loaded = registerPlugin(&host.v2.v1) == GAZEORA_TRUE;
        status.error =
            status.loaded ? Joined(registrar.Refusals()) : L"the plugin stopped while registering";
        if (status.loaded) {
            catalog = registrar.Registered();
            Count(catalog, status);
        }
    }
    catalog.plugins.push_back(std::move(status));
    return catalog;
}

bool PluginLoader::ReadInfo(const IPluginModule& module, PluginStatus& status) {
    auto info = reinterpret_cast<GazeoraPluginInfoFn>(module.Symbol(GAZEORA_PLUGIN_INFO_SYMBOL));
    if (!info || !module.Symbol(GAZEORA_PLUGIN_REGISTER_SYMBOL)) {
        status.error = L"not a Gazeora plugin: it doesn't export " +
                       Utf8::ToWide(GAZEORA_PLUGIN_INFO_SYMBOL) + L" and " +
                       Utf8::ToWide(GAZEORA_PLUGIN_REGISTER_SYMBOL);
        return false;
    }
    const GazeoraPluginInfoV1* about = info();
    if (!about || about->struct_size < sizeof(GazeoraPluginInfoV1)) {
        status.error = L"the plugin gave no information about itself";
        return false;
    }
    if (about->name && *about->name) {
        status.name = PluginText::Wide(about->name);
    }
    status.version = PluginText::Wide(about->version);
    if (about->api_version < GAZEORA_PLUGIN_API_OLDEST_VERSION ||
        about->api_version > GAZEORA_PLUGIN_API_VERSION) {
        status.error = L"built for plugin API version " + std::to_wstring(about->api_version) +
                       L"; this Gazeora supports versions " +
                       std::to_wstring(GAZEORA_PLUGIN_API_OLDEST_VERSION) + L" to " +
                       std::to_wstring(GAZEORA_PLUGIN_API_VERSION);
        return false;
    }
    return true;
}

}  // namespace gazeora
