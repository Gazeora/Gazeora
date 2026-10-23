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

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

namespace {

// What each protocol is good for, in the protocol choice's help.
const char* ProtocolHelp(StreamProtocol protocol) {
    switch (protocol) {
        case StreamProtocol::RtspTcp:
            return "RTSP over TCP: for the Gazeora Android app, VLC, ffplay or OBS. Nothing is "
                   "lost; a slow link skips frames. Players set to UDP are refused.";
        case StreamProtocol::RtspUdp:
            return "RTSP over UDP: for VLC, ffplay or OBS. Lowest delay on a good network; a "
                   "lost packet smears the picture briefly. Doesn't reach an Android emulator.";
        case StreamProtocol::Udp:
            return "UDP: for the Gazeora Android app. Frames go straight to the phone with the "
                   "least overhead; a lost datagram costs its frame.";
        case StreamProtocol::Http:
            return "HTTP: a live MPEG-TS stream for the Gazeora Android app, VLC or ffplay. "
                   "Nothing is lost; a slow link skips frames.";
        case StreamProtocol::Usb:
            return "USB: for the Gazeora Android app on a phone plugged into this PC, no Wi-Fi "
                   "needed. Turn on USB debugging on the phone, or install the USB driver.";
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
    FieldLabel(m_fonts.small, "Library file");
    HelpMarker("Shared with GazeoraCLI.", S(360));
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
    FieldLabel(m_fonts.small, "Capture hook");
    ImGui::PushStyleColor(ImGuiCol_Text, found ? kAccent : kDanger);
    ImGui::TextUnformatted(found ? ICON_CHECK "  Found" : ICON_WARNING "  Missing - rebuild Gazeora");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", U8(dll).c_str());
    }
    EndCard();

    BeginCard("##stream", m_scale, width);
    CardTitle(m_fonts.heading, ICON_CAMERA "  Frame stream");
    const StreamProtocol protocol = m_stream.Protocol();
    std::string help = std::string(ProtocolHelp(protocol)) +
                       "\n\nAlways on: shows this window (or the logo) while no game is "
                       "captured. The Android app finds this PC by itself on the same network.";
    if (protocol != StreamProtocol::Udp && protocol != StreamProtocol::Usb) {
        // VLC buffers a second of any network stream by default; that's most of the delay.
        help += "\n\nVLC buffers 1 s by default: lower its network caching (0-100 ms wired, "
                "about 300 ms on Wi-Fi).";
    }
    help += "\n\nOn a weak link the bitrate drops, and recovers once the link is clean. 5 GHz "
            "Wi-Fi and a wired PC help most.\n\nLog: " +
            U8(StreamSettings::LogPath());
    FieldLabel(m_fonts.small, "Protocol");
    HelpMarker(help.c_str(), S(380));
    DrawProtocolChoice();
    if (m_stream.Running()) {
        DrawCodecChoice();
        DrawQualityChoice();
        FieldLabel(m_fonts.small, "When no game is running, stream");
        static const char* kIdlePictures[] = {"Gazeora (this window)", "The Gazeora logo"};
        int idle = int(m_idlePicture);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##idlepicture", &idle, kIdlePictures, IM_ARRAYSIZE(kIdlePictures))) {
            SetIdlePicture(IdlePicture(idle));
        }
        std::vector<std::wstring> urls = m_stream.Urls();
        if (!urls.empty()) {
            FieldLabel(m_fonts.small, "Address");
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
        ImGui::TextWrapped("%s", (ICON_WARNING "  Not running: " + U8(m_streamError)).c_str());
        ImGui::PopStyleColor();
    }
    EndCard();

    DrawOverlayCard(width);
    DrawVrCard(width);

    BeginCard("##about", m_scale, width);
    CardTitle(m_fonts.heading, ICON_INFO "  About");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::Text("Gazeora game launcher and frame capture  \xC2\xB7  Dear ImGui %s", IMGUI_VERSION);
    ImGui::TextUnformatted("CLI: GazeoraCLI.exe list | add | remove | launch | attach");
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
                        ShowToast(std::string("Streaming over ") + ProtocolName(protocol) +
                                  (watched ? ". Players were disconnected." : "."));
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
    if (ImGui::Button(working ? ICON_REFRESH "  Working on the USB driver..." : label.c_str())) {
        HWND owner = m_hwnd;
        m_usbDriverDone = phones.empty()
                              ? "USB driver installed. The phone connects in a moment."
                              : "USB driver installed. The phone switches to the stream in a "
                                "moment, without USB debugging.";
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
        ImGui::TextUnformatted("Once per PC: WinUSB for phones in accessory mode. Windows asks "
                               "for an administrator.");
        if (!phones.empty()) {
            ImGui::TextUnformatted("Phones without USB debugging lose file transfer on this PC "
                                   "until you restore it.");
        }
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    if (!m_installedPhones.empty()) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_REFRESH "  Restore file transfer")) {
            HWND owner = m_hwnd;
            m_usbDriverDone = "File transfer restored. Those phones need USB debugging for the "
                              "USB stream again.";
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
        ImGui::Text("%zu phone model%s switched without USB debugging.", m_installedPhones.size(),
                    m_installedPhones.size() == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }
}

void App::DrawCodecChoice() {
    struct Choice {
        VideoCodec codec;
        const char* name;
    };
    static const Choice kChoices[] = {
        {VideoCodec::H264, "H.264 - plays on every device"},
        {VideoCodec::H265, "H.265 - sharper on weak Wi-Fi, newer devices only"},
    };
    VideoCodec current = m_stream.Codec();
    FieldLabel(m_fonts.small, "Video codec");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##codec", kChoices[current == VideoCodec::H265].name)) {
        for (const Choice& choice : kChoices) {
            bool available = m_stream.CodecAvailable(choice.codec);
            std::string label = choice.name;
            if (!available) label += " (no encoder on this PC)";
            if (ImGui::Selectable(label.c_str(), choice.codec == current,
                                  available ? 0 : ImGuiSelectableFlags_Disabled) &&
                choice.codec != current) {
                bool watched = m_stream.Viewers() > 0;
                m_stream.SetCodec(choice.codec);
                m_settings.SaveCodec(choice.codec);
                ShowToast(std::string("Streaming ") + CodecName(choice.codec) +
                          (watched ? ". Players were disconnected: open the stream again." : "."));
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
        {20000000, "High - 20 Mbit/s, sharp on good Wi-Fi"},
        {35000000, "Very high - 35 Mbit/s, for 5 GHz Wi-Fi and USB"},
    };
    uint32_t current = m_stream.MaxBitrate();
    char custom[48];
    snprintf(custom, sizeof(custom), "Custom - %u Mbit/s", current / 1000000);
    const char* shown = custom;
    for (const Choice& choice : kChoices) {
        if (choice.bitrate == current) shown = choice.name;
    }
    FieldLabel(m_fonts.small, "Stream quality");
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
    HelpMarker("The bitrate at full quality: more bits keep fine detail and motion sharp. H.265 "
               "uses 70% of it for about the same picture. While the phone reports lost "
               "packets, the stream steps down to as little as a quarter of it, and back up "
               "once the link is clean. Applies at once.",
               S(380));
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
    ImGui::TextUnformatted(m_overlay.enabled ? "Streaming details shown in the stream"
                                             : "Show streaming details in the stream");
    HelpMarker("Draws the details you pick into the frames themselves, so every player shows "
               "them: in a corner of the picture, or of each eye in VR mode.",
               S(380));

    ImGui::BeginDisabled(!m_overlay.enabled);
    FieldLabel(m_fonts.small, "Details");
    struct Detail {
        const char* label;
        bool* value;
    };
    const Detail details[] = {
        {"Protocol", &m_overlay.protocol},     {"Viewers", &m_overlay.viewers},
        {"Codec", &m_overlay.codec},           {"Resolution", &m_overlay.resolution},
        {"Frame rate", &m_overlay.frameRate},  {"Bitrate", &m_overlay.bitrate},
        {"Packet loss", &m_overlay.loss},      {"Encoder", &m_overlay.encoder},
        {"PC clock", &m_overlay.clock},
    };
    if (ImGui::BeginTable("##overlaydetails", 3)) {
        for (const Detail& detail : details) {
            ImGui::TableNextColumn();
            changed |= ImGui::Checkbox(detail.label, detail.value);
        }
        ImGui::EndTable();
    }

    FieldLabel(m_fonts.small, "Your own text (a line under the details)");
    std::string text = U8(m_overlay.text);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##overlaytext", "e.g. a name, or where it's streaming from",
                                 &text)) {
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0);
        std::wstring wide(size_t(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), wide.data(), size);
        m_overlay.text = wide;
        m_stream.SetOverlay(m_overlay);
    }
    committed |= ImGui::IsItemDeactivatedAfterEdit();

    static const char* kCorners[] = {"Top left", "Top right", "Bottom left", "Bottom right"};
    FieldLabel(m_fonts.small, "Corner");
    ImGui::SetNextItemWidth(S(220));
    int corner = int(m_overlay.corner);
    if (ImGui::Combo("##overlaycorner", &corner, kCorners, IM_ARRAYSIZE(kCorners))) {
        m_overlay.corner = OverlaySettings::Corner(corner);
        changed = true;
    }
    FieldLabel(m_fonts.small, "Text size");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##overlaysize", &m_overlay.textSize, 50, 300, "%d%%")) {
        m_stream.SetOverlay(m_overlay);
    }
    committed |= ImGui::IsItemDeactivatedAfterEdit();
    FieldLabel(m_fonts.small, "Background");
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
        FieldLabel(m_fonts.small, label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat(id, value, low, high, format)) {
            m_stream.SetVrLayout(m_vr);
            m_logoDirty = true;
        }
        changed |= ImGui::IsItemDeactivatedAfterEdit();
    };

    changed |= ImGui::Checkbox("Side by side for a phone in a VR viewer", &m_vr.enabled);
    HelpMarker("Shows the picture once per eye, pre-distorted for the viewer's lenses. Play the "
               "stream full screen in landscape. Changes apply live, so you can tune them with "
               "the headset on.",
               S(380));

    if (m_vr.enabled) {
        FieldLabel(m_fonts.small, "Lens correction done by");
        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* where[] = {"The phone (Gazeora app): sharper",
                               "This PC: for other players, such as VLC"};
        int onPc = m_vr.lensOnPhone ? 0 : 1;
        if (ImGui::Combo("##vrwhere", &onPc, where, IM_ARRAYSIZE(where))) {
            m_vr.lensOnPhone = onPc == 0;
            changed = true;
        }
        HelpMarker("On the phone, the stream carries the game's picture as it is, every pixel "
                   "for the game, and the app draws it into both eyes for its own screen. On "
                   "this PC, frames go out already split and warped, so any player can show "
                   "them.",
                   S(380));

        int lens = IM_ARRAYSIZE(kLensPresets);  // custom
        for (int i = 0; i < IM_ARRAYSIZE(kLensPresets); ++i) {
            if (kLensPresets[i].k1 == m_vr.k1 && kLensPresets[i].k2 == m_vr.k2) lens = i;
        }
        FieldLabel(m_fonts.small, "Viewer lenses");
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
            FieldLabel(m_fonts.small, "Phone screen shape (landscape)");
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

        slider("Barrel distortion (k1)", "##vrk1", &m_vr.k1, 0.0f, 1.0f, "%.3f");
        slider("Edge distortion (k2)", "##vrk2", &m_vr.k2, 0.0f, 1.0f, "%.3f");
        slider("Picture size", "##vrscale", &m_vr.imageScale, 0.3f, 1.5f, "%.2f");
        slider("Lens separation (share of the screen width)", "##vrsep", &m_vr.lensSeparation,
               0.3f, 0.7f, "%.3f");
        slider("Color fringe correction", "##vrchroma", &m_vr.chromaCorrection, 0.0f, 0.05f,
               "%.3f");

        ImGui::Dummy(ImVec2(0, S(4)));
        changed |= ImGui::Checkbox("3D from the game's depth", &m_vr.stereo);
        HelpMarker("Gives each eye its own view of the scene, made from the game's depth buffer. "
                   "The subject in the middle of the view sits at the screen; farther things go "
                   "back, nearer ones come out. Works in D3D11 and D3D12 games while they draw "
                   "a 3D scene; the logo and videos stay flat. Things drawn over the scene, "
                   "like the HUD, take the depth of what's behind them.",
                   S(380));
        if (m_vr.stereo) {
            slider("3D depth (how far back the distance goes)", "##vrdepth", &m_vr.stereoDepth,
                   0.002f, 0.04f, "%.3f");
            slider("Pop-out (how far near things come out, share of the depth)", "##vrpop",
                   &m_vr.stereoPopOut, 0.0f, 1.0f, "%.2f");
        }

        if (ImGui::Button(ICON_REFRESH "  Reset VR settings")) {
            m_vr = VrLayout();
            m_vr.enabled = true;
            changed = true;
        }

        if (m_logoPreview) {
            FieldLabel(m_fonts.small, "Preview of the idle frame");
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
