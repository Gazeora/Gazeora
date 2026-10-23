#include "pages/controls_page.h"

#include <algorithm>
#include <string>

#include "ui/text_format.h"
#include "ui/theme.h"
#include "ui/tracking_status_line.h"
#include "ui/widgets.h"

namespace gazeora::app {

namespace {

using input::TrackingSnapshot;
using input::TrackingState;

constexpr float kMaxCardWidth = 720.0f;
constexpr float kHelpWidth = 380.0f;
constexpr float kTitleGap = 6.0f;
constexpr float kRefreshButtonWidth = 110.0f;
constexpr ImVec2 kCardItemSpacing(10.0f, 14.0f);

}  // namespace

ControlsPage::ControlsPage(input::HandGestureServices& hands, const ControlsSettings& store,
                           float scale)
    : m_hands(hands),
      m_store(store),
      m_scale(scale),
      m_settings(store.LoadHandGestures()),
      m_preview(scale) {}

void ControlsPage::Resume() { RefreshCameras(); }

void ControlsPage::SetPhoneInVr(bool inVr) {
    if (inVr == m_phoneInVr) {
        return;
    }
    m_phoneInVr = inVr;
    m_settings.enabled = inVr;
    Apply();
}

void ControlsPage::SetGameRunning(bool running) {
    if (running == m_gameRunning) {
        return;
    }
    m_gameRunning = running;
    Apply();
}

bool ControlsPage::Animating() const { return Tracking(); }

void ControlsPage::Draw(const Fonts& fonts) {
    ImGui::PushFont(fonts.title);
    ImGui::TextUnformatted("Controls");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    ImGui::TextUnformatted("Your mouse and keyboard always work. Add other ways to play here.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, S(kTitleGap)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(kCardItemSpacing.x), S(kCardItemSpacing.y)));
    DrawHandGestures(fonts, std::min(ImGui::GetContentRegionAvail().x, S(kMaxCardWidth)));
    ImGui::PopStyleVar();
}

void ControlsPage::DrawHandGestures(const Fonts& fonts, float width) {
    BeginCard("##handgestures", m_scale, width);
    CardTitle(fonts.heading, ICON_HAND "  Hand gestures");
    DrawSwitch();
    ImGui::BeginDisabled(!m_settings.enabled);
    DrawCameraChoice(fonts);
    DrawSpeed(fonts);
    ImGui::EndDisabled();
    TrackingSnapshot snapshot = m_hands.Session().Snapshot();
    DrawStatus(snapshot);
    if (Tracking()) {
        m_preview.Draw(snapshot);
    }
    EndCard();
}

void ControlsPage::DrawSwitch() {
    ImGui::AlignTextToFramePadding();
    ImGui::BeginDisabled(!m_phoneInVr || m_gameRunning);
    if (Toggle("##handson", &m_settings.enabled, m_scale)) {
        Apply();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(m_gameRunning ? "Use your hand as a mouse (paused while a game runs)"
                                         : "Use your hand as a mouse");
    HelpMarker(
        "Use your hands like a touch screen you don't touch.\n\nMove: palm toward the screen, "
        "right index finger up. The pointer goes where your fingertip is in the air in front "
        "of you; small, slow moves fine-tune it.\n\nClick: raise your left index finger to "
        "hold the pointer still, then fold your right index finger (halfway is enough) and "
        "open it again.\n\nScroll: pinch your left thumb and index fingertips together, and "
        "move your right index finger up, down or sideways: the page follows it, as on a "
        "touch screen.\n\nLower your left hand, or let go, to move the pointer again. The left "
        "hand never moves the pointer.\n\nOpen hand: hold all your right fingers open for half "
        "a second to put the pointer right back under your fingertip. The mouse works "
        "alongside: the pointer stays where the mouse puts it until your finger moves.\n\n"
        "Each gesture shows in its own color: "
        "violet moving, amber holding, red clicking, cyan scrolling, green an open hand.",
        S(kHelpWidth));
    if (!m_phoneInVr) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(
            "For playing in the headset: connect the Gazeora app on your phone, with VR on in "
            "Settings, and hand gestures turn on.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
}

void ControlsPage::DrawCameraChoice(const Fonts& fonts) {
    FieldLabel(fonts.caption, "Camera");
    int chosen = ChosenCamera();
    ImGui::SetNextItemWidth(-S(kRefreshButtonWidth));
    const char* preview = m_cameras.empty() ? "No camera found" : nullptr;
    std::string name = preview ? preview : U8(m_cameras[chosen].name);
    if (ImGui::BeginCombo("##camera", name.c_str())) {
        for (int i = 0; i < int(m_cameras.size()); ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(U8(m_cameras[i].name).c_str(), i == chosen) && i != chosen) {
                m_settings.cameraId = m_cameras[i].id;
                Apply();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_REFRESH "  Refresh")) {
        RefreshCameras();
    }
}

void ControlsPage::DrawSpeed(const Fonts& fonts) {
    FieldLabel(fonts.caption, "Pointer speed");
    HelpMarker(
        "How far your fingertip moves to cross the screen: the higher, the shorter. At 5, "
        "about 20 cm, the length of two and a half palms.",
        S(kHelpWidth));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##speed", &m_settings.speed, input::HandGestureSettings::kMinSpeed,
                           input::HandGestureSettings::kMaxSpeed, "%.1f")) {
        m_hands.Session().SetSpeed(m_settings.speed);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_store.SaveHandGestures(m_settings);
    }
}

void ControlsPage::DrawStatus(const TrackingSnapshot& snapshot) {
    TrackingStatusLine status(snapshot);
    ImGui::PushStyleColor(ImGuiCol_Text, status.Color());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(status.Text().c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (snapshot.state == TrackingState::Error && ImGui::Button(ICON_REFRESH "  Try again")) {
        Apply();
    }
}

void ControlsPage::RefreshCameras() { m_cameras = m_hands.Cameras().List(); }

int ControlsPage::ChosenCamera() const {
    for (int i = 0; i < int(m_cameras.size()); ++i) {
        if (m_cameras[i].id == m_settings.cameraId) {
            return i;
        }
    }
    return 0;  // none chosen yet: the first camera, which an empty id opens
}

void ControlsPage::Apply() {
    m_store.SaveHandGestures(m_settings);
    if (Tracking()) {
        m_hands.Session().Start(m_settings);
    } else {
        m_hands.Session().Stop();
    }
}

}  // namespace gazeora::app
