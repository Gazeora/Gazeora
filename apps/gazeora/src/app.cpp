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
#include "ui/gamepad_scroll.h"
#include "ui/logo_frame.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

namespace {

// Port for the frame stream, whatever the protocol (8554 is RTSP's usual alternative to
// 554). If it's taken, any free port is used.
constexpr uint16_t kStreamPort = 8554;
// Size of the logo frame streamed while no game is capturing.
constexpr int kLogoFrameWidth = 1280;
constexpr int kLogoFrameHeight = 720;

}  // namespace

App::App(HWND hwnd, ID3D11Device* device, float dpiScale)
    : m_hwnd(hwnd), m_scale(dpiScale), m_device(device), m_textures(device) {
    std::wstring error;
    if (!m_library.Load(error)) {
        ShowToast(U8(error), true);
    }
    // The stream is always on and open to the whole network. It shows the logo frame until a
    // game is captured.
    m_vr = m_settings.LoadVrLayout();
    m_stream.SetVrLayout(m_vr);
    m_overlay = m_settings.LoadOverlay();
    m_stream.SetOverlay(m_overlay);
    m_idlePicture = m_settings.LoadIdlePicture() ? IdlePicture::Window : IdlePicture::Logo;
    m_stream.SetLogFile(StreamSettings::LogPath());
    m_stream.SetCodec(m_settings.LoadCodec());
    m_stream.SetMaxBitrate(m_settings.LoadMaxBitrate());
    m_stream.SetFrameRate(m_settings.LoadFrameRate());
    m_stream.SetProtocol(m_settings.LoadProtocol());
    m_stream.SetPointerSink(&m_remoteTouches);
    StartStream();
    m_controls.Resume();
    m_gamepadControl.Resume();
}

void App::StartStream() {
    m_streamError.clear();
    if (!m_stream.Start(kStreamPort, true, m_streamError) &&
        !m_stream.Start(0, true, m_streamError)) {
        ShowToast("Frame streaming unavailable: " + U8(m_streamError), true);
    }
}

App::~App() {
    // Count the time of games still running; the games themselves keep running.
    bool changed = false;
    for (const auto& session : m_sessions) {
        GameSession::Stats stats = session->Snapshot();
        if (stats.state == GameSession::State::Running) {
            if (Game* game = m_library.Find(session->GameId())) {
                game->playtimeSeconds += stats.elapsedSeconds;
                changed = true;
            }
        }
    }
    m_sessions.clear();
    if (changed) {
        std::wstring error;
        m_library.Save(error);
    }
}

