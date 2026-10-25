// App: a game's details page: banner, session, settings and info cards.
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
#include "gazeora/common/text/utf8.h"
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

void App::DrawGameBanner(const Game& game, float height, const char* eyebrow, bool showDetails) {
    ImGui::PushID("banner");
    ImGui::PushID(int(game.id));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 max = pos + ImVec2(width, height);
    float rounding = S(12);

    const Texture& art = ArtFor(game);
    bool isCover = !game.coverPath.empty() && m_textures.Image(game.coverPath);
    float rgb[3];
    if (art) {
        std::copy(art.averageColor, art.averageColor + 3, rgb);
    } else {
        NameColor(game.name, rgb);
    }

    // Background: the cover (or the icon's color), fading into the page on the left
    // so the text stays readable.
    draw->AddRectFilled(pos, max, Shade(rgb, 0.3f), rounding);
    if (isCover) {
        ImVec2 uv0, uv1;
        CoverUv(art.Aspect(), width / height, uv0, uv1);
        draw->AddImageRounded(ImTextureID(art.Id()), pos, max, uv0, uv1, IM_COL32_WHITE, rounding);
    } else if (art) {
        float iconSize = std::min(height * 0.72f, std::max(S(128), float(art.width) * 4.0f));
        ImVec2 iconPos(max.x - iconSize - height * 0.16f, pos.y + (height - iconSize) * 0.5f);
        draw->AddImage(ImTextureID(art.Id()), iconPos, iconPos + ImVec2(iconSize, iconSize),
                       ImVec2(0, 0), ImVec2(1, 1), WithAlpha(IM_COL32_WHITE, 0.92f));
    }
    draw->AddImageRounded(ImTextureID(m_textures.Gradient(false).Id()), pos,
                          ImVec2(pos.x + width * 0.85f, max.y), ImVec2(0, 0), ImVec2(1, 1),
                          Shade(rgb, 0.12f, 0.97f), rounding, ImDrawFlags_RoundCornersLeft);

    float padding = S(30);
    float textWidth = width * 0.6f;
    GameSession* session = SessionFor(game.id);
    GameSession::Stats stats;
    if (session) {
        stats = session->Snapshot();
    }
    bool starting = session && stats.state != GameSession::State::Running;

    ImVec2 cursor = pos + ImVec2(padding, padding);
    draw->AddText(m_fonts.caption, m_fonts.caption->FontSize, cursor, starting ? kWarning : kAccent,
                  starting ? "STARTING" : eyebrow);
    cursor.y += S(18);
    TextEllipsis(draw, m_fonts.title, cursor, kText, U8(game.name), textWidth);
    cursor.y += m_fonts.title->FontSize + S(6);

    std::string meta = starting  ? ICON_CLOCK "  Starting..."
                       : session ? ICON_CLOCK "  Playing for " + ClockText(stats.elapsedSeconds)
                                 : ICON_CLOCK "  " + RelativeTime(game.lastPlayed) +
                                       "   \xC2\xB7   " + PlaytimeText(game.playtimeSeconds);
    draw->AddText(m_fonts.body, m_fonts.body->FontSize, cursor, WithAlpha(kText, 0.85f),
                  meta.c_str());

    // Buttons along the bottom.
    float buttonHeight = S(40);
    ImVec2 buttons(pos.x + padding, max.y - padding - buttonHeight);
    ImGui::SetCursorScreenPos(buttons);
    const char* playLabel = !session ? ICON_PLAY "   PLAY##play"
                            : stats.state == GameSession::State::Launching ? "LAUNCHING...##play"
                            : stats.state == GameSession::State::WaitingForGame
                                ? "STARTING...##play"
                                : "RUNNING##play";
    if (AccentButton(playLabel, ImVec2(S(150), buttonHeight), m_fonts.bold, S(6), AccentGradient(),
                     !session)) {
        Launch(game.id);
    }
    if (showDetails) {
        ImGui::SameLine(0.0f, S(10));
        if (FilledButton("Details##details", ImVec2(S(110), buttonHeight), WithAlpha(kText, 0.12f),
                         WithAlpha(kText, 0.2f), kText, m_fonts.bold, S(6))) {
            OpenDetails(game.id);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, max.y));
    ImGui::Dummy(ImVec2(width, 0));
    ImGui::PopID();
    ImGui::PopID();
}

