#include "gazeora/library/game_library.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

#include "gazeora/library/exceptions/library_storage_error.h"

namespace gazeora {

namespace {

constexpr int kDecimal = 10;

bool IsNumber(const std::wstring& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), iswdigit);
}

}  // namespace

GameLibrary::GameLibrary(IGameRepository& repository, const IClock& clock,
                         const IGameNameResolver& names)
    : m_repository(repository), m_clock(clock), m_names(names) {}

bool GameLibrary::Load(std::wstring& error) {
    m_games.clear();
    try {
        m_games = m_repository.Load();
        return true;
    } catch (const LibraryStorageError& e) {
        error = e.Message();
        return false;
    }
}

bool GameLibrary::Save(std::wstring& error) {
    try {
        m_repository.Save(m_games);
        return true;
    } catch (const LibraryStorageError& e) {
        error = e.Message();
        return false;
    }
}

uint32_t GameLibrary::Add(Game game) {
    game.id = NextId();
    game.addedAt = m_clock.NowUnixSeconds();
    if (game.name.empty()) {
        game.name = m_names.NameFor(game.exePath);
    }
    m_games.push_back(std::move(game));
    return m_games.back().id;
}

bool GameLibrary::Remove(uint32_t id) {
    auto it = std::find_if(m_games.begin(), m_games.end(),
                           [id](const Game& game) { return game.id == id; });
    if (it == m_games.end()) {
        return false;
    }
    m_games.erase(it);
    return true;
}

Game* GameLibrary::Find(uint32_t id) {
    for (Game& game : m_games) {
        if (game.id == id) {
            return &game;
        }
    }
    return nullptr;
}

Game* GameLibrary::FindByIdOrName(const std::wstring& key) {
    Game* game = FindById(key);
    return game ? game : FindUniqueByName(key);
}

// New ids follow the highest existing one, so removed ids aren't reused.
uint32_t GameLibrary::NextId() const {
    uint32_t maxId = 0;
    for (const Game& existing : m_games) {
        maxId = std::max(maxId, existing.id);
    }
    return maxId + 1;
}

Game* GameLibrary::FindById(const std::wstring& key) {
    return IsNumber(key) ? Find(wcstoul(key.c_str(), nullptr, kDecimal)) : nullptr;
}

Game* GameLibrary::FindUniqueByName(const std::wstring& name) {
    Game* match = nullptr;
    for (Game& game : m_games) {
        if (_wcsicmp(game.name.c_str(), name.c_str()) == 0) {
            if (match) {
                return nullptr;  // ambiguous
            }
            match = &game;
        }
    }
    return match;
}

}  // namespace gazeora
