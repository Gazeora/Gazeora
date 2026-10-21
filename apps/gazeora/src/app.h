#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "game_session.h"
#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/library/game_library.h"
#include "textures.h"

struct ImFont;

namespace gazeora::app {

// The Gazeora.exe window contents: sidebar navigation, home, library grid, game
// details and settings. Owns the game library, the sessions of launched games and the
// local server that streams their captured frames.
class App {
public:
    App(HWND hwnd, ID3D11Device* device, float dpiScale);
    ~App();

    // Builds the fonts and style; call once before the first frame.
    void SetupImGui();

    // Polls launched games. Safe to call while minimized.
    void Tick();
    // Builds this frame's UI (between ImGui::NewFrame and ImGui::Render).
    void Draw();

    // True while something animates or changes faster than idle repaints show it.
    bool WantsContinuousFrames() const;

    void OnFilesDropped(const std::vector<std::wstring>& paths);
    // The window was activated: pick up changes GazeoraCLI made to the library.
    void OnActivated();

private:
    enum class Page { Home, Library, Details, Settings };
    enum class Sort { RecentlyPlayed, Name, RecentlyAdded };

    struct Toast {
        std::string text;
        bool error;
        double expiresAt;
    };

    struct Fonts {
        ImFont* body = nullptr;
        ImFont* bold = nullptr;
        ImFont* small = nullptr;
        ImFont* heading = nullptr;
        ImFont* title = nullptr;
        ImFont* hugeIcon = nullptr;
    };

    // Pages and sections.
    void DrawSidebar();
    void DrawHome();
    void DrawLibrary();
    void DrawDetails();
    void DrawSettings();
    void DrawEmptyLibrary();
    void DrawToasts();
    void DrawGameBanner(const Game& game, float height, const char* eyebrow, bool showDetails);
    void DrawSessionCard(const Game& game);
    void DrawGameSettingsCard(Game& game);
    void DrawGameInfoCard(Game& game);
    void DrawGameCard(const Game& game, float width);
    void DrawGameContextMenu(const Game& game);
    void DrawCodecChoice();  // the video codec setting, in the frame stream card
    void DrawVrCard(float width);

    // Actions.
    void AddGamesFromDialog();
    void AddGamePaths(const std::vector<std::wstring>& paths);
    void Launch(uint32_t gameId, bool capture);
    void RemoveGame(uint32_t gameId);
    void OpenDetails(uint32_t gameId);
    void SaveLibrary();
    void ShowToast(const std::string& text, bool error = false);
    // Applies m_vr to the stream and saves it; the logo frame is redone on the next Tick.
    void ApplyVrLayout();
    // Streams the logo, through the VR layout when it's on, and refreshes its preview.
    void UpdateLogoFrame();

    GameSession* SessionFor(uint32_t gameId) const;
    const Texture& ArtFor(const Game& game);
    uint64_t AccentGradient();  // ImTextureID of the horizontal gradient
    std::vector<const Game*> SortedGames(Sort sort, const std::string& filter) const;
    float S(float px) const { return px * m_scale; }

    HWND m_hwnd;
    float m_scale;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    TextureCache m_textures;
    GameLibrary m_library;
    FrameStreamServer m_stream;  // declared before m_sessions: they push frames into it
    std::wstring m_streamError;  // why it isn't running
    VrLayout m_vr;
    bool m_logoDirty = true;
    // What the logo frame looks like in the stream, shown under the VR settings.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_logoPreview;
    float m_logoPreviewAspect = 1.0f;
    std::vector<std::unique_ptr<GameSession>> m_sessions;
    Fonts m_fonts;

    Page m_page = Page::Home;
    uint32_t m_detailsId = 0;
    Page m_shownPage = Page::Home;  // to scroll back to the top when the page changes
    uint32_t m_shownDetailsId = 0;
    Sort m_sort = Sort::RecentlyPlayed;
    std::string m_search;
    uint32_t m_pendingRemoveId = 0;

    // Edit buffers for the details page, refreshed when it shows another game.
    uint32_t m_editId = 0;
    std::string m_editName, m_editArgs, m_editWorkDir, m_editProcess;
    std::wstring m_editSteamAppId;  // detected from the exe's Steam library, if any

    std::vector<Toast> m_toasts;
    // Actions that add or remove games run after the frame is drawn, so the pages never
    // hold pointers into a library that changed under them.
    std::vector<std::function<void()>> m_deferred;
};

}  // namespace gazeora::app
