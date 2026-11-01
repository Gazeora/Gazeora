// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/launch_via_choices.h"

#include "gazeora/common/text/utf8.h"

namespace gazeora::app {

namespace {

constexpr int kAutoIndex = 0;
constexpr int kDirectIndex = 1;
constexpr char kExeLabel[] = "The game's exe";

}  // namespace

LaunchViaChoices::LaunchViaChoices(const GameStoreRegistry& stores, const Game& game) {
    m_choices.push_back({LaunchVia::Auto, {}, {}, {}});
    m_choices.push_back({LaunchVia::Direct, {}, {}, {}});
    AddStores(stores, game.exePath);
    if (game.launchVia == LaunchVia::Store && !stores.Find(game.storeId)) {
        // Kept, so opening the page doesn't change the game's choice.
        m_choices.push_back(
            {LaunchVia::Store, game.storeId, Utf8::ToWide(game.storeId), {}, false});
    }
    for (const Choice& choice : m_choices) {
        m_labels.push_back(Label(choice));
    }
}

void LaunchViaChoices::AddStores(const GameStoreRegistry& stores, const std::wstring& exePath) {
    for (const auto& store : stores.Stores()) {
        m_choices.push_back(
            {LaunchVia::Store, store->Id(), store->Name(), store->AppIdFor(exePath)});
    }
}

std::string LaunchViaChoices::Label(const Choice& choice) const {
    switch (choice.via) {
        case LaunchVia::Auto:
            return AutoLabel();
        case LaunchVia::Direct:
            return kExeLabel;
        case LaunchVia::Store:
            break;
    }
    const std::string name = Utf8::FromWide(choice.storeName);
    return choice.known ? name : name + " (not in this version)";
}

std::string LaunchViaChoices::AutoLabel() const {
    for (const Choice& choice : m_choices) {
        if (!choice.appId.empty()) {
            return "Automatic (through " + Utf8::FromWide(choice.storeName) + ", app " +
                   Utf8::FromWide(choice.appId) + ")";
        }
    }
    return "Automatic (start the exe)";
}

int LaunchViaChoices::IndexOf(const Game& game) const {
    if (game.launchVia != LaunchVia::Store) {
        return game.launchVia == LaunchVia::Direct ? kDirectIndex : kAutoIndex;
    }
    for (size_t i = 0; i < m_choices.size(); ++i) {
        if (m_choices[i].via == LaunchVia::Store && m_choices[i].storeId == game.storeId) {
            return int(i);
        }
    }
    return kAutoIndex;
}

void LaunchViaChoices::Apply(int index, Game& game) const {
    if (index < 0 || size_t(index) >= m_choices.size()) {
        return;
    }
    game.launchVia = m_choices[index].via;
    game.storeId = m_choices[index].storeId;
}

std::string LaunchViaChoices::Warning(const Game& game) const {
    const Choice& choice = m_choices[IndexOf(game)];
    if (choice.via != LaunchVia::Store || !choice.appId.empty()) {
        return {};
    }
    const std::string name = Utf8::FromWide(choice.storeName);
    return choice.known ? "Not in a " + name + " library, so the exe starts directly"
                        : name + " isn't in this version of Gazeora, so the exe starts directly";
}

}  // namespace gazeora::app