void App::SetupImGui() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // fed by ApplyGamepadInput

    wchar_t windows[MAX_PATH];
    GetWindowsDirectoryW(windows, MAX_PATH);
    std::string fonts = U8(windows) + "\\Fonts\\";
    auto exists = [](const std::string& path) {
        return GetFileAttributesW(Utf8::ToWide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
    };
    // Windows 11 ships Segoe Fluent Icons; Windows 10 has the MDL2 assets with the same codes.
    std::string iconFont = fonts + "SegoeIcons.ttf";
    if (!exists(iconFont)) {
        iconFont = fonts + "segmdl2.ttf";
    }

    static ImVector<ImWchar> iconRanges;
    if (iconRanges.empty()) {
        ImFontGlyphRangesBuilder builder;
        builder.AddText(kAllIcons);
        builder.BuildRanges(&iconRanges);
    }
    static const ImWchar textRanges[] = {0x0020, 0x00FF, 0x0100, 0x024F, 0x2010, 0x205E, 0};

    auto addFont = [&](const char* file, float size, bool withIcons) {
        float px = std::round(size * m_scale);
        std::string path = fonts + file;
        ImFont* font = exists(path)
                           ? io.Fonts->AddFontFromFileTTF(path.c_str(), px, nullptr, textRanges)
                           : nullptr;
        if (!font) {
            ImFontConfig config;
            config.SizePixels = px;
            font = io.Fonts->AddFontDefault(&config);
        }
        if (withIcons && exists(iconFont)) {
            ImFontConfig config;
            config.MergeMode = true;
            config.PixelSnapH = true;
            config.GlyphOffset.y = std::round(px * 0.12f);
            io.Fonts->AddFontFromFileTTF(iconFont.c_str(), px * 0.92f, &config, iconRanges.Data);
        }
        return font;
    };
    m_fonts.body = addFont("segoeui.ttf", 14.0f, true);  // first font = default
    m_fonts.bold = addFont("seguisb.ttf", 14.0f, true);
    m_fonts.caption = addFont("segoeui.ttf", 12.0f, true);
    m_fonts.heading = addFont("seguisb.ttf", 17.0f, true);
    m_fonts.title = addFont("segoeuib.ttf", 26.0f, true);
    m_fonts.hugeIcon = addFont("segoeui.ttf", 44.0f, true);

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = ImVec2(10, 10);  // popups and menus; the panes set their own
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(8, 8);
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ScaleAllSizes(m_scale);

    ImVec4* colors = style.Colors;
    auto c = [](ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); };
    colors[ImGuiCol_Text] = c(kText);
    colors[ImGuiCol_TextDisabled] = c(kTextFaint);
    colors[ImGuiCol_WindowBg] = c(kWindowBg);
    colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg] = c(WithAlpha(kCardBg, 0.98f));
    colors[ImGuiCol_Border] = c(kBorder);
    colors[ImGuiCol_FrameBg] = c(kSecondary);
    colors[ImGuiCol_FrameBgHovered] = c(kSecondaryHover);
    colors[ImGuiCol_FrameBgActive] = c(kSecondaryHover);
    colors[ImGuiCol_Button] = c(kSecondary);
    colors[ImGuiCol_ButtonHovered] = c(kSecondaryHover);
    colors[ImGuiCol_ButtonActive] = c(kBorder);
    colors[ImGuiCol_Header] = c(WithAlpha(kAccent, 0.22f));
    colors[ImGuiCol_HeaderHovered] = c(WithAlpha(kAccent, 0.32f));
    colors[ImGuiCol_HeaderActive] = c(WithAlpha(kAccent, 0.42f));
    colors[ImGuiCol_CheckMark] = c(kAccent);
    colors[ImGuiCol_SliderGrab] = c(kAccent);
    colors[ImGuiCol_SliderGrabActive] = c(kAccentHover);
    colors[ImGuiCol_TextSelectedBg] = c(WithAlpha(kAccent, 0.35f));
    colors[ImGuiCol_NavHighlight] = c(kAccent);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ScrollbarGrab] = c(kSecondary);
    colors[ImGuiCol_ScrollbarGrabHovered] = c(kSecondaryHover);
    colors[ImGuiCol_ScrollbarGrabActive] = c(kBorder);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.6f);
}

void App::Tick() {
    m_controls.SetPhoneInVr(m_stream.AppViewers() > 0 && m_vr.enabled);
    m_controls.SetGameRunning(GameRunning());
    m_gamepadControl.SetGameRunning(GameRunning());
    if (m_logoDirty) {
        UpdateLogoFrame();  // here rather than in Draw: it uses the UI's device context
        m_logoDirty = false;
    }
    if (m_usbDriverInstall.valid() &&
        m_usbDriverInstall.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::wstring error = m_usbDriverInstall.get();
        if (error.empty()) {
            ShowToast(m_usbDriverDone);
        } else {
            ShowToast(U8(error), true);
        }
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        GameSession& session = **it;
        if (!session.Finished()) {
            ++it;
            continue;
        }
        GameSession::Stats stats = session.Snapshot();
        Game* game = m_library.Find(session.GameId());
        std::string name = game ? U8(game->name) : "Game";
        if (stats.state == GameSession::State::Failed) {
            ShowToast("Couldn't launch " + name + ": " + U8(stats.error), true);
        } else if (stats.state == GameSession::State::Exited && game) {
            game->playtimeSeconds += stats.elapsedSeconds;
            SaveLibrary();
        }
        it = m_sessions.erase(it);
    }
}

