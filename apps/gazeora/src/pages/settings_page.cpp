// App: the settings page: capture, frame stream and VR headset.
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
#include "platform/usb_driver.h"
#include "settings/stream_settings.h"
#include "ui/logo_frame.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "version_info.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

namespace {

// Extra space between groups of controls in a card, at 100% scaling.
constexpr float kGroupGap = 4.0f;

// What each protocol is good for, in the protocol choice's help.
const char* ProtocolHelp(StreamProtocol protocol) {
    switch (protocol) {
        case StreamProtocol::RtspTcp:
            return "RTSP over TCP: works with the Gazeora app, VLC, ffplay and OBS. Reliable: "
                   "nothing is lost, and a slow connection skips frames instead. Players set "
                   "to UDP can't connect.";
        case StreamProtocol::RtspUdp:
            return "RTSP over UDP: works with VLC, ffplay and OBS. The lowest delay on a good "
                   "network, but a lost packet briefly smears the picture. Doesn't work with "
                   "Android emulators.";
        case StreamProtocol::Udp:
            return "UDP: for the Gazeora app. Frames go straight to the phone with the least "
                   "overhead, and a lost packet drops its frame.";
        case StreamProtocol::Http:
            return "HTTP: a live MPEG-TS stream for the Gazeora app, VLC and ffplay. Reliable: "
                   "nothing is lost, and a slow connection skips frames instead.";
        case StreamProtocol::Usb:
            return "USB: for the Gazeora app on a phone plugged into this PC. No Wi-Fi "
                   "needed. Turn on USB debugging on the phone, or install the USB driver below.";
    }
    return "";
}

}  // namespace

