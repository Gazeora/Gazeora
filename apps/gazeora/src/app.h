#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "game_session.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/stream/frame_stream.h"
#include "gazeora/input/hand_gesture_services.h"
#include "gazeora/library/game_library_services.h"
#include "gazeora/library/platform/library_location.h"
#include "pages/controls_page.h"
#include "remote/imgui_pointer_input.h"
#include "remote/pointer_event_queue.h"
#include "settings/controls_settings.h"
#include "settings/stream_settings.h"
#include "textures.h"

struct ImFont;

namespace gazeora::app {

// The Gazeora.exe window contents: sidebar navigation, home, library grid, game
// details and settings. Owns the game library, the sessions of launched games and the
// local server that streams their captured frames.
//
// The implementation is split by concern: app.cpp (lifetime, frame loop, the stream's logo
// and VR layout), app_actions.cpp (what buttons do), and one file per page under pages/.
// The controls and colors the pages draw with live in ui/.
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
    // Whether to keep drawing while minimized: someone watches this window on the stream.
    bool WantsFramesWhileMinimized() const;

    // The phone's touches since the last frame, onto this window's UI only. Call between the
    // platform backend's NewFrame and ImGui::NewFrame.
    void ApplyRemoteInput();

    // After a frame is drawn, before it's presented: when no game is running, this window is
    // what streams (unless the logo is chosen), so the launcher can be seen from the headset.
    void StreamWindow(IDXGISwapChain* swapChain);

    void OnFilesDropped(const std::vector<std::wstring>& paths);
    // The window was activated: pick up changes GazeoraCLI made to the library.
    void OnActivated();

private:
    enum class Page { Home, Library, Details, Controls, Settings };
    // What streams when no game is.
    enum class IdlePicture { Window, Logo };
    enum class Sort { RecentlyPlayed, Name, RecentlyAdded };

    struct Toast {
        std::string text;
        bool error;
        double expiresAt;
    };

    struct Fonts {
        ImFont* body = nullptr;
        ImFont* bold = nullptr;
        ImFont* caption = nullptr;  // labels and small print ("small" is a Windows macro)
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
    void DrawProtocolChoice();          // the protocol setting, in the frame stream card
    void DrawCodecChoice();             // the video codec setting, in the frame stream card
    void DrawQualityChoice();           // the stream quality (bitrate) setting, beside it
    void DrawFrameRateChoice();         // the frame rate setting, beside those
    void DrawUsbDriverButton();         // the USB driver install, in the frame stream card
    void DrawOverlayCard(float width);  // the streaming details drawn into the frames
    void DrawVrCard(float width);

    // Actions.
    void AddGamesFromDialog();
    void AddGamePaths(const std::vector<std::wstring>& paths);
    void Launch(uint32_t gameId, bool capture);
    void RemoveGame(uint32_t gameId);
    void OpenDetails(uint32_t gameId);
    void SaveLibrary();
    void ShowToast(const std::string& text, bool error = false);
    // Starts the stream with its protocol, on kStreamPort or any free port; m_streamError
    // says why if it can't.
    void StartStream();
    // Applies m_vr to the stream and saves it; the logo frame is redone on the next Tick.
    void ApplyVrLayout();
    // Streams the logo, through the VR layout when it's on, and refreshes its preview.
    void UpdateLogoFrame();
    // Picks what streams when no game is, and saves the choice.
    void SetIdlePicture(IdlePicture picture);

    GameSession* SessionFor(uint32_t gameId) const;
    // A game is launching or running: the phone's touches and hand gestures are off.
    bool GameRunning() const;
    const Texture& ArtFor(const Game& game);
    uint64_t AccentGradient();  // ImTextureID of the horizontal gradient
    std::vector<const Game*> SortedGames(Sort sort, const std::string& filter) const;
    float S(float px) const { return px * m_scale; }

    HWND m_hwnd;
    float m_scale;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    TextureCache m_textures;
    GameLibraryServices m_libraryServices{LibraryLocation::DefaultPath()};
    GameLibrary& m_library = m_libraryServices.Library();
    // The phone's touches, while the stream isn't laid out for VR. Declared before m_stream,
    // whose threads deliver them; each one wakes the UI thread to draw.
    PointerEventQueue m_remoteTouches{[this] { PostMessageW(m_hwnd, WM_NULL, 0, 0); }};
    ImGuiPointerInput m_remoteInput;
    FrameStreamServer m_stream;  // declared before m_sessions: they push frames into it
    std::wstring m_streamError;  // why it isn't running
    StreamSettings m_settings;   // stream.ini: codec, protocol and VR layout
    VrLayout m_vr;
    OverlaySettings m_overlay;
    IdlePicture m_idlePicture = IdlePicture::Window;
    // This window, streamed: a copy of the back buffer, read back like game frames.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_windowCopy;
    TextureReader m_windowReader;
    uint64_t m_windowStreamedAtMs = 0;  // 0: not yet
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

    // The USB driver being installed or taken off phones, as an administrator: what went
    // wrong, empty if nothing; and what to say when it's done.
    std::future<std::wstring> m_usbDriverInstall;
    std::string m_usbDriverDone;
    // The phones the installed driver covers, and when that was looked up.
    std::vector<std::wstring> m_installedPhones;
    uint64_t m_installedPhonesAtMs = 0;

    std::vector<Toast> m_toasts;
    // Actions that add or remove games run after the frame is drawn, so the pages never
    // hold pointers into a library that changed under them.
    std::vector<std::function<void()>> m_deferred;

    // Hand gestures and the Controls page; the page is declared last, as it holds references
    // to the others.
    input::HandGestureServices m_hands;
    ControlsSettings m_controlsSettings;
    ControlsPage m_controls{m_hands, m_controlsSettings, m_scale};
};

}  // namespace gazeora::app
