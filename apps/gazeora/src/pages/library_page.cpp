// App: the library grid, its game cards and their context menu.
#include "app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

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

void App::DrawLibrary() {
    // Header: title + count on the left; search, sort and Add on the right.
    float headerY = ImGui::GetCursorPosY();
    // Controls are a frame (with S(9) vertical padding) tall; the texts center on them.
    float controlsHeight = m_fonts.body->FontSize + S(18);
    ImGui::SetCursorPosY(headerY + (controlsHeight - m_fonts.title->FontSize) * 0.5f);
    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Library");
    ImGui::PopFont();
    ImGui::SameLine(0.0f, S(12));
    ImGui::SetCursorPosY(headerY + (controlsHeight - m_fonts.body->FontSize) * 0.5f);
    char count[32];
    snprintf(count, sizeof(count), "%zu %s", m_library.Games().size(),
             m_library.Games().size() == 1 ? "game" : "games");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted(count);
    ImGui::PopStyleColor();

    float right = ImGui::GetContentRegionMax().x;
    float addWidth = S(130), sortWidth = S(160), searchWidth = S(220), spacing = S(8);
    float controlsY = headerY;
    float x = right - addWidth - sortWidth - searchWidth - spacing * 2;

    ImGui::SetCursorPos(ImVec2(x, controlsY));
    ImGui::SetNextItemWidth(searchWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(12), S(9)));
    ImGui::InputTextWithHint("##search", ICON_SEARCH "  Search games", &m_search);
    TypeOnSelect();

    ImGui::SetCursorPos(ImVec2(x + searchWidth + spacing, controlsY));
    ImGui::SetNextItemWidth(sortWidth);
    const char* sortNames[] = {"Recently played", "Name", "Recently added"};
    int sort = int(m_sort);
    if (ImGui::Combo("##sort", &sort, sortNames, IM_ARRAYSIZE(sortNames))) {
        m_sort = Sort(sort);
    }
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(right - addWidth, controlsY));
    if (AccentButton(ICON_ADD "   Add game##library", ImVec2(addWidth, controlsHeight),
                     m_fonts.bold, S(6), AccentGradient())) {
        m_deferred.push_back([this] { AddGamesFromDialog(); });
    }
    ImGui::SetCursorPosY(headerY + controlsHeight + S(20));

    if (m_library.Games().empty()) {
        DrawEmptyLibrary();
        return;
    }
    std::vector<const Game*> games = SortedGames(m_sort, m_search);
    if (games.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::Text("No games match \"%s\"", m_search.c_str());
        ImGui::PopStyleColor();
        return;
    }

    float gap = S(16);
    float avail = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, int((avail + gap) / (S(160) + gap)));
    float cardWidth = (avail - gap * float(columns - 1)) / float(columns);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, S(18)));
    for (size_t i = 0; i < games.size(); ++i) {
        if (i % size_t(columns) != 0) {
            ImGui::SameLine(0.0f, gap);
        }
        DrawGameCard(*games[i], cardWidth);
    }
    ImGui::PopStyleVar();
}