void App::DrawSettings() {
    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Settings");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(6)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), S(14)));
    float width = std::min(ImGui::GetContentRegionAvail().x, S(720));

    BeginCard("##library", m_scale, width);
    CardTitle(m_fonts.heading, ICON_LIBRARY "  Library");
    FieldLabel(m_fonts.caption, "Library file");
    HelpMarker("GazeoraCLI uses this file too.", S(360));
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextWrapped("%s  \xC2\xB7  %zu %s", U8(m_library.Path()).c_str(),
                       m_library.Games().size(), m_library.Games().size() == 1 ? "game" : "games");
    ImGui::PopStyleColor();
    if (ImGui::Button(ICON_FOLDER "  Open folder")) {
        ShowInExplorer(FolderOf(m_library.Path()));
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_REFRESH "  Reload")) {
        m_deferred.push_back([this] {
            std::wstring error;
            if (m_library.Load(error)) {
                ShowToast("Library reloaded");
            } else {
                ShowToast(U8(error), true);
            }
            m_editId = 0;
        });
    }
    EndCard();

    BeginCard("##capture", m_scale, width);
    CardTitle(m_fonts.heading, ICON_CAPTURE "  Capture");
    std::wstring dll = HookDllPathNextToExe();
    bool found = GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES;
    FieldLabel(m_fonts.caption, "Game capture");
    ImGui::PushStyleColor(ImGuiCol_Text, found ? kAccent : kDanger);
    ImGui::TextUnformatted(found ? ICON_CHECK "  Ready"
                                 : ICON_WARNING "  Missing: reinstall Gazeora");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", U8(dll).c_str());
    }
    EndCard();

    BeginCard("##stream", m_scale, width);
    CardTitle(m_fonts.heading, ICON_CAMERA "  Frame stream");
    const StreamProtocol protocol = m_stream.Protocol();
    std::string help = std::string(ProtocolHelp(protocol)) +
                       "\n\nThe stream is always on: while no game runs, it shows this window. "
                       "The Gazeora app finds this PC by itself on the same network.";
    if (protocol != StreamProtocol::Udp && protocol != StreamProtocol::Usb) {
        // VLC buffers a second of any network stream by default; that's most of the delay.
        help +=
            "\n\nVLC waits 1 second before playing by default. To cut the delay, lower its "
            "network caching: 0-100 ms on a wired network, about 300 ms on Wi-Fi.";
    }
    help +=
        "\n\nOn a weak connection the quality drops, then recovers once the connection is "
        "clean. 5 GHz Wi-Fi and a wired PC help most.\n\nLog file: " +
        U8(StreamSettings::LogPath());
    FieldLabel(m_fonts.caption, "Protocol");
    HelpMarker(help.c_str(), S(380));
    DrawProtocolChoice();
    if (m_stream.Running()) {
        DrawCodecChoice();
        DrawQualityChoice();
        DrawFrameRateChoice();
        std::vector<std::wstring> urls = m_stream.Urls();
        if (!urls.empty()) {
            FieldLabel(m_fonts.caption, "Stream URL");
        }
        for (const std::wstring& url : urls) {
            std::string text = U8(url);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(text.c_str());
            ImGui::SameLine();
            if (ImGui::Button(("Copy##" + text).c_str())) {
                ImGui::SetClipboardText(text.c_str());
                ShowToast("Copied " + text);
            }
        }
        if (protocol == StreamProtocol::Usb) {
            DrawUsbDriverButton();
        }

        int viewers = m_stream.Viewers();
        std::wstring status = m_stream.Status();
        std::wstring encoder = m_stream.EncoderName();
        ImGui::PushTextWrapPos(0.0f);
        if (!status.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, viewers > 0 ? kAccent : kText);
            ImGui::TextUnformatted(U8(status).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::Text("%d %s  \xC2\xB7  %s%s%s", viewers, viewers == 1 ? "viewer" : "viewers",
                    CodecName(m_stream.Codec()), encoder.empty() ? "" : "  \xC2\xB7  ",
                    U8(encoder).c_str());
        ImGui::PopStyleColor();
        std::string health = StreamHealth(m_stream);
        if (!health.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
            ImGui::TextUnformatted((ICON_WARNING "  " + health).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        std::string stopped = ICON_WARNING "  Stream not running: " + U8(m_streamError);
        ImGui::TextWrapped("%s", stopped.c_str());
        ImGui::PopStyleColor();
    }
    EndCard();

    DrawOverlayCard(width);
    DrawVrCard(width);

    BeginCard("##about", m_scale, width);
    CardTitle(m_fonts.heading, ICON_INFO "  About");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::Text("Game launcher with capture and streaming  \xC2\xB7  Dear ImGui %s", IMGUI_VERSION);
    ImGui::TextUnformatted("Command line: GazeoraCLI.exe list | add | remove | launch | attach");
    ImGui::PopStyleColor();
    // Required in every version by NOTICE's additional terms (GPLv3 section 7(b)).
    ImGui::TextUnformatted(GAZEORA_ATTRIBUTION "  \xC2\xB7  " GAZEORA_VERSION_STRING);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextWrapped("%s", GAZEORA_COPYRIGHT
                       ". Free software under the GNU GPL version 3, "
                       "with no warranty: see LICENSE and NOTICE.");
    ImGui::PopStyleColor();
    EndCard();
    ImGui::PopStyleVar();
}

void App::DrawProtocolChoice() {
    StreamProtocol current = m_stream.Protocol();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##protocol", ProtocolName(current))) {
        for (StreamProtocol protocol : kAllStreamProtocols) {
            if (ImGui::Selectable(ProtocolName(protocol), protocol == current) &&
                protocol != current) {
                // After the frame: restarting joins the stream's threads.
                m_deferred.push_back([this, protocol] {
                    bool watched = m_stream.Viewers() > 0;
                    m_stream.Stop();
                    m_stream.SetProtocol(protocol);
                    m_settings.SaveProtocol(protocol);
                    StartStream();
                    if (m_stream.Running()) {
                        ShowToast(std::string("Now streaming over ") + ProtocolName(protocol) +
                                  (watched ? ". Viewers need to reconnect." : ""));
                    }
                });
            }
        }
        ImGui::EndCombo();
    }
}

void App::DrawUsbDriverButton() {
    const bool working = m_usbDriverInstall.valid();
    const bool needed = m_stream.NeedsUsbDriver();
    const std::vector<UsbPhone> phones = m_stream.PhonesNeedingUsbDriver();
    // Which phones the driver covers: from Windows' INF folder, now and then.
    if (!working && GetTickCount64() - m_installedPhonesAtMs > 3000) {
        m_installedPhones = InstalledPhoneIds();
        m_installedPhonesAtMs = GetTickCount64();
    }

    std::string label = ICON_SETTINGS "  Install USB driver";
    std::vector<std::wstring> phoneIds;
    for (const UsbPhone& phone : phones) {
        label += (phoneIds.empty() ? " for " : ", ") + U8(phone.name);
        phoneIds.push_back(phone.hardwareId);
    }
    ImGui::BeginDisabled(working);
    if (needed) {
        ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
    }
    if (ImGui::Button(working ? ICON_REFRESH "  Updating the USB driver..." : label.c_str())) {
        HWND owner = m_hwnd;
        m_usbDriverDone = phones.empty()
                              ? "USB driver installed. The phone will connect in a moment."
                              : "USB driver installed. The phone will start streaming in a "
                                "moment, no USB debugging needed.";
        m_usbDriverInstall = std::async(std::launch::async, [owner, phoneIds] {
            std::wstring error;
            InstallUsbDriverElevated(owner, phoneIds, error);
            return error;
        });
        m_installedPhonesAtMs = 0;
    }
    if (needed) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(S(380));
        ImGui::TextUnformatted(
            "Needed once per PC to stream over USB. Windows will ask for "
            "administrator permission.");
        if (!phones.empty()) {
            ImGui::TextUnformatted(
                "Phones without USB debugging can't transfer files with this PC "
                "until you restore file transfer.");
        }
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    if (!m_installedPhones.empty()) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_REFRESH "  Restore file transfer")) {
            HWND owner = m_hwnd;
            m_usbDriverDone =
                "File transfer restored. These phones need USB debugging again to "
                "stream over USB.";
            m_usbDriverInstall = std::async(std::launch::async, [owner] {
                std::wstring error;
                RestoreFileTransferElevated(owner, error);
                return error;
            });
            m_installedPhonesAtMs = 0;
        }
    }
    ImGui::EndDisabled();

    if (!m_installedPhones.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::Text("%zu phone model%s set up to stream without USB debugging",
                    m_installedPhones.size(), m_installedPhones.size() == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }
}