void App::DrawDetails() {
    Game* game = m_library.Find(m_detailsId);
    if (!game) {
        m_page = Page::Library;
        return;
    }
    if (m_editId != game->id) {
        m_editId = game->id;
        m_editName = U8(game->name);
        m_editArgs = U8(game->arguments);
        m_editWorkDir = U8(game->workingDir);
        m_editProcess = U8(game->processName);
        m_editSteamAppId = FindSteamAppId(game->exePath);
    }

    if (LinkButton(ICON_BACK "   Library##back", m_fonts.body, kTextDim, kText)) {
        m_page = Page::Library;
    }
    ImGui::Dummy(ImVec2(0, S(8)));
    DrawGameBanner(*game, S(240), SessionFor(game->id) ? "NOW PLAYING" : "GAME", false);
    ImGui::Dummy(ImVec2(0, S(16)));

    float gap = S(16);
    float sideWidth = S(300);
    if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("main", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("side", ImGuiTableColumnFlags_WidthFixed, sideWidth);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), gap));
        DrawSessionCard(*game);
        DrawGameSettingsCard(*game);
        ImGui::PopStyleVar();
        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gap * 0.5f);
        DrawGameInfoCard(*game);
        ImGui::EndTable();
    }
}

void App::DrawSessionCard(const Game& game) {
    BeginCard("##session", m_scale);
    GameSession* session = SessionFor(game.id);
    if (!session) {
        CardTitle(m_fonts.heading, ICON_CAPTURE "  Gazeora capture");
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted("Capture starts when you launch the game.");
        ImGui::PopStyleColor();
        EndCard();
        return;
    }

    GameSession::Stats stats = session->Snapshot();
    CardTitle(m_fonts.heading, ICON_PLAY "  Session");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    if (stats.state == GameSession::State::Launching) {
        ImGui::TextUnformatted(session->Capture() ? "Launching and setting up capture..."
                                                  : "Launching...");
    } else if (stats.state == GameSession::State::WaitingForGame) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("Waiting for %s to start (launched through %s)...",
                    U8(session->GameProcessName()).c_str(),
                    session->ThroughSteam() ? "Steam" : "its launcher");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        if (FilledButton("Stop waiting##cancel", ImVec2(S(140), S(36)), kSecondary, kSecondaryHover,
                         kText, m_fonts.bold, S(6))) {
            session->Cancel();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    } else {
        ImGui::Text("Process %lu   \xC2\xB7   running for %s", stats.pid,
                    ClockText(stats.elapsedSeconds).c_str());
    }
    ImGui::PopStyleColor();

    if (session->Capture() && stats.state == GameSession::State::Running) {
        ImGui::Dummy(ImVec2(0, S(2)));
        struct Tile {
            const char* label;
            std::string value;
        };
        char fps[32], res[32], latency[32];
        snprintf(fps, sizeof(fps), "%.0f", stats.fps);
        snprintf(res, sizeof(res), "%ux%u", stats.width, stats.height);
        snprintf(latency, sizeof(latency), "%.2f ms", stats.handoffMs);
        Tile tiles[] = {{"FPS", fps},
                        {"RESOLUTION", stats.width ? res : "-"},
                        {"GRAPHICS API", stats.api ? GraphicsApiName(stats.api) : "-"},
                        {"DELAY", stats.fps > 0 ? latency : "-"}};
        float gap = S(10);
        float tileWidth = (ImGui::GetContentRegionAvail().x - gap * 3) / 4;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        for (int i = 0; i < 4; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, gap);
            }
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 size(tileWidth, S(60));
            ImGui::Dummy(size);
            draw->AddRectFilled(p, p + size, kInset, S(8));
            draw->AddText(m_fonts.caption, m_fonts.caption->FontSize, p + ImVec2(S(12), S(10)),
                          kTextDim, tiles[i].label);
            draw->AddText(m_fonts.heading, m_fonts.heading->FontSize, p + ImVec2(S(12), S(28)),
                          i == 0 ? kAccent : kText, tiles[i].value.c_str());
        }

        ImU32 stateColor = stats.hookState == kHookError       ? kDanger
                           : stats.hookState == kHookCapturing ? kAccent
                                                               : kWarning;
        ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
        ImGui::Text("%s  %s", stats.hookState == kHookError ? ICON_WARNING : ICON_CHECK,
                    HookStateText(stats.hookState));
        ImGui::PopStyleColor();
        if (!stats.hookStatus.empty()) {
            HelpMarker(stats.hookStatus.c_str(), S(380));
        }
        if (stats.stereoWanted) {
            ImGui::SameLine(0.0f, S(16));
            ImGui::PushStyleColor(ImGuiCol_Text, stats.stereo ? kAccent : kWarning);
            ImGui::TextUnformatted(stats.stereo ? "3D from the game's depth"
                                                : "3D unavailable: streaming in 2D");
            ImGui::PopStyleColor();
            if (!stats.stereo) {
                HelpMarker(
                    "3D needs the game's depth information, which Gazeora finds in D3D11 "
                    "and D3D12 games while they draw a 3D scene. Menus, videos and other "
                    "graphics APIs stay 2D.",
                    S(380));
            }
        }

        std::vector<std::wstring> urls = m_stream.Urls();
        if (FilledButton(ICON_CAPTURE "   Copy stream URL##stream", ImVec2(S(170), S(34)),
                         kSecondary, kSecondaryHover, kText, m_fonts.bold, S(6), !urls.empty())) {
            ImGui::SetClipboardText(U8(urls.front()).c_str());
            ShowToast("Copied " + U8(urls.front()));
        }
        ImGui::SameLine(0.0f, S(12));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (S(34) - ImGui::GetTextLineHeight()) * 0.5f);
        std::wstring streamError =
            !stats.streamError.empty() ? stats.streamError : m_stream.LastError();
        if (!m_stream.Running()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
            ImGui::TextUnformatted("Stream not running");
            HelpMarker(U8(m_streamError).c_str(), S(380));
            ImGui::PopStyleColor();
        } else if (!streamError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
            ImGui::TextUnformatted("Can't stream this game");
            HelpMarker(U8(streamError).c_str(), S(380));
            ImGui::PopStyleColor();
        } else {
            int viewers = m_stream.Viewers();
            // USB has no URL: how the phone's connection stands instead.
            std::string where = urls.empty() ? U8(m_stream.Status()) : U8(urls.front());
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
            ImGui::Text("%s   \xC2\xB7   %d %s", where.c_str(), viewers,
                        viewers == 1 ? "viewer" : "viewers");
            ImGui::PopStyleColor();
            std::string health = StreamHealth(m_stream);
            if (!health.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
                ImGui::TextUnformatted((ICON_WARNING "  " + health).c_str());
                ImGui::PopStyleColor();
            }
        }
    }
    EndCard();
}

