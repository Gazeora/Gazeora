// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "ui/gamepad_status_line.h"

#include "ui/text_format.h"
#include "ui/theme.h"

namespace gazeora::app {

namespace {

using input::GamepadSessionState;
using input::GamepadSnapshot;

constexpr char kNameSeparator[] = ", ";

}  // namespace

GamepadStatusLine::GamepadStatusLine(const GamepadSnapshot& snapshot, bool paused)
    : m_color(kTextDim) {
    if (paused) {
        m_text = "Paused while a game runs, so the game gets your controller";
        return;
    }
    Describe(snapshot);
}

void GamepadStatusLine::Describe(const GamepadSnapshot& snapshot) {
    switch (snapshot.state) {
        case GamepadSessionState::Off:
            m_text = "Off";
            m_color = kTextFaint;
            return;
        case GamepadSessionState::Running:
            DescribeConnected(snapshot);
            return;
        case GamepadSessionState::Error:
            m_text = U8(snapshot.message);
            m_color = kDanger;
            return;
    }
}

void GamepadStatusLine::DescribeConnected(const GamepadSnapshot& snapshot) {
    if (snapshot.connected.empty()) {
        m_text = "No controller connected: plug one in or pair it over Bluetooth";
        m_color = kWarning;
        return;
    }
    m_text = ICON_CHECK "  ";
    for (size_t i = 0; i < snapshot.connected.size(); ++i) {
        m_text += (i > 0 ? kNameSeparator : "") + U8(snapshot.connected[i].name);
    }
    m_color = kSuccess;
}

}  // namespace gazeora::app
