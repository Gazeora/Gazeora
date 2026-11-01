// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "plugin_ui/gamepad_status_cache.h"

namespace gazeora::app {

const GamepadStatusLine& GamepadStatusCache::Line() {
    Refresh();
    return *m_line;
}

bool GamepadStatusCache::Failed() {
    Refresh();
    return m_failed;
}

void GamepadStatusCache::Refresh() {
    const uint64_t version = m_control.Version();
    const bool paused = m_control.Paused();
    if (m_line && version == m_version && paused == m_paused) {
        return;
    }
    const input::GamepadSnapshot snapshot = m_control.Snapshot();
    m_line.emplace(snapshot, paused);
    m_failed = snapshot.state == input::GamepadSessionState::Error;
    m_version = version;
    m_paused = paused;
}

}  // namespace gazeora::app
