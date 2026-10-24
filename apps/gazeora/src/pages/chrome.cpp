// App: what surrounds every page: the sidebar and the toasts.
#include "app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iterator>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "gazeora/common/shared.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/launch/injector.h"
#include "platform/shell.h"
#include "settings/stream_settings.h"
#include "ui/logo_frame.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

const App::NavItem App::kNavItems[] = {
    {ICON_HOME, "Home", Page::Home},
    {ICON_LIBRARY, "Library", Page::Library},
    {ICON_HAND, "Controls", Page::Controls},
    {ICON_SETTINGS, "Settings", Page::Settings},
};

void App::DrawSidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kSidebarBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(12), S(20)));
    ImGui::BeginChild("##sidebar", ImVec2(S(212), 0),
                      ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, S(4)));
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Logo: the exe's own icon, then the name.
    ImVec2 pos = ImGui::GetCursorScreenPos() + ImVec2(S(8), 0);
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const Texture& logo = m_textures.Icon(exePath);
    if (logo) {
        draw->AddImage(ImTextureID(logo.Id()), pos, pos + ImVec2(S(28), S(28)));
    }
    draw->AddText(m_fonts.heading, m_fonts.heading->FontSize,
                  pos + ImVec2(S(38), (S(28) - m_fonts.heading->FontSize) * 0.5f - S(1)), kText,
                  "GAZEORA");
    ImGui::Dummy(ImVec2(0, S(44)));

    float width = ImGui::GetContentRegionAvail().x;
    for (const NavItem& item : kNavItems) {
        bool selected =
            m_page == item.page || (item.page == Page::Library && m_page == Page::Details);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 size(width, S(38));
        ImGui::PushID(item.label);
        if (ImGui::InvisibleButton("##nav", size)) {
            m_page = item.page;
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        if (selected || hovered) {
            draw->AddRectFilled(
                p, p + size, selected ? WithAlpha(kAccent, 0.16f) : WithAlpha(kText, 0.04f), S(8));
        }
        if (selected) {
            draw->AddRectFilled(p + ImVec2(0, S(10)), p + ImVec2(S(3), size.y - S(10)), kAccent,
                                S(2));
        }
        ImU32 color = selected ? kText : (hovered ? kText : kTextDim);
        float textY = p.y + (size.y - m_fonts.body->FontSize) * 0.5f;
        draw->AddText(m_fonts.body, m_fonts.body->FontSize, ImVec2(p.x + S(14), textY),
                      selected ? kAccent : color, item.icon);
        draw->AddText(selected ? m_fonts.bold : m_fonts.body, m_fonts.body->FontSize,
                      ImVec2(p.x + S(42), textY), color, item.label);
    }

    // "Now playing" panel at the bottom while a game launched from here is running.
    const GameSession* playing = nullptr;
    for (const auto& session : m_sessions) {
        if (!session->Finished()) {
            playing = session.get();
        }
    }
    if (playing) {
        const Game* game = m_library.Find(playing->GameId());
        GameSession::Stats stats = playing->Snapshot();
        float panelHeight = S(playing->Capture() ? 100 : 80);
        float bottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - S(16);
        ImVec2 p(ImGui::GetCursorScreenPos().x, bottom - panelHeight);
        ImVec2 size(width, panelHeight);
        ImGui::SetCursorScreenPos(p);
        if (ImGui::InvisibleButton("##nowplaying", size) && game) {
            OpenDetails(game->id);
        }
        bool hovered = ImGui::IsItemHovered();
        draw->AddRectFilled(p, p + size, hovered ? kCardHover : kCardBg, S(8));

        ImVec2 inner = p + ImVec2(S(12), S(10));
        bool launching = stats.state != GameSession::State::Running;
        draw->AddCircleFilled(inner + ImVec2(S(4), S(7)), S(3.5f), launching ? kWarning : kAccent);
        draw->AddText(m_fonts.caption, m_fonts.caption->FontSize, inner + ImVec2(S(14), 0),
                      launching ? kWarning : kAccent,
                      launching ? SessionLabel(stats.state) : "NOW PLAYING");
        inner.y += S(20);
        TextEllipsis(draw, m_fonts.bold, inner, kText, game ? U8(game->name) : "Game",
                     size.x - S(24));
        inner.y += S(20);
        draw->AddText(m_fonts.caption, m_fonts.caption->FontSize, inner, kTextDim,
                      (ICON_CLOCK "  " + ClockText(stats.elapsedSeconds)).c_str());
        if (playing->Capture()) {
            inner.y += S(18);
            char text[64];
            if (launching) {
                snprintf(text, sizeof(text), ICON_CAPTURE "  Waiting...");
            } else if (stats.fps > 0.0) {
                snprintf(text, sizeof(text), ICON_CAPTURE "  %.0f fps  %ux%u", stats.fps,
                         stats.width, stats.height);
            } else {
                snprintf(text, sizeof(text), ICON_CAPTURE "  %s", HookStateText(stats.hookState));
            }
            TextEllipsis(draw, m_fonts.caption, inner, kTextDim, text, size.x - S(24));
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void App::FollowGamepadPageKeys() {
    int step = int(ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) -
               int(ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false));
    // Not while a slider is held or a popup is open: the page would change under them.
    if (step == 0 || ImGui::IsAnyItemActive() ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        return;
    }
    Page shown = m_page == Page::Details ? Page::Library : m_page;
    const int count = int(std::size(kNavItems));
    for (int i = 0; i < count; ++i) {
        if (kNavItems[i].page == shown) {
            m_page = kNavItems[std::clamp(i + step, 0, count - 1)].page;
            return;
        }
    }
}

void App::DrawToasts() {
    // Remove confirmation modal (opened from details or a context menu).
    if (m_pendingRemoveId != 0 && !ImGui::IsPopupOpen("Remove game?")) {
        ImGui::OpenPopup("Remove game?");
    }
    ImGui::SetNextWindowSize(ImVec2(S(380), 0));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(20), S(18)));
    if (ImGui::BeginPopupModal(
            "Remove game?", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {
        const Game* game = m_library.Find(m_pendingRemoveId);
        ImGui::PushFont(m_fonts.heading);
        ImGui::TextUnformatted("Remove from library?");
        ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("%s will be removed from Gazeora. The game stays installed.",
                    game ? U8(game->name).c_str() : "This game");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, S(6)));
        float buttonWidth = S(100);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - S(20) - buttonWidth * 2 - S(8));
        if (FilledButton("Cancel##remove", ImVec2(buttonWidth, S(34)), kSecondary, kSecondaryHover,
                         kText, m_fonts.bold, S(6)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_pendingRemoveId = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, S(8));
        if (FilledButton("Remove##remove", ImVec2(buttonWidth, S(34)), kDanger, kDangerHover,
                         IM_COL32_WHITE, m_fonts.bold, S(6))) {
            uint32_t id = m_pendingRemoveId;
            m_deferred.push_back([this, id] { RemoveGame(id); });
            m_pendingRemoveId = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    // Notifications stacked in the bottom-right corner.
    double now = ImGui::GetTime();
    m_toasts.erase(std::remove_if(m_toasts.begin(), m_toasts.end(),
                                  [now](const Toast& toast) { return toast.expiresAt < now; }),
                   m_toasts.end());
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 corner = ImGui::GetMainViewport()->WorkPos + ImGui::GetMainViewport()->WorkSize -
                    ImVec2(S(20), S(20));
    float maxTextWidth = S(340);
    for (auto it = m_toasts.rbegin(); it != m_toasts.rend(); ++it) {
        float fade = float(std::min(1.0, (it->expiresAt - now) / 0.4));
        ImVec2 textSize = m_fonts.body->CalcTextSizeA(m_fonts.body->FontSize, FLT_MAX, maxTextWidth,
                                                      it->text.c_str());
        ImVec2 size = textSize + ImVec2(S(50), S(22));
        ImVec2 pos = corner - size;
        draw->AddRectFilled(pos, pos + size, WithAlpha(kCardHover, 0.97f * fade), S(8));
        draw->AddRectFilled(pos, pos + ImVec2(S(4), size.y),
                            WithAlpha(it->error ? kDanger : kAccent, fade), S(8),
                            ImDrawFlags_RoundCornersLeft);
        draw->AddText(m_fonts.body, m_fonts.body->FontSize, pos + ImVec2(S(16), S(11)),
                      WithAlpha(it->error ? kDanger : kAccent, fade),
                      it->error ? ICON_WARNING : ICON_CHECK);
        draw->AddText(m_fonts.body, m_fonts.body->FontSize, pos + ImVec2(S(38), S(11)),
                      WithAlpha(kText, fade), it->text.c_str(), nullptr, maxTextWidth);
        corner.y -= size.y + S(8);
    }
}

}  // namespace gazeora::app
