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
#include "settings/stream_settings.h"
#include "ui/logo_frame.h"
#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/widgets.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::app {

void App::DrawSettings() {
    ImGui::PushFont(m_fonts.title);
    ImGui::TextUnformatted("Settings");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, S(10)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), S(20)));
    float width = std::min(ImGui::GetContentRegionAvail().x, S(760));

    BeginCard("##library", m_scale, width);
    CardTitle(m_fonts.heading, ICON_LIBRARY "  Library");
    FieldLabel(m_fonts.small, "Library file (shared with GazeoraCLI)");
    ImGui::TextUnformatted(U8(m_library.Path()).c_str());
    ImGui::Text("%zu games", m_library.Games().size());
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
    ImGui::TextUnformatted(U8(dll).c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, found ? kAccent : kDanger);
    ImGui::TextUnformatted(found ? ICON_CHECK "  Found" : ICON_WARNING "  Missing - rebuild Gazeora");
    ImGui::PopStyleColor();
    FieldLabel(m_fonts.small, "Frame stream");
    std::vector<std::wstring> urls = m_stream.Urls();
    if (!urls.empty()) {
        for (const std::wstring& url : urls) {
            std::string text = U8(url);
            ImGui::TextUnformatted(text.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(("Copy##" + text).c_str())) {
                ImGui::SetClipboardText(text.c_str());
                ShowToast("Copied " + text);
            }
        }
        DrawCodecChoice();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(0.0f);
        std::wstring encoder = m_stream.EncoderName();
        ImGui::Text("Always on, %d %s. Shows the Gazeora logo while no game is captured. "
                    "Watch on any device on this network: in VLC, Media > Open Network "
                    "Stream (or ffplay, OBS). %s over RTP/UDP, or TCP for players set to "
                    "it%s%s.",
                    m_stream.Viewers(), m_stream.Viewers() == 1 ? "viewer" : "viewers",
                    CodecName(m_stream.Codec()), encoder.empty() ? "" : ", encoded by ",
                    U8(encoder).c_str());
        // VLC buffers a second of any network stream by default; that's most of the delay.
        ImGui::TextUnformatted(
            "Delay: VLC buffers 1 second by default. On a wired PC, 0-100 ms of network caching "
            "is fine. Phones on Wi-Fi need some slack or the picture stalls: start around "
            "300 ms. PC: Open Network Stream > Show more options > Caching. Android: Settings > "
            "Advanced > Network caching value. iOS: Settings > Network Caching Level > Low "
            "Latency (Lowest Latency stalls on most Wi-Fi).");
        ImGui::TextUnformatted(
            "On a weak link the stream lowers its bitrate, and raises it again once the link is "
            "clean. A lost packet smears the picture for under a second. A 5 GHz Wi-Fi network, "
            "and the PC on a cable, help most.");
        ImGui::TextUnformatted(
            "Phone smearing or stopping over Wi-Fi? Turn on RTSP over TCP in the player (VLC "
            "for iOS: the RTSP over TCP switch in Settings). Nothing is lost then; a slow link "
            "skips frames instead.");
        ImGui::TextUnformatted(("Log of what players did: " + U8(StreamSettings::LogPath())).c_str());
        std::string health = StreamHealth(m_stream);
        if (!health.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kWarning);
            ImGui::TextUnformatted((ICON_WARNING "  Now: " + health).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        ImGui::TextUnformatted((ICON_WARNING "  Not running: " + U8(m_streamError)).c_str());
        ImGui::PopStyleColor();
    }
    EndCard();

    DrawVrCard(width);

    BeginCard("##about", m_scale, width);
    CardTitle(m_fonts.heading, ICON_INFO "  About");
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted("Gazeora game launcher and frame capture.");
    ImGui::Text("Command line: GazeoraCLI.exe list | add | remove | launch | attach");
    ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
    ImGui::PopStyleColor();
    EndCard();
    ImGui::PopStyleVar();
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
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(
        "Shows the picture once per eye and pre-distorts it for the viewer's lenses. Play the "
        "stream full screen in landscape on the phone. Changes reach the stream right away, so "
        "you can tune them with the headset on.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    if (m_vr.enabled) {
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

        int screen = -1;
        for (int i = 0; i < IM_ARRAYSIZE(kScreenPresets); ++i) {
            if (fabsf(kScreenPresets[i].aspect - m_vr.screenAspect) < 0.01f) screen = i;
        }
        char custom[32];
        snprintf(custom, sizeof(custom), "%.2f:1", m_vr.screenAspect);
        FieldLabel(m_fonts.small, "Phone screen shape (landscape)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##vrscreen", screen >= 0 ? kScreenPresets[screen].name : custom)) {
            for (int i = 0; i < IM_ARRAYSIZE(kScreenPresets); ++i) {
                if (ImGui::Selectable(kScreenPresets[i].name, i == screen)) {
                    m_vr.screenAspect = kScreenPresets[i].aspect;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        slider("Barrel distortion (k1)", "##vrk1", &m_vr.k1, 0.0f, 1.0f, "%.3f");
        slider("Edge distortion (k2)", "##vrk2", &m_vr.k2, 0.0f, 1.0f, "%.3f");
        slider("Picture size", "##vrscale", &m_vr.imageScale, 0.3f, 1.5f, "%.2f");
        slider("Lens separation (share of the screen width)", "##vrsep", &m_vr.lensSeparation,
               0.3f, 0.7f, "%.3f");
        slider("Color fringe correction", "##vrchroma", &m_vr.chromaCorrection, 0.0f, 0.05f,
               "%.3f");
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
