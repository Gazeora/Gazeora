// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/library/services/launch_spec_factory.h"

namespace gazeora {

LaunchSpec LaunchSpecFactory::Create(const Game& game) const {
    LaunchSpec spec;
    spec.exePath = game.exePath;
    spec.arguments = game.arguments;
    spec.workingDir = game.workingDir;
    spec.processName = game.processName;
    ResolveStore(game, spec);
    return spec;
}

void LaunchSpecFactory::ResolveStore(const Game& game, LaunchSpec& spec) const {
    std::shared_ptr<const GameStore> store;
    std::wstring appId;
    if (game.launchVia == LaunchVia::Auto) {
        store = m_stores.OwnerOf(game.exePath, appId);
    } else if (game.launchVia == LaunchVia::Store) {
        store = m_stores.Find(game.storeId);
        appId = store ? store->AppIdFor(game.exePath) : std::wstring();
    }
    if (store && !appId.empty()) {
        spec.store = std::move(store);
        spec.storeAppId = std::move(appId);
    }
}

}  // namespace gazeora
