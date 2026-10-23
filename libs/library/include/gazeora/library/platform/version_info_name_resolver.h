#pragma once

#include <string>

#include "gazeora/library/interfaces/game_name_resolver.h"

namespace gazeora {

// Adapter pattern: names a game from its exe's version resource (FileDescription), else
// from the file name.
class VersionInfoNameResolver final : public IGameNameResolver {
public:
    std::wstring NameFor(const std::wstring& exePath) const override;

private:
    std::wstring FileDescription(const std::wstring& exePath) const;
};

}  // namespace gazeora