void App::DrawCodecChoice() {
    struct Choice {
        VideoCodec codec;
        const char* name;
    };
    static const Choice kChoices[] = {
        {VideoCodec::H264, "H.264 - works on every device"},
        {VideoCodec::H265, "H.265 - sharper on weak Wi-Fi, newer devices only"},
    };
    VideoCodec current = m_stream.Codec();
    FieldLabel(m_fonts.caption, "Video codec");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##codec", kChoices[current == VideoCodec::H265].name)) {
        for (const Choice& choice : kChoices) {
            bool available = m_stream.CodecAvailable(choice.codec);
            std::string label = choice.name;
            if (!available) label += " (not supported on this PC)";
            if (ImGui::Selectable(label.c_str(), choice.codec == current,
                                  available ? 0 : ImGuiSelectableFlags_Disabled) &&
                choice.codec != current) {
                bool watched = m_stream.Viewers() > 0;
                m_stream.SetCodec(choice.codec);
                m_settings.SaveCodec(choice.codec);
                ShowToast(std::string("Now streaming ") + CodecName(choice.codec) +
                          (watched ? ". Viewers need to reconnect." : ""));
            }
        }
        ImGui::EndCombo();
    }
}

void App::DrawQualityChoice() {
    struct Choice {
        uint32_t bitrate;
        const char* name;
    };
    static const Choice kChoices[] = {
        {10000000, "Balanced - 10 Mbit/s, for weak or busy Wi-Fi"},
        {20000000, "High - 20 Mbit/s, for good Wi-Fi"},
        {35000000, "Very high - 35 Mbit/s, for 5 GHz Wi-Fi and USB"},
    };
    uint32_t current = m_stream.MaxBitrate();
    char custom[48];
    snprintf(custom, sizeof(custom), "Custom - %u Mbit/s", current / 1000000);
    const char* shown = custom;
    for (const Choice& choice : kChoices) {
        if (choice.bitrate == current) shown = choice.name;
    }
    FieldLabel(m_fonts.caption, "Stream quality");
    HelpMarker(
        "Higher quality keeps detail and motion sharp, but needs a faster connection. "
        "H.265 looks about the same with 30% less. If the phone starts losing packets, "
        "Gazeora lowers the quality (down to a quarter) and raises it again once the "
        "connection recovers. Changes apply right away.",
        S(380));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##quality", shown)) {
        for (const Choice& choice : kChoices) {
            if (ImGui::Selectable(choice.name, choice.bitrate == current)) {
                m_stream.SetMaxBitrate(choice.bitrate);
                m_settings.SaveMaxBitrate(choice.bitrate);
            }
        }
        ImGui::EndCombo();
    }
}

