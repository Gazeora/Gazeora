// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/head_tracking_control.h"

namespace gazeora::app {

HeadTrackingControl::HeadTrackingControl(const Parts& parts, const ControlsSettings& store)
    : m_parts(parts), m_store(store) {
    m_parts.look.SetEnabled(store.LoadHeadTrackingEnabled());
    m_parts.mouse.SetCountsPerDegree(
        store.LoadHeadCountsPerDegree(MouseLookDriver::kDefaultCountsPerDegree));
    m_parts.controller.SetStick(
        store.LoadStickDegreesPerSecond(ControllerLookDriver::kDefaultDegreesPerSecond),
        store.LoadStickDeadZone(ControllerLookDriver::kDefaultDeadZone));
}

void HeadTrackingControl::SetEnabled(bool enabled) {
    m_parts.look.SetEnabled(enabled);
    m_store.SaveHeadTrackingEnabled(enabled);
}

void HeadTrackingControl::SaveCountsPerDegree() const {
    m_store.SaveHeadCountsPerDegree(m_parts.mouse.CountsPerDegree());
}

void HeadTrackingControl::SetStick(float degreesPerSecond, float deadZone) {
    m_parts.controller.SetStick(degreesPerSecond, deadZone);
}

void HeadTrackingControl::SaveStick() const {
    m_store.SaveStickDegreesPerSecond(m_parts.controller.DegreesPerSecond());
    m_store.SaveStickDeadZone(m_parts.controller.DeadZone());
}

std::wstring HeadTrackingControl::OverlayStatus() const {
    if (!Enabled()) {
        return {};
    }
    if (!GameInFront()) {
        return L"no game in front";
    }
    return OnController() ? L"turning the game with the controller"
                          : L"turning the game with the mouse";
}

}  // namespace gazeora::app