bool App::WantsFramesWhileMinimized() const {
    return m_idlePicture == IdlePicture::Window && m_stream.Running() && m_stream.Viewers() > 0;
}

void App::ApplyRemoteInput() {
    std::vector<PointerEvent> touches = m_remoteTouches.Take();
    if (GameRunning()) {
        return;  // dropped, not kept: they'd land all at once when the game closes
    }
    m_remoteInput.Apply(touches, ImGui::GetIO(), GetTickCount64());
}

void App::ApplyGamepadInput() {
    std::vector<input::GamepadState> states = m_gamepadEvents.Take();
    ImGuiIO& io = ImGui::GetIO();
    if (!GamepadDrivesWindow()) {
        m_gamepadInput.Release(io);  // dropped, like touches while a game runs
        m_gamepadPresses.Clear();
        return;
    }
    m_gamepadPresses.Update(states);
    m_gamepadInput.Apply(states, m_gamepadControl.Connected(), io);
}

bool App::GamepadDrivesWindow() const {
    return !GameRunning() && (GetForegroundWindow() == m_hwnd || WantsFramesWhileMinimized());
}

bool App::GameRunning() const {
    for (const auto& session : m_sessions) {
        if (!session->Finished()) {
            return true;
        }
    }
    return false;
}

bool App::WantsContinuousFrames() const {
    if (!m_toasts.empty() || ImGui::GetIO().WantTextInput || ImGuiPointerInput::HasQueuedInput() ||
        m_gamepadInput.Holding() || m_gamepadPresses.Holding()) {
        return true;
    }
    if (m_page == Page::Controls && m_controls.Animating()) {
        return true;  // the hand preview
    }
    for (const auto& session : m_sessions) {
        if (session->Snapshot().state == GameSession::State::Launching) {
            return true;
        }
    }
    return false;
}

void App::OnFilesDropped(const std::vector<std::wstring>& paths) {
    m_deferred.push_back([this, paths] { AddGamePaths(paths); });
}

void App::OnActivated() {
    if (!m_library.ChangedOnDisk()) {
        return;
    }
    m_deferred.push_back([this] {
        std::wstring error;
        if (!m_library.Load(error)) {
            ShowToast(U8(error), true);
        }
        m_editId = 0;
        if (m_page == Page::Details && !m_library.Find(m_detailsId)) {
            m_page = Page::Library;
        }
    });
}

void App::Draw() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(3);

    m_panes.Apply();
    DrawSidebar();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(32), S(26)));
    ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
    m_panes.MarkPage();
    GamepadScroll(m_scale).Apply();
    FollowGamepadPageKeys();
    FollowGamepadBack();
    if (m_page != m_shownPage || m_detailsId != m_shownDetailsId) {
        m_shownPage = m_page;
        m_shownDetailsId = m_detailsId;
        ImGui::SetScrollY(0.0f);
    }
    switch (m_page) {
        case Page::Home:
            DrawHome();
            break;
        case Page::Library:
            DrawLibrary();
            break;
        case Page::Details:
            DrawDetails();
            break;
        case Page::Controls:
            m_controls.Draw({m_fonts.title, m_fonts.heading, m_fonts.caption});
            break;
        case Page::Settings:
            DrawSettings();
            break;
    }
    ImGui::Dummy(ImVec2(0, S(8)));
    ImGui::EndChild();
    ImGui::End();

    DrawToasts();
    m_panes.Follow();
    m_gamepadBusy =
        ImGui::IsAnyItemActive() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

    std::vector<std::function<void()>> deferred;
    deferred.swap(m_deferred);
    for (auto& action : deferred) {
        action();
    }
}

void App::ApplyVrLayout() {
    m_stream.SetVrLayout(m_vr);
    m_settings.SaveVrLayout(m_vr);
    m_logoDirty = true;
}

