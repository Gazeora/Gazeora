#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "gazeora/library/interfaces/game_repository.h"

namespace gazeora {

// Repository pattern: the games as a JSON file, replaced atomically on save.
class JsonGameRepository final : public IGameRepository {
public:
    explicit JsonGameRepository(std::wstring path);

    std::vector<Game> Load() override;
    void Save(const std::vector<Game>& games) override;
    bool ChangedSinceLastAccess() const override;
    const std::wstring& Location() const override { return m_path; }

private:
    std::optional<std::string> ReadFile() const;
    void WriteFileAtomically(const std::string& text) const;
    uint64_t FileWriteTime() const;

    std::wstring m_path;
    uint64_t m_accessedWriteTime = 0;
};

}  // namespace gazeora
