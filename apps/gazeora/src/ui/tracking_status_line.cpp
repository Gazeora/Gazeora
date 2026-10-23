#include "ui/tracking_status_line.h"

#include <cstdio>

#include "ui/hand_preview.h"
#include "ui/text_format.h"
#include "ui/theme.h"

namespace gazeora::app {

namespace {

using input::TrackingSnapshot;
using input::TrackingState;

constexpr size_t kMaxText = 128;

}  // namespace

TrackingStatusLine::TrackingStatusLine(const TrackingSnapshot& snapshot) : m_color(kTextDim) {
    Describe(snapshot);
}

const char* TrackingStatusLine::ModeText(input::GestureMode mode) {
    switch (mode) {
        case input::GestureMode::Steering:
            return "Moving with your right index finger";
        case input::GestureMode::Holding:
            return "Holding: fold your right index finger and open it to click";
        case input::GestureMode::Folded:
            return "Folded: open your right index finger to click";
        case input::GestureMode::Scrolling:
            return "Scrolling: move your index finger; open the left pinch to stop";
        case input::GestureMode::Resting:
            return "Starting";
    }
    return "";
}

// An open hand is held to bring the pointer back under the fingertip.
const char* TrackingStatusLine::GestureText(const TrackingSnapshot& snapshot) {
    return snapshot.handOpen ? "Open hand: hold it to bring the pointer under your finger"
                             : ModeText(snapshot.mode);
}

ImU32 TrackingStatusLine::GestureColor(const TrackingSnapshot& snapshot) {
    return snapshot.handOpen ? kSuccess : HandPreview::ColorOf(snapshot.mode);
}

void TrackingStatusLine::Describe(const TrackingSnapshot& snapshot) {
    char text[kMaxText];
    switch (snapshot.state) {
        case TrackingState::Off:
            m_text = "Off";
            m_color = kTextFaint;
            return;
        case TrackingState::Starting:
            m_text = "Starting the camera...";
            return;
        case TrackingState::NoHand:
            snprintf(text, sizeof(text), "Show your right hand to the camera  \xC2\xB7  %.0f fps",
                     snapshot.framesPerSecond);
            m_text = text;
            m_color = kWarning;
            return;
        case TrackingState::Tracking:
            snprintf(text, sizeof(text), "%s  \xC2\xB7  %.0f fps  \xC2\xB7  %d %s",
                     GestureText(snapshot), snapshot.framesPerSecond, snapshot.clicks,
                     snapshot.clicks == 1 ? "click" : "clicks");
            m_text = text;
            m_color = GestureColor(snapshot);
            return;
        case TrackingState::Error:
            m_text = U8(snapshot.message);
            m_color = kDanger;
            return;
    }
}

}  // namespace gazeora::app