void App::DrawFrameRateChoice() {
    struct Choice {
        uint32_t fps;
        const char* name;
    };
    static const Choice kChoices[] = {
        {30, "30 fps - more detail in each frame"},
        {60, "60 fps - smoother, with less delay"},
    };
    uint32_t current = m_stream.FrameRate();
    char custom[32];
    snprintf(custom, sizeof(custom), "Custom - %u fps", current);
    const char* shown = custom;
    for (const Choice& choice : kChoices) {
        if (choice.fps == current) shown = choice.name;
    }
    FieldLabel(m_fonts.caption, "Frame rate");
    HelpMarker(
        "How many frames are sent each second. 60 fps feels smoother and responds "
        "faster, but each frame gets half the data, so raise the stream quality too if "
        "your Wi-Fi allows. Games drawing fewer frames repeat their last one. Changes "
        "apply right away.",
        S(380));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##framerate", shown)) {
        for (const Choice& choice : kChoices) {
            if (ImGui::Selectable(choice.name, choice.fps == current)) {
                m_stream.SetFrameRate(choice.fps);
                m_settings.SaveFrameRate(choice.fps);
            }
        }
        ImGui::EndCombo();
    }
}

void App::DrawOverlayCard(float width) {
    BeginCard("##overlay", m_scale, width);
    CardTitle(m_fonts.heading, ICON_INFO "  Stream overlay");
    bool changed = false;
    // Like the sliders below, text and sliders save when let go; the stream follows every step.
    bool committed = false;

    ImGui::AlignTextToFramePadding();
    changed |= Toggle("##overlayon", &m_overlay.enabled, m_scale);
    ImGui::SameLine();
    ImGui::TextUnformatted("Show stream details on the picture");
    HelpMarker(
        "Draws the details you choose onto the picture itself, so every player shows "
        "them. They appear in a corner of the picture, or of each eye in VR mode.",
        S(380));

    ImGui::BeginDisabled(!m_overlay.enabled);
    FieldLabel(m_fonts.caption, "Details to show");
    struct Detail {
        const char* label;
        bool* value;
    };
    const Detail details[] = {
        {"Protocol", &m_overlay.protocol},    {"Viewers", &m_overlay.viewers},
        {"Codec", &m_overlay.codec},          {"Resolution", &m_overlay.resolution},
        {"Frame rate", &m_overlay.frameRate}, {"Bitrate", &m_overlay.bitrate},
        {"Packet loss", &m_overlay.loss},     {"Encoder", &m_overlay.encoder},
        {"PC time", &m_overlay.clock},
    };
    if (ImGui::BeginTable("##overlaydetails", 3)) {
        for (const Detail& detail : details) {
            ImGui::TableNextColumn();
            changed |= ImGui::Checkbox(detail.label, detail.value);
        }
        ImGui::EndTable();
    }

    FieldLabel(m_fonts.caption, "Custom text (shown under the details)");
    std::string text = U8(m_overlay.text);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool edited =
        ImGui::InputTextWithHint("##overlaytext", "For example, a name or a place", &text);
    TypeOnSelect();
    if (edited) {
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0);
        std::wstring wide(size_t(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), wide.data(), size);
        m_overlay.text = wide;
        m_stream.SetOverlay(m_overlay);
    }
    committed |= ImGui::IsItemDeactivatedAfterEdit();

    static const char* kCorners[] = {"Top left", "Top right", "Bottom left", "Bottom right"};
    FieldLabel(m_fonts.caption, "Corner");
    ImGui::SetNextItemWidth(S(220));
    int corner = int(m_overlay.corner);
    if (ImGui::Combo("##overlaycorner", &corner, kCorners, IM_ARRAYSIZE(kCorners))) {
        m_overlay.corner = OverlaySettings::Corner(corner);
        changed = true;
    }
    FieldLabel(m_fonts.caption, "Text size");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##overlaysize", &m_overlay.textSize, 50, 300, "%d%%")) {
        m_stream.SetOverlay(m_overlay);
    }
    committed |= ImGui::IsItemDeactivatedAfterEdit();
    FieldLabel(m_fonts.caption, "Background");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##overlayopacity", &m_overlay.opacity, 0, 100, "%d%% opaque")) {
        m_stream.SetOverlay(m_overlay);
    }
    committed |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::EndDisabled();

    if (changed) {
        m_stream.SetOverlay(m_overlay);
    }
    if (changed || committed) {
        m_settings.SaveOverlay(m_overlay);
    }
    EndCard();
}

