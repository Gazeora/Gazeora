// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <atomic>
#include <string>

#include "gazeora/input/interfaces/gamepad_switch.h"

namespace gazeora {

// Repository pattern: the plugin's switch, saved as [game_controller] enabled in controls.ini
// in Gazeora's settings folder, where Gazeora kept it before it was a plugin. On unless saved
// off. Without a folder it isn't saved.
class ControllerSettings final : public input::IGamepadSwitch {
public:
    explicit ControllerSettings(const std::wstring& folder);

    bool Enabled() const override { return m_enabled; }
    void SetEnabled(bool enabled);

private:
    bool Load() const;

    const std::wstring m_path;  // empty without a folder
    std::atomic<bool> m_enabled;
};

}  // namespace gazeora