void App::DrawGameSettingsCard(Game& game) {
    BeginCard("##settings", m_scale);
    CardTitle(m_fonts.heading, ICON_EDIT "  Game settings");
    float browseWidth = S(90);

    FieldLabel(m_fonts.caption, "Name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##name", &m_editName);
    TypeOnSelect();
    if (ImGui::IsItemDeactivatedAfterEdit() && !m_editName.empty()) {
        game.name = Utf8::ToWide(m_editName);
        SaveLibrary();
    }

    FieldLabel(m_fonts.caption, "Executable");
    std::string exe = U8(game.exePath);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - S(8));
    ImGui::InputText("##exe", &exe, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine(0.0f, S(8));
    if (ImGui::Button("Show##exe", ImVec2(browseWidth, 0))) {
        ShowInExplorer(game.exePath);
    }

    FieldLabel(m_fonts.caption, "Launch arguments");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##args", "For example: -dx12 -windowed", &m_editArgs);
    TypeOnSelect();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        game.arguments = Utf8::ToWide(m_editArgs);
        SaveLibrary();
    }

    FieldLabel(m_fonts.caption, "Working directory");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - S(8));
    ImGui::InputTextWithHint("##workdir", "The game's folder", &m_editWorkDir);
    TypeOnSelect();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        game.workingDir = Utf8::ToWide(m_editWorkDir);
        SaveLibrary();
    }
    ImGui::SameLine(0.0f, S(8));
    if (ImGui::Button("Browse##workdir", ImVec2(browseWidth, 0))) {
        std::vector<std::wstring> picked = PickFiles(m_hwnd, PickKind::Folder);
        if (!picked.empty()) {
            game.workingDir = picked[0];
            m_editWorkDir = U8(picked[0]);
            SaveLibrary();
        }
    }

    FieldLabel(m_fonts.caption, "Launch through");
    std::string autoLabel = m_editSteamAppId.empty()
                                ? std::string("Automatic (start the exe)")
                                : "Automatic (through Steam, app " + U8(m_editSteamAppId) + ")";
    const char* viaNames[] = {autoLabel.c_str(), "The game's exe", "Steam"};
    int via = int(game.launchVia);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##via", &via, viaNames, IM_ARRAYSIZE(viaNames))) {
        game.launchVia = LaunchVia(via);
        SaveLibrary();
    }
    if (game.launchVia == LaunchVia::Steam && m_editSteamAppId.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
        ImGui::TextUnformatted(ICON_WARNING "  Not in a Steam library, so the exe starts directly");
        ImGui::PopStyleColor();
    }

    FieldLabel(m_fonts.caption, "Game process");
    HelpMarker(
        "Set this if the exe is a launcher that starts the real game, so Gazeora "
        "captures the right program.",
        S(380));
    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string processHint = "Default: " + U8(LaunchSpec{game.exePath}.GameProcessName());
    ImGui::InputTextWithHint("##process", processHint.c_str(), &m_editProcess);
    TypeOnSelect();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        game.processName = Utf8::ToWide(m_editProcess);
        SaveLibrary();
    }

    FieldLabel(m_fonts.caption, "Cover art");
    if (ImGui::Button(ICON_PICTURE "  Choose image...")) {
        std::vector<std::wstring> picked = PickFiles(m_hwnd, PickKind::Image);
        if (!picked.empty()) {
            m_textures.Forget(picked[0]);
            if (m_textures.Image(picked[0])) {
                game.coverPath = picked[0];
                SaveLibrary();
            } else {
                ShowToast("Couldn't load that image. Try a PNG or JPG file.", true);
            }
        }
    }
    if (!game.coverPath.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Use game icon")) {
            game.coverPath.clear();
            SaveLibrary();
        }
    }
    EndCard();
}

