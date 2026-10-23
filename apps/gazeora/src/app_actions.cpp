// App: adding, launching and removing games, and the lookups they need.
#include "app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "gazeora/common/shared.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/launch/injector.h"
#include "platform/shell.h"
#include "settings/stream_settings.h"
#include "ui/logo_frame.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

void App::AddGamesFromDialog() { AddGamePaths(PickFiles(m_hwnd, PickKind::Games)); }

void App::AddGamePaths(const std::vector<std::wstring>& paths) {
    uint32_t lastAdded = 0;
    int added = 0;
    for (const std::wstring& path : paths) {
        Game game;
        if (EndsWithI(path, L".lnk")) {
            if (!m_libraryServices.Shortcuts().Resolve(path, game)) {
                ShowToast("Skipped a shortcut that doesn't point at a program", true);
                continue;
            }
        } else if (EndsWithI(path, L".exe")) {
            game.exePath = path;
        } else {
            ShowToast("Only .exe files and shortcuts can be added", true);
            continue;
        }
        bool duplicate = false;
        for (const Game& existing : m_library.Games()) {
            duplicate = duplicate || (_wcsicmp(existing.exePath.c_str(), game.exePath.c_str()) == 0 &&
                                      existing.arguments == game.arguments);
        }
        if (duplicate) {
            ShowToast(U8(m_libraryServices.Names().NameFor(game.exePath)) + " is already in your library");
            continue;
        }
        lastAdded = m_library.Add(game);
        ++added;
    }
    if (added == 0) {
        return;
    }
    SaveLibrary();
    if (added == 1) {
        ShowToast("Added " + U8(m_library.Find(lastAdded)->name));
        OpenDetails(lastAdded);
    } else {
        ShowToast("Added " + std::to_string(added) + " games");
        m_page = Page::Library;
    }
}


void App::Launch(uint32_t gameId, bool capture) {
    Game* game = m_library.Find(gameId);
    if (!game) {
        return;
    }
    if (SessionFor(gameId)) {
        ShowToast(U8(game->name) + " is already running");
        return;
    }
    m_sessions.push_back(std::make_unique<GameSession>(gameId, m_libraryServices.LaunchSpecs().Create(*game), capture,
                                                       &m_stream));
    game->lastPlayed = m_libraryServices.Clock().NowUnixSeconds();
    SaveLibrary();
    ShowToast("Launching " + U8(game->name) + (capture ? " with capture" : ""));
}

void App::RemoveGame(uint32_t gameId) {
    Game* game = m_library.Find(gameId);
    if (!game) {
        return;
    }
    std::string name = U8(game->name);
    m_library.Remove(gameId);
    SaveLibrary();
    ShowToast("Removed " + name);
    if (m_page == Page::Details && m_detailsId == gameId) {
        m_page = Page::Library;
    }
}

void App::OpenDetails(uint32_t gameId) {
    m_detailsId = gameId;
    m_page = Page::Details;
}

void App::SaveLibrary() {
    std::wstring error;
    if (!m_library.Save(error)) {
        ShowToast(U8(error), true);
    }
}

void App::ShowToast(const std::string& text, bool error) {
    double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    m_toasts.push_back({text, error, now + (error ? 6.0 : 3.0)});
    if (m_toasts.size() > 4) {
        m_toasts.erase(m_toasts.begin());
    }
}

GameSession* App::SessionFor(uint32_t gameId) const {
    for (const auto& session : m_sessions) {
        if (session->GameId() == gameId && !session->Finished()) {
            return session.get();
        }
    }
    return nullptr;
}

uint64_t App::AccentGradient() { return m_textures.Gradient(false).Id(); }

const Texture& App::ArtFor(const Game& game) {
    if (!game.coverPath.empty()) {
        const Texture& cover = m_textures.Image(game.coverPath);
        if (cover) {
            return cover;
        }
    }
    return m_textures.Icon(game.exePath);
}

std::vector<const Game*> App::SortedGames(Sort sort, const std::string& filter) const {
    std::string needle = Lower(filter);
    std::vector<const Game*> games;
    for (const Game& game : m_library.Games()) {
        if (needle.empty() || Lower(U8(game.name)).find(needle) != std::string::npos) {
            games.push_back(&game);
        }
    }
    std::stable_sort(games.begin(), games.end(), [sort](const Game* a, const Game* b) {
        switch (sort) {
            case Sort::Name:
                return _wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
            case Sort::RecentlyAdded:
                return a->addedAt > b->addedAt || (a->addedAt == b->addedAt && a->id > b->id);
            case Sort::RecentlyPlayed:
            default:
                if (a->lastPlayed != b->lastPlayed) {
                    return a->lastPlayed > b->lastPlayed;
                }
                return a->addedAt > b->addedAt || (a->addedAt == b->addedAt && a->id > b->id);
        }
    });
    return games;
}


}  // namespace gazeora::app
