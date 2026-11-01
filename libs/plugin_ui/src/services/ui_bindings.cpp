// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/plugin_ui/services/ui_bindings.h"

namespace gazeora::ui {

std::string UiBindings::Get(const std::string& key) const { return For(key).Get(key); }

void UiBindings::Set(const std::string& key, const std::string& value) { For(key).Set(key, value); }

void UiBindings::Act(const std::string& action) { For(action).Act(action); }

bool UiBindings::IsGazeoras(const std::string& key) { return key.rfind(kGazeoraPrefix, 0) == 0; }

IUiValues& UiBindings::For(const std::string& key) const {
    return IsGazeoras(key) ? m_gazeora : m_plugin;
}

}  // namespace gazeora::ui