void App::DrawGameInfoCard(Game& game) {
    BeginCard("##info", m_scale);
    CardTitle(m_fonts.heading, ICON_INFO "  Info");
    auto row = [&](const char* label, const std::string& value) {
        FieldLabel(m_fonts.caption, label);
        ImGui::TextUnformatted(value.c_str());
    };
    row("Playtime", PlaytimeText(game.playtimeSeconds));
    row("Last played", game.lastPlayed ? DateText(game.lastPlayed) : "Never");
    row("Added", DateText(game.addedAt));
    row("Library ID", std::to_string(game.id));

    ImGui::Dummy(ImVec2(0, S(4)));
    float width = ImGui::GetContentRegionAvail().x;
    if (FilledButton(ICON_FOLDER "   Open file location##open", ImVec2(width, S(34)), kSecondary,
                     kSecondaryHover, kText, m_fonts.bold, S(6))) {
        ShowInExplorer(game.exePath);
    }
    if (FilledButton(ICON_DELETE "   Remove from library##remove", ImVec2(width, S(34)),
                     WithAlpha(kDanger, 0.14f), WithAlpha(kDanger, 0.26f), kDanger, m_fonts.bold,
                     S(6))) {
        m_pendingRemoveId = game.id;
    }
    EndCard();
}

}  // namespace gazeora::app
