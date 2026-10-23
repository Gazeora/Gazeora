#pragma once

#include "gazeora/library/interfaces/shortcut_resolver.h"

namespace gazeora {

// Adapter pattern: resolves .lnk shortcuts through the shell's IShellLink.
class ShellLinkResolver final : public IShortcutResolver {
public:
    bool Resolve(const std::wstring& shortcutPath, Game& game) const override;
};

}  // namespace gazeora
