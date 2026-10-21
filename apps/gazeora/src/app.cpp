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

namespace {

// RTSP port for the frame stream (8554 is the usual alternative to 554). If it's taken,
// any free port is used.
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
    m_stream.SetLogFile(StreamSettings::LogPath());
    m_stream.SetCodec(m_settings.LoadCodec());
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

    wchar_t windows[MAX_PATH];
    GetWindowsDirectoryW(windows, MAX_PATH);
    std::string fonts = U8(windows) + "\\Fonts\\";
    auto exists = [](const std::string& path) {
        return GetFileAttributesW(Utf8ToWide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
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
    m_fonts.body = addFont("segoeui.ttf", 15.0f, true);  // first font = default
    m_fonts.bold = addFont("seguisb.ttf", 15.0f, true);
    m_fonts.small = addFont("segoeui.ttf", 13.0f, true);
    m_fonts.heading = addFont("seguisb.ttf", 22.0f, true);
    m_fonts.title = addFont("segoeuib.ttf", 36.0f, true);
    m_fonts.hugeIcon = addFont("segoeui.ttf", 56.0f, true);

    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = ImVec2(10, 10);  // popups and menus; the panes set their own
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 10);
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
    if (m_logoDirty) {
        UpdateLogoFrame();  // here rather than in Draw: it uses the UI's device context
        m_logoDirty = false;
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

bool App::WantsContinuousFrames() const {
    if (!m_toasts.empty() || ImGui::GetIO().WantTextInput) {
        return true;
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

    DrawSidebar();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(40), S(32)));
    ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
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
        case Page::Settings:
            DrawSettings();
            break;
    }
    ImGui::Dummy(ImVec2(0, S(8)));
    ImGui::EndChild();
    ImGui::End();

    DrawToasts();

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
    if (m_vr.enabled && SUCCEEDED(m_device->CreateTexture2D(&desc, &data, &texture))) {
        TextureReader reader;
        reader.SetMaxSize(FrameStreamServer::kMaxWidth, FrameStreamServer::kMaxHeight);
        reader.SetVrLayout(m_vr);
        std::vector<uint8_t> warped;
        UINT warpedWidth, warpedHeight;
        std::wstring error;
        if (reader.Read(m_device.Get(), texture.Get(), warpedWidth, warpedHeight, warped,
                        error)) {
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
    if (m_stream.Running()) {
        m_stream.SetIdleFrame(width, height, std::move(logo));
    }
}


}  // namespace gazeora::app
