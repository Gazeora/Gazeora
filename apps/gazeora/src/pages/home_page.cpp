// App: the home page, and what it shows while the library is empty.
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

void App::DrawHome() {
    if (m_library.Games().empty()) {
        DrawEmptyLibrary();
        return;
    }

    // Feature the running game, else the last one played, else the newest.
    const Game* featured = nullptr;
    const char* eyebrow = "READY TO PLAY";
    for (const auto& session : m_sessions) {
        if (!session->Finished()) {
            featured = m_library.Find(session->GameId());
            eyebrow = "NOW PLAYING";
        }
    }
    if (!featured) {
        std::vector<const Game*> recent = SortedGames(Sort::RecentlyPlayed, "");
        featured = recent.front();
        eyebrow = featured->lastPlayed > 0 ? "CONTINUE PLAYING" : "RECENTLY ADDED";
    }

    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Home");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(8)));
    DrawGameBanner(*featured, S(260), eyebrow, true);
    ImGui::Dummy(ImVec2(0, S(22)));

    // One row of recent games, as many as fit.
    ImGui::PushFont(m_fonts.heading);
    ImGui::TextUnformatted("Your games");
    ImGui::PopFont();
    float viewAllWidth =
        m_fonts.body->CalcTextSizeA(m_fonts.body->FontSize, FLT_MAX, 0.0f, "View all").x;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - viewAllWidth);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                         (m_fonts.heading->FontSize - m_fonts.body->FontSize) * 0.5f);
    if (LinkButton("View all##home", m_fonts.body, kAccent, kAccentHover)) {
        m_page = Page::Library;
    }
    ImGui::Dummy(ImVec2(0, S(6)));

    float gap = S(16);
    float avail = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, int((avail + gap) / (S(160) + gap)));
    float cardWidth = (avail - gap * float(columns - 1)) / float(columns);
    std::vector<const Game*> games = SortedGames(Sort::RecentlyPlayed, "");
    for (int i = 0; i < columns && i < int(games.size()); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, gap);
        }
        DrawGameCard(*games[i], cardWidth);
    }
}

void App::DrawEmptyLibrary() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float blockHeight = S(220);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (avail.y - blockHeight) * 0.4f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float centerX = ImGui::GetCursorScreenPos().x + avail.x * 0.5f;
    float y = ImGui::GetCursorScreenPos().y;

    float circle = S(44);
    draw->AddCircleFilled(ImVec2(centerX, y + circle), circle, kCardBg);
    ImVec2 iconSize = m_fonts.hugeIcon->CalcTextSizeA(m_fonts.hugeIcon->FontSize, FLT_MAX, 0.0f,
                                                      ICON_GAME);
    draw->AddText(m_fonts.hugeIcon, m_fonts.hugeIcon->FontSize,
                  ImVec2(centerX - iconSize.x * 0.5f, y + circle - iconSize.y * 0.5f - S(4)),
                  kAccent, ICON_GAME);
    y += circle * 2 + S(20);

    auto centered = [&](ImFont* font, ImU32 color, const char* text) {
        ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
        draw->AddText(font, font->FontSize, ImVec2(centerX - size.x * 0.5f, y), color, text);
        y += size.y + S(6);
    };
    centered(m_fonts.heading, kText, "Your library is empty");
    centered(m_fonts.body, kTextDim, "Add an .exe or shortcut, or drop one onto this window.");
    y += S(14);

    ImVec2 buttonSize(S(150), S(38));
    ImGui::SetCursorScreenPos(ImVec2(centerX - buttonSize.x * 0.5f, y));
    if (AccentButton(ICON_ADD "   Add game##empty", buttonSize, m_fonts.bold, S(6),
                     AccentGradient())) {
        m_deferred.push_back([this] { AddGamesFromDialog(); });
    }
}


}  // namespace gazeora::app
