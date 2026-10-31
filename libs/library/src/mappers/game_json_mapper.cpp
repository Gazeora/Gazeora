// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "mappers/game_json_mapper.h"

#include "gazeora/common/text/utf8.h"
#include "mappers/launch_via_names.h"

using nlohmann::json;

namespace gazeora {

json GameJsonMapper::ToJson(const Game& game) const {
    return json{
        {"id", game.id},
        {"name", Utf8::FromWide(game.name)},
        {"exePath", Utf8::FromWide(game.exePath)},
        {"arguments", Utf8::FromWide(game.arguments)},
        {"workingDir", Utf8::FromWide(game.workingDir)},
        {"coverPath", Utf8::FromWide(game.coverPath)},
        {"captureOnLaunch", game.captureOnLaunch},
        {"launchVia", LaunchViaNames::NameOf(game.launchVia)},
        {"processName", Utf8::FromWide(game.processName)},
        {"addedAt", game.addedAt},
        {"lastPlayed", game.lastPlayed},
        {"playtimeSeconds", game.playtimeSeconds},
    };
}

Game GameJsonMapper::FromJson(const json& entry) const {
    Game game;
    game.id = entry.value("id", 0u);
    game.name = Utf8::ToWide(entry.value("name", ""));
    game.exePath = Utf8::ToWide(entry.value("exePath", ""));
    game.arguments = Utf8::ToWide(entry.value("arguments", ""));
    game.workingDir = Utf8::ToWide(entry.value("workingDir", ""));
    game.coverPath = Utf8::ToWide(entry.value("coverPath", ""));
    game.captureOnLaunch = entry.value("captureOnLaunch", false);
    game.launchVia = LaunchViaNames::FromName(entry.value("launchVia", "auto"));
    game.processName = Utf8::ToWide(entry.value("processName", ""));
    game.addedAt = entry.value("addedAt", int64_t(0));
    game.lastPlayed = entry.value("lastPlayed", int64_t(0));
    game.playtimeSeconds = entry.value("playtimeSeconds", int64_t(0));
    return game;
}

}  // namespace gazeora