void App::DrawVrCard(float width) {
    BeginCard("##vr", m_scale, width);
    CardTitle(m_fonts.heading, ICON_GAME "  VR headset");
    bool changed = false;
    // Sliders save when let go; the stream follows every step of the drag.
    auto slider = [&](const char* label, const char* id, float* value, float low, float high,
                      const char* format) {
        FieldLabel(m_fonts.caption, label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat(id, value, low, high, format)) {
            m_stream.SetVrLayout(m_vr);
            m_logoDirty = true;
        }
        changed |= ImGui::IsItemDeactivatedAfterEdit();
    };

    changed |= ImGui::Checkbox("Split the picture for a phone VR viewer", &m_vr.enabled);
    HelpMarker(
        "Shows the picture once for each eye, shaped for the viewer's lenses. Play the "
        "stream full screen in landscape. Changes apply live, so you can adjust them "
        "while wearing the headset.",
        S(380));

    if (m_vr.enabled) {
        FieldLabel(m_fonts.caption, "Lens correction done by");
        HelpMarker(
            "The phone: the stream sends the game's picture as is, and the Gazeora app "
            "splits and shapes it for its own screen. Sharper.\n\nThis PC: frames are "
            "split and shaped before they're sent, so any player, such as VLC, can show "
            "them.",
            S(380));
        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* where[] = {"The phone (Gazeora app): sharper",
                               "This PC: for other players, such as VLC"};
        int onPc = m_vr.lensOnPhone ? 0 : 1;
        if (ImGui::Combo("##vrwhere", &onPc, where, IM_ARRAYSIZE(where))) {
            m_vr.lensOnPhone = onPc == 0;
            changed = true;
        }

        int lens = IM_ARRAYSIZE(kLensPresets);  // custom
        for (int i = 0; i < IM_ARRAYSIZE(kLensPresets); ++i) {
            if (kLensPresets[i].k1 == m_vr.k1 && kLensPresets[i].k2 == m_vr.k2) lens = i;
        }
        FieldLabel(m_fonts.caption, "Viewer lenses");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##vrlens", lens < IM_ARRAYSIZE(kLensPresets)
                                              ? kLensPresets[lens].name
                                              : "Custom")) {
            for (int i = 0; i < IM_ARRAYSIZE(kLensPresets); ++i) {
                if (ImGui::Selectable(kLensPresets[i].name, i == lens)) {
                    m_vr.k1 = kLensPresets[i].k1;
                    m_vr.k2 = kLensPresets[i].k2;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        // The phone knows its own screen.
        if (!m_vr.lensOnPhone) {
            int screen = -1;
            for (int i = 0; i < IM_ARRAYSIZE(kScreenPresets); ++i) {
                if (fabsf(kScreenPresets[i].aspect - m_vr.screenAspect) < 0.01f) screen = i;
            }
            char custom[32];
            snprintf(custom, sizeof(custom), "%.2f:1", m_vr.screenAspect);
            FieldLabel(m_fonts.caption, "Phone screen shape (landscape)");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##vrscreen",
                                  screen >= 0 ? kScreenPresets[screen].name : custom)) {
                for (int i = 0; i < IM_ARRAYSIZE(kScreenPresets); ++i) {
                    if (ImGui::Selectable(kScreenPresets[i].name, i == screen)) {
                        m_vr.screenAspect = kScreenPresets[i].aspect;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }

        slider("Lens distortion (k1)", "##vrk1", &m_vr.k1, 0.0f, 1.0f, "%.3f");
        slider("Edge distortion (k2)", "##vrk2", &m_vr.k2, 0.0f, 1.0f, "%.3f");
        slider("Picture size", "##vrscale", &m_vr.imageScale, 0.3f, 1.5f, "%.2f");
        slider("Distance between lenses (share of screen width)", "##vrsep", &m_vr.lensSeparation,
               0.3f, 0.7f, "%.3f");
        slider("Color fringe correction", "##vrchroma", &m_vr.chromaCorrection, 0.0f, 0.05f,
               "%.3f");

        ImGui::Dummy(ImVec2(0, S(kGroupGap)));
        changed |= ImGui::Checkbox("3D from the game's depth", &m_vr.stereo);
        HelpMarker(
            "Gives each eye its own view, built from the game's depth information. "
            "Whatever is in the middle of the view sits at the screen; farther things "
            "recede and nearer ones come forward. Works in D3D11 and D3D12 games during "
            "3D scenes; this window and videos stay flat. Overlays such as the HUD take the "
            "depth of what's behind them.",
            S(380));
        if (m_vr.stereo) {
            slider("3D depth (how far back distant things go)", "##vrdepth", &m_vr.stereoDepth,
                   0.002f, 0.04f, "%.3f");
            slider("Pop-out (how far near things come forward)", "##vrpop", &m_vr.stereoPopOut,
                   0.0f, 1.0f, "%.2f");
        }

        ImGui::Dummy(ImVec2(0, S(kGroupGap)));
        if (ImGui::Button(ICON_REFRESH "  Reset VR settings")) {
            m_vr = VrLayout();
            m_vr.enabled = true;
            changed = true;
        }

        if (m_logoPreview) {
            FieldLabel(m_fonts.caption, "Preview: how these settings shape the picture");
            float previewWidth = ImGui::GetContentRegionAvail().x;
            ImGui::Image(reinterpret_cast<ImTextureID>(m_logoPreview.Get()),
                         ImVec2(previewWidth, previewWidth / m_logoPreviewAspect));
        }
    }
    if (changed) {
        ApplyVrLayout();
    }
    EndCard();
}

}  // namespace gazeora::app