void App::UpdateLogoFrame() {
    std::vector<uint8_t> logo = RenderLogoFrame(kLogoFrameWidth, kLogoFrameHeight);
    uint32_t width = kLogoFrameWidth, height = kLogoFrameHeight;
    if (logo.empty()) {
        return;
    }

    // Through the same GPU pass as game frames, so the logo matches them in the headset.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = {logo.data(), width * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    if (m_vr.WarpOnPc() && SUCCEEDED(m_device->CreateTexture2D(&desc, &data, &texture))) {
        TextureReader reader;
        reader.SetMaxSize(FrameStreamServer::kMaxWidth, FrameStreamServer::kMaxHeight);
        reader.SetVrLayout(m_vr);
        std::vector<uint8_t> warped;
        UINT warpedWidth, warpedHeight;
        std::wstring error;
        if (reader.Read(m_device.Get(), texture.Get(), warpedWidth, warpedHeight, warped, error)) {
            logo.swap(warped);
            width = warpedWidth;
            height = warpedHeight;
        }
    }

    // The preview shows exactly what goes out.
    m_logoPreview.Reset();
    desc.Width = width;
    desc.Height = height;
    data = {logo.data(), width * 4, 0};
    if (SUCCEEDED(m_device->CreateTexture2D(&desc, &data, texture.ReleaseAndGetAddressOf()))) {
        m_device->CreateShaderResourceView(texture.Get(), nullptr, &m_logoPreview);
        m_logoPreviewAspect = float(width) / float(height);
    }
    // Kept by the stream across restarts, and for a stream that starts later. Streamed when no
    // game is, unless this window is (StreamWindow).
    if (m_idlePicture == IdlePicture::Logo) {
        m_stream.SetIdleFrame(width, height, std::move(logo));
    }
}

void App::SetIdlePicture(IdlePicture picture) {
    m_idlePicture = picture;
    m_settings.SaveIdlePicture(picture == IdlePicture::Window);
    m_windowStreamedAtMs = 0;  // the window's next frame goes out at once
    m_logoDirty = true;        // and the logo, when it's the one
}

void App::StreamWindow(IDXGISwapChain* swapChain) {
    if (m_idlePicture != IdlePicture::Window || !m_stream.Running()) {
        return;
    }
    // While someone watches, at the stream's frame rate at most; otherwise once, so the
    // stream has a picture to start with (it repeats the last one until the window changes).
    uint64_t now = GetTickCount64();
    if (m_windowStreamedAtMs != 0 && (m_stream.Viewers() == 0 || now - m_windowStreamedAtMs < 33)) {
        return;
    }
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        return;
    }
    // A copy the reader can sample: back buffers of a flip swap chain can't be.
    D3D11_TEXTURE2D_DESC desc;
    backBuffer->GetDesc(&desc);
    D3D11_TEXTURE2D_DESC copied = {};
    if (m_windowCopy) {
        m_windowCopy->GetDesc(&copied);
    }
    if (!m_windowCopy || copied.Width != desc.Width || copied.Height != desc.Height ||
        copied.Format != desc.Format) {
        D3D11_TEXTURE2D_DESC copy = {};
        copy.Width = desc.Width;
        copy.Height = desc.Height;
        copy.MipLevels = 1;
        copy.ArraySize = 1;
        copy.Format = desc.Format;
        copy.SampleDesc.Count = 1;
        copy.Usage = D3D11_USAGE_DEFAULT;
        copy.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        m_windowCopy.Reset();
        if (FAILED(m_device->CreateTexture2D(&copy, nullptr, &m_windowCopy))) {
            return;
        }
    }
    ComPtr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);
    context->CopyResource(m_windowCopy.Get(), backBuffer.Get());

    // Scaled to fit the stream, and laid out for the headset like game frames.
    m_windowReader.SetMaxSize(FrameStreamServer::kMaxWidth, FrameStreamServer::kMaxHeight);
    m_windowReader.SetVrLayout(m_vr);
    UINT width = 0, height = 0;
    std::vector<uint8_t> bgra;
    std::wstring error;
    if (m_windowReader.Read(m_device.Get(), m_windowCopy.Get(), width, height, bgra, error)) {
        m_stream.SetIdleFrame(width, height, std::move(bgra));
        m_windowStreamedAtMs = now;
    }
}

}  // namespace gazeora::app
