// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugins_page.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <vector>

#include "gazeora/common/text/utf8.h"
#include "gazeora/plugin_manager/services/plugin_directory.h"
#include "platform/shell.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kMaxWidth = 1240.0f;
constexpr float kControlHeight = 36.0f;
constexpr float kInputPadX = 12.0f;
constexpr float kRefreshWidth = 140.0f;
constexpr float kFromFileWidth = 170.0f;
constexpr float kRounding = 6.0f;
constexpr float kSectionGap = 10.0f;

void Wrapped(ImU32 color, const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace

void PluginsPage::Draw(const Fonts& fonts) {
    m_control.Refresh(false);  // the list is fetched when the page is first opened
    const float width = std::min(ImGui::GetContentRegionAvail().x, S(kMaxWidth));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
    DrawHeader(fonts);
    ImGui::PopTextWrapPos();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), S(12)));
    DrawToolbar(fonts, width);
    m_filter.Draw(fonts.bold, m_control.Entries());
    DrawStatus();
    if (DrawSection(fonts, "Installed", true, width) == 0) {
        Wrapped(kTextDim, Filtering() ? "No installed plugin matches."
                                      : "No plugins installed yet. Install one below.");
    }
    ImGui::Dummy(ImVec2(0.0f, S(kSectionGap)));
    DrawAvailableNote(DrawSection(fonts, "Available", false, width));
    ImGui::PopStyleVar();
}

void PluginsPage::DrawHeader(const Fonts& fonts) {
    ImGui::PushFont(fonts.title);
    ImGui::TextUnformatted("Plugins");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextWrapped(
        "Add protocols, codecs, game stores and encoders to Gazeora. Plugins run "
        "with Gazeora's permissions, so install only plugins you trust.");
    ImGui::PopStyleColor();
    ImGui::PushFont(fonts.caption);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
    ImGui::TextWrapped("%s", (ICON_FOLDER "  " + U8(m_folder)).c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void PluginsPage::DrawSearch(float width) {
    // As tall as the buttons beside it.
    const float padY = (S(kControlHeight) - ImGui::GetFontSize()) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(kInputPadX), padY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(kRounding));
    ImGui::SetNextItemWidth(width);
    ImGui::InputTextWithHint("##pluginsearch", ICON_SEARCH "  Search plugins", &m_query);
    ImGui::PopStyleVar(2);
    TypeOnSelect();
}

void PluginsPage::DrawToolbar(const Fonts& fonts, float width) {
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float buttons = S(kRefreshWidth) + S(kFromFileWidth) + spacing * 2.0f;
    DrawSearch(std::max(width - buttons, S(kRefreshWidth)));
    ImGui::SameLine();
    if (FilledButton(ICON_REFRESH "  Refresh list##pluginrefresh",
                     ImVec2(S(kRefreshWidth), S(kControlHeight)), kSecondary, kSecondaryHover,
                     kText, fonts.bold, S(kRounding), !m_control.Listing())) {
        m_control.Refresh(true);
    }
    ImGui::SameLine();
    if (FilledButton(ICON_FOLDER "  Install from file##pluginfile",
                     ImVec2(S(kFromFileWidth), S(kControlHeight)), kSecondary, kSecondaryHover,
                     kText, fonts.bold, S(kRounding), !m_control.Busy())) {
        std::vector<std::wstring> picked = PickFiles(m_owner, PickKind::Plugin);
        if (!picked.empty()) {
            m_control.InstallFile(picked[0]);
        }
    }
}

void PluginsPage::DrawStatus() {
    if (m_control.Task() == PluginTask::Installing) {
        Wrapped(kTextDim, "Installing. Windows asks for administrator permission.");
    } else if (m_control.Task() == PluginTask::Removing) {
        Wrapped(kTextDim, "Uninstalling. Windows asks for administrator permission.");
    }
    if (!m_control.Error().empty()) {
        Wrapped(kDanger, ICON_WARNING "  " + m_control.Error());
    }
    if (m_control.RestartNeeded()) {
        Wrapped(kWarning, ICON_INFO "  Restart Gazeora to apply the changes.");
    }
}

int PluginsPage::DrawSection(const Fonts& fonts, const char* title, bool installed, float width) {
    std::vector<const PluginEntry*> shown;
    for (const PluginEntry& entry : m_control.Entries()) {
        if (InInstalledSection(entry) == installed && Shown(entry)) {
            shown.push_back(&entry);
        }
    }
    DrawSectionTitle(fonts, title, int(shown.size()));
    m_grid.Draw({fonts.bold, fonts.caption, fonts.heading}, shown, width);
    return int(shown.size());
}

void PluginsPage::DrawSectionTitle(const Fonts& fonts, const char* title, int count) {
    ImGui::PushFont(fonts.heading);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (count > 0) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
        ImGui::TextUnformatted(std::to_string(count).c_str());
        ImGui::PopStyleColor();
    }
}

void PluginsPage::DrawAvailableNote(int shown) {
    if (m_control.Listing()) {
        Wrapped(kTextFaint, "Loading more plugins from gazeora.github.io...");
    } else if (!m_control.ListError().empty()) {
        Wrapped(kTextFaint, "The plugin list from gazeora.github.io couldn't be loaded (" +
                                m_control.ListError() +
                                "). The plugins that come with Gazeora can still be installed.");
    } else if (shown == 0) {
        Wrapped(kTextDim,
                Filtering() ? "No other plugin matches." : "Every plugin available is installed.");
    }
}

bool PluginsPage::InInstalledSection(const PluginEntry& entry) const {
    const auto change = m_control.ChangeOf(entry.id);
    return change ? change == PluginChange::Installed : entry.installed.has_value();
}

bool PluginsPage::Shown(const PluginEntry& entry) const {
    return m_filter.Matches(entry) && PluginDirectory::Matches(entry, Utf8::ToWide(m_query));
}

bool PluginsPage::Filtering() const { return !m_query.empty() || m_filter.Active(); }

}  // namespace gazeora::app
