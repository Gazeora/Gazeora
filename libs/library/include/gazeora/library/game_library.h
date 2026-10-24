#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gazeora/library/interfaces/clock.h"
#include "gazeora/library/interfaces/game_name_resolver.h"
#include "gazeora/library/interfaces/game_repository.h"
#include "gazeora/library/models/game.h"

namespace gazeora {

// The user's games: lookups and edits, kept in a repository. Shared by Gazeora.exe and
// GazeoraCLI.exe.
class GameLibrary final {
public:
    GameLibrary(IGameRepository& repository, const IClock& clock, const IGameNameResolver& names);

    // On failure, error says why and the library is empty.
    bool Load(std::wstring& error);
    bool Save(std::wstring& error);

    // True if the store was written by someone else since Load/Save.
    bool ChangedOnDisk() const { return m_repository.ChangedSinceLastAccess(); }

    const std::wstring& Path() const { return m_repository.Location(); }
    const std::vector<Game>& Games() const { return m_games; }

    // Assigns the id and addedAt; fills name from the exe if empty. Returns the new id.
    uint32_t Add(Game game);
    bool Remove(uint32_t id);
    Game* Find(uint32_t id);
    // Accepts an id ("3") or a name (case-insensitive). Returns null if not found or ambiguous.
    Game* FindByIdOrName(const std::wstring& key);

private:
    uint32_t NextId() const;
    Game* FindById(const std::wstring& key);
    Game* FindUniqueByName(const std::wstring& name);

    IGameRepository& m_repository;
    const IClock& m_clock;
    const IGameNameResolver& m_names;
    std::vector<Game> m_games;
};

}  // namespace gazeora
