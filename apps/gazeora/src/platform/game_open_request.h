// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Games to add, from Explorer's "Open with Gazeora": on the command line of a new Gazeora, which
// passes them on to the one already running.
#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

namespace gazeora::app {

class GameOpenRequest {
public:
    // The paths on this process's command line; empty when there are none.
    static GameOpenRequest FromCommandLine();
    // A request another Gazeora sent with SendTo; none if `data` isn't one.
    static std::optional<GameOpenRequest> FromCopyData(const COPYDATASTRUCT& data);

    bool Empty() const { return m_paths.empty(); }
    const std::vector<std::wstring>& Paths() const { return m_paths; }
    // Hands the paths to the Gazeora window `target` (WM_COPYDATA); false if it didn't take them.
    bool SendTo(HWND target) const;

private:
    explicit GameOpenRequest(std::vector<std::wstring> paths) : m_paths(std::move(paths)) {}

    std::vector<std::wstring> m_paths;
};

}  // namespace gazeora::app
