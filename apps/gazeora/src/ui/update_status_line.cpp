// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "ui/update_status_line.h"

#include "ui/theme.h"

namespace gazeora::app {

UpdateStatusLine::UpdateStatusLine(const UpdateControl& control) : m_color(kTextDim) {
    Describe(control);
}

void UpdateStatusLine::Describe(const UpdateControl& control) {
    const std::string current = control.Current().Label();
    switch (control.State()) {
        case UpdateState::Idle:
            m_text = "You have Gazeora " + current + ". Not checked for updates yet.";
            m_color = kTextFaint;
            return;
        case UpdateState::Checking:
            m_text = ICON_REFRESH "  Checking for updates...";
            return;
        case UpdateState::UpToDate:
            m_text = ICON_CHECK "  Gazeora " + current + " is up to date";
            m_color = kSuccess;
            return;
        case UpdateState::Available:
            m_text = "Gazeora " + UpdateLabel(control) + " is available. You have " + current + ".";
            m_color = kAccent;
            return;
        case UpdateState::Downloading:
            m_text = "Downloading Gazeora " + UpdateLabel(control) + "...";
            return;
        case UpdateState::Installing:
            m_text = "Starting the installer. Gazeora closes so it can be updated.";
            return;
        case UpdateState::Failed:
            m_text = ICON_WARNING "  " + control.Error();
            m_color = kDanger;
            return;
    }
}

std::string UpdateStatusLine::UpdateLabel(const UpdateControl& control) const {
    return control.Update() ? control.Update()->Version().Label() : std::string();
}

}  // namespace gazeora::app
