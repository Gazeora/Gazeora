// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "pages/plugin_card.h"

#include <algorithm>

#include "gazeora/common/text/utf8.h"
#include "pages/plugin_card_text.h"
#include "pages/plugin_category_look.h"
#include "platform/shell.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

constexpr float kPadX = 18.0f;
constexpr float kPadY = 16.0f;
constexpr float kSpacingX = 8.0f;
constexpr float kSpacingY = 6.0f;
constexpr float kHeadingGap = 4.0f;
constexpr float kTileRounding = 10.0f;
constexpr float kIconSize = 40.0f;
constexpr float kIconRounding = 9.0f;
constexpr float kButtonHeight = 32.0f;
constexpr float kButtonRounding = 6.0f;
constexpr float kBadgePadX = 8.0f;
constexpr float kBadgePadY = 2.0f;

void Wrapped(ImU32 color, const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

}  // namespace

float PluginCard::Draw(const Fonts& fonts, const PluginEntry& entry, ImVec2 size) {
    ImGui::PushID(Utf8::FromWide(entry.id).c_str());
    BeginTile(size);
    const std::vector<Action> actions = ActionsFor(entry);
    DrawHeading(fonts, entry);
    DrawAbout(entry);
    DrawNotes(entry);
    const float buttons = actions.empty() ? 0.0f : S(kButtonHeight) + S(kPadY);
    const float needed = ImGui::GetCursorPosY() + buttons + S(kPadY);
    if (!actions.empty()) {
        const float bottom = ImGui::GetWindowHeight() - S(kPadY) - S(kButtonHeight);
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), bottom));
        DrawActions(fonts, actions);
    }
    EndTile();
    ImGui::PopID();
    return needed;
}

void PluginCard::BeginTile(ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(kTileRounding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(kPadX), S(kPadY)));
    ImGui::BeginChild("##plugin", size,
                      ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(kSpacingX), S(kSpacingY)));
}

void PluginCard::EndTile() {
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

std::vector<PluginCard::Action> PluginCard::ActionsFor(const PluginEntry& entry) const {
    const auto change = m_control.ChangeOf(entry.id);
    const bool installed = change ? change == PluginChange::Installed : entry.installed.has_value();
    const bool idle = !m_control.Busy();
    const bool broken = entry.installed && !entry.installed->loaded && !change;
    std::vector<Action> actions;
    if (!installed && entry.Installable()) {
        actions.push_back({"Install", true, idle, [this, entry] { m_control.Install(entry); }});
    } else if (installed && entry.Installable() && (entry.UpdateAvailable() || broken)) {
        actions.push_back({entry.UpdateAvailable() ? "Update" : "Reinstall", true, idle,
                           [this, entry] { m_control.Install(entry); }});
    }
    if (installed) {
        actions.push_back(
            {"Uninstall", false, idle, [this, entry] { m_control.Uninstall(entry); }});
    }
    if (entry.listed && !entry.listed->homepage.empty()) {
        const std::string url = entry.listed->homepage;
        actions.push_back({"Website", false, true, [url] { OpenUrl(url); }});
    }
    return actions;
}

void PluginCard::DrawHeading(const Fonts& fonts, const PluginEntry& entry) {
    DrawIcon(fonts, PluginCategoryLook::Icon(entry.Category()));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(fonts.bold);
    ImGui::TextUnformatted(U8(entry.Name()).c_str());
    ImGui::PopFont();
    const std::string version = PluginCardText::Version(entry);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextFaint);
    ImGui::TextUnformatted(version.empty() ? " " : version.c_str());
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0.0f, S(kHeadingGap)));
    DrawBadges(fonts, entry);
}

void PluginCard::DrawBadges(const Fonts& fonts, const PluginEntry& entry) {
    DrawBadge(fonts, PluginCategoryLook::Name(entry.Category()), kSecondary, kTextDim);
    if (entry.Bundled()) {
        ImGui::SameLine();
        DrawBadge(fonts, "Comes with Gazeora", kAccentSoft, kAccentHover);
    }
}

void PluginCard::DrawIcon(const Fonts& fonts, const char* icon) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(S(kIconSize), S(kIconSize));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), kAccentSoft, S(kIconRounding));
    ImFont* font = fonts.heading;
    const ImVec2 glyph = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, icon);
    draw->AddText(font, font->FontSize,
                  ImVec2(pos.x + (size.x - glyph.x) * 0.5f, pos.y + (size.y - glyph.y) * 0.5f),
                  kAccentHover, icon);
    ImGui::Dummy(size);
}

void PluginCard::DrawBadge(const Fonts& fonts, const char* text, ImU32 background, ImU32 color) {
    ImFont* font = fonts.caption;
    const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
    const ImVec2 pad(S(kBadgePadX), S(kBadgePadY));
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size.x + pad.x * 2.0f, min.y + size.y + pad.y * 2.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max, background, (max.y - min.y) * 0.5f);
    draw->AddText(font, font->FontSize, ImVec2(min.x + pad.x, min.y + pad.y), color, text);
    ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
}

void PluginCard::DrawAbout(const PluginEntry& entry) {
    if (entry.listed && !entry.listed->summary.empty()) {
        Wrapped(kTextDim, U8(entry.listed->summary));
    }
    const std::string facts = PluginCardText::Facts(entry);
    if (!facts.empty()) {
        Wrapped(kTextFaint, facts);
    }
}

void PluginCard::DrawNotes(const PluginEntry& entry) {
    const auto change = m_control.ChangeOf(entry.id);
    if (change == PluginChange::Installed) {
        Wrapped(kSuccess, ICON_CHECK "  Installed. Restart Gazeora to load it.");
    } else if (change == PluginChange::Removed) {
        Wrapped(kWarning, "Uninstalled. Restart Gazeora to unload it.");
    } else if (entry.installed && !entry.installed->error.empty()) {
        Wrapped(kWarning, ICON_WARNING "  " + U8(entry.installed->error));
    }
    if (entry.listed && !entry.Installable()) {
        Wrapped(kTextFaint, entry.listed->download
                                ? "Made for another plugin API version than this Gazeora's."
                                : "No build of it for this release's architecture.");
    }
}

void PluginCard::DrawActions(const Fonts& fonts, const std::vector<Action>& actions) {
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float count = float(actions.size());
    const ImVec2 size((ImGui::GetContentRegionAvail().x - spacing * (count - 1.0f)) / count,
                      S(kButtonHeight));
    for (size_t i = 0; i < actions.size(); ++i) {
        const Action& action = actions[i];
        if (i > 0) {
            ImGui::SameLine();
        }
        const bool pressed =
            action.primary ? FilledButton(action.label.c_str(), size, kAccent, kAccentHover,
                                          kOnAccent, fonts.bold, S(kButtonRounding), action.enabled)
                           : FilledButton(action.label.c_str(), size, kSecondary, kSecondaryHover,
                                          kText, fonts.bold, S(kButtonRounding), action.enabled);
        if (pressed) {
            action.run();
        }
    }
}

}  // namespace gazeora::app
