#pragma once

#include <string>

namespace gazeora {

// Picks a name for a game the user added without one.
class IGameNameResolver {
public:
    virtual ~IGameNameResolver() = default;

    virtual std::wstring NameFor(const std::wstring& exePath) const = 0;
};

}  // namespace gazeora
