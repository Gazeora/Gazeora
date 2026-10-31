// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <windows.h>

#include <string>

#include "gazeora/host/capture/controller_turn_channel.h"
#include "head/controller_look_driver.h"
#include "head/head_look.h"
#include "head/mouse_look_driver.h"
#include "head/player_device.h"
#include "settings/controls_settings.h"

namespace gazeora::app {

// The head tracking settings, saved, and applied: the switch, the mouse's sensitivity, and how
// the game turns with its controller's stick. Also how head turns are reaching the game.
class HeadTrackingControl {
public:
    struct Parts {
        HeadLook& look;
        MouseLookDriver& mouse;
        ControllerLookDriver& controller;
        const PlayerDevice& device;
        const ControllerTurnChannel& channel;
    };

    // Applies the saved settings.
    HeadTrackingControl(const Parts& parts, const ControlsSettings& store);

    bool Enabled() const { return m_parts.look.Enabled(); }
    void SetEnabled(bool enabled);
    float CountsPerDegree() const { return m_parts.mouse.CountsPerDegree(); }
    // Applies at once; Save keeps it, once the slider is let go.
    void SetCountsPerDegree(float counts) { m_parts.mouse.SetCountsPerDegree(counts); }
    void SaveCountsPerDegree() const;
    float StickDegreesPerSecond() const { return m_parts.controller.DegreesPerSecond(); }
    float StickDeadZone() const { return m_parts.controller.DeadZone(); }
    // Applies at once; Save keeps them.
    void SetStick(float degreesPerSecond, float deadZone);
    void SaveStick() const;

    bool Receiving() const { return m_parts.look.Receiving(); }
    bool GameInFront() const { return m_parts.look.GameInFront(); }
    // Head turns go through the game's controller, as the player is on it.
    bool OnController() const { return m_parts.device.UsingController(); }
    // How the game reads its controllers, as the hook has seen (ControllerPath bits).
    LONG ControllerPaths() const { return m_parts.channel.Read().paths; }
    // A few words on what head tracking does, for the stream overlay; empty while it's off.
    std::wstring OverlayStatus() const;

private:
    Parts m_parts;
    const ControlsSettings& m_store;
};

}  // namespace gazeora::app
