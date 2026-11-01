// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "mappers/launch_via_names.h"

namespace gazeora {

namespace {

constexpr char kAuto[] = "auto";
constexpr char kDirect[] = "direct";

}  // namespace

std::string LaunchViaNames::NameOf(const Game& game) {
    switch (game.launchVia) {
        case LaunchVia::Direct:
            return kDirect;
        case LaunchVia::Store:
            return game.storeId.empty() ? kAuto : game.storeId;
        case LaunchVia::Auto:
            break;
    }
    return kAuto;
}

void LaunchViaNames::Apply(const std::string& name, Game& game) {
    game.storeId.clear();
    if (name.empty() || name == kAuto) {
        game.launchVia = LaunchVia::Auto;
    } else if (name == kDirect) {
        game.launchVia = LaunchVia::Direct;
    } else {
        game.launchVia = LaunchVia::Store;
        game.storeId = name;
    }
}

}  // namespace gazeora
