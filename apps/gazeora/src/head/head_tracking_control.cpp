// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "head/head_tracking_control.h"

namespace gazeora::app {

HeadTrackingControl::HeadTrackingControl(HeadLook& look, MouseLookDriver& mouse,
                                         const ControlsSettings& store)
    : m_look(look), m_mouse(mouse), m_store(store) {
    m_look.SetEnabled(store.LoadHeadTrackingEnabled());
    m_mouse.SetCountsPerDegree(
        store.LoadHeadCountsPerDegree(MouseLookDriver::kDefaultCountsPerDegree));
}

void HeadTrackingControl::SetEnabled(bool enabled) {
    m_look.SetEnabled(enabled);
    m_store.SaveHeadTrackingEnabled(enabled);
}

std::wstring HeadTrackingControl::OverlayStatus() const {
    if (!Enabled()) {
        return {};
    }
    return GameInFront() ? L"turning the game" : L"no game in front";
}

void HeadTrackingControl::SaveCountsPerDegree() const {
    m_store.SaveHeadCountsPerDegree(m_mouse.CountsPerDegree());
}

}  // namespace gazeora::app