void App::DrawGameCard(const Game& game, float width) {
    ImGui::PushID(int(game.id));
    float coverHeight = std::round(width * 4.0f / 3.0f);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(width, coverHeight + S(48));
    // Clicked, or pressed with a controller: then it's focused, and its menu button opens the
    // menu a right-click does.
    bool pressed = NavButton("##card", size);
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (game.id == m_returnToGame && m_panes.FocusHereOnEntry()) {
        m_returnToGame = 0;  // back from its page: focus lands here again, once
    }
    if (ImGui::IsItemFocused() && m_gamepadPresses.Pressed(input::GamepadButton::Start)) {
        ImGui::OpenPopup("##context");
    }
    DrawGameContextMenu(game);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 coverMax = pos + ImVec2(width, coverHeight);
    float rounding = S(8);
    const Texture& art = ArtFor(game);
    bool isCover = !game.coverPath.empty() && m_textures.Image(game.coverPath);

    if (isCover) {
        ImVec2 uv0, uv1;
        CoverUv(art.Aspect(), width / coverHeight, uv0, uv1);
        draw->AddImageRounded(ImTextureID(art.Id()), pos, coverMax, uv0, uv1, IM_COL32_WHITE,
                              rounding);
    } else {
        // Icon over a gradient in the icon's own color.
        float rgb[3];
        if (art) {
            std::copy(art.averageColor, art.averageColor + 3, rgb);
        } else {
            NameColor(game.name, rgb);
        }
        draw->AddRectFilled(pos, coverMax, Shade(rgb, 0.22f), rounding);
        draw->AddImageRounded(ImTextureID(m_textures.Gradient(true).Id()), pos, coverMax,
                              ImVec2(0, 0), ImVec2(1, 1), Shade(rgb, 0.55f, 0.85f), rounding);
        // Exes with only small icons get scaled less, so they don't turn to mush.
        float iconSize = std::min(width * 0.44f, S(96));
        if (art) {
            iconSize = std::min(iconSize, std::max(S(64), float(art.width) * 2.5f));
        }
        ImVec2 iconPos = pos + ImVec2((width - iconSize) * 0.5f, (coverHeight - iconSize) * 0.42f);
        if (art) {
            draw->AddImage(ImTextureID(art.Id()), iconPos, iconPos + ImVec2(iconSize, iconSize));
        } else {
            std::string initial = U8(game.name.substr(0, 1));
            ImVec2 textSize = m_fonts.hugeIcon->CalcTextSizeA(m_fonts.hugeIcon->FontSize, FLT_MAX,
                                                              0.0f, initial.c_str());
            draw->AddText(m_fonts.hugeIcon, m_fonts.hugeIcon->FontSize,
                          iconPos + (ImVec2(iconSize, iconSize) - textSize) * 0.5f, kText,
                          initial.c_str());
        }
    }

    // Running badge.
    GameSession* session = SessionFor(game.id);
    if (session) {
        const char* label = SessionLabel(session->Snapshot().state);
        ImVec2 textSize =
            m_fonts.caption->CalcTextSizeA(m_fonts.caption->FontSize, FLT_MAX, 0.0f, label);
        ImVec2 badge = pos + ImVec2(S(10), S(10));
        draw->AddRectFilled(badge, badge + textSize + ImVec2(S(16), S(6)), kAccent, S(4));
        draw->AddText(m_fonts.caption, m_fonts.caption->FontSize, badge + ImVec2(S(8), S(3)),
                      kOnAccent, label);
    }

    // Hover: accent outline, dim the art, and a round play button in the middle.
    bool overPlay = false;
    // The mouse's hover: outline, dimmed art and a play button. A controller's focus has its
    // own edge (FocusEdge), and pressing opens the game's page, as in Steam.
    bool byMouse = !ImGui::GetIO().NavVisible;
    if (hovered && byMouse) {
        draw->AddRectFilled(pos, coverMax, WithAlpha(kSidebarBg, 0.45f), rounding);
        draw->AddRect(pos, coverMax, kAccent, rounding, 0, S(2));
        ImVec2 center = pos + ImVec2(width * 0.5f, coverHeight * 0.5f);
        float radius = S(24);
        ImVec2 mouse = ImGui::GetIO().MousePos;
        ImVec2 d = mouse - center;
        overPlay = !session && d.x * d.x + d.y * d.y <= radius * radius;
        if (!session) {
            draw->AddCircleFilled(center, radius, overPlay ? kAccentHover : kAccent, 48);
            float t = radius * 0.42f;
            draw->AddTriangleFilled(center + ImVec2(-t * 0.7f, -t), center + ImVec2(-t * 0.7f, t),
                                    center + ImVec2(t * 1.05f, 0), kOnAccent);
        }
    }

    TextEllipsis(draw, m_fonts.bold, ImVec2(pos.x + S(2), coverMax.y + S(8)),
                 hovered ? kText : WithAlpha(kText, 0.9f), U8(game.name), width - S(4));
    std::string subtitle = session ? "Playing now" : RelativeTime(game.lastPlayed);
    draw->AddText(m_fonts.caption, m_fonts.caption->FontSize,
                  ImVec2(pos.x + S(2), coverMax.y + S(28)), session ? kAccent : kTextDim,
                  subtitle.c_str());

    if (pressed) {
        if (overPlay) {
            Launch(game.id);
        } else {
            OpenDetails(game.id);
        }
    }
    ImGui::PopID();
}

void App::DrawGameContextMenu(const Game& game) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(8), S(8)));
    bool open = ImGui::BeginPopupContextItem("##context");
    ImGui::PopStyleVar();
    if (!open) {
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8), S(10)));
    bool running = SessionFor(game.id) != nullptr;
    if (ImGui::MenuItem(ICON_PLAY "   Play", nullptr, false, !running)) {
        Launch(game.id);
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_INFO "   Details")) {
        OpenDetails(game.id);
    }
    if (ImGui::MenuItem(ICON_FOLDER "   Open file location")) {
        ShowInExplorer(game.exePath);
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
    if (ImGui::MenuItem(ICON_DELETE "   Remove from library")) {
        m_pendingRemoveId = game.id;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndPopup();
}

}  // namespace gazeora::app
