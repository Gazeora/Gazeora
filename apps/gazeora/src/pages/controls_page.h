#pragma once

#include <imgui.h>

#include <string>
#include <vector>

#include "gazeora/input/hand_gesture_services.h"
#include "settings/controls_settings.h"
#include "ui/hand_preview.h"

namespace gazeora::app {

// The Controls page: ways to control the PC besides its own mouse and keyboard, which always
// keep working. Hand gestures through a webcam, for now.
class ControlsPage {
public:
    struct Fonts {
        ImFont* title;
        ImFont* heading;
        ImFont* small;
    };

    ControlsPage(input::HandGestureServices& hands, const ControlsSettings& store, float scale);

    // Lists the cameras.
    void Resume();
    // Whether the Gazeora app on a phone watches the stream with VR on. Hand gestures are for
    // playing in the headset: they switch on when it starts (the user can switch them off
    // again), and off, and can't be switched on, while it doesn't.
    void SetPhoneInVr(bool inVr);
    // Hand gestures pause while a game runs, whatever the switch says, and pick up again as
    // they were once it's closed.
    void SetGameRunning(bool running);
    void Draw(const Fonts& fonts);
    // True while the hand preview moves.
    bool Animating() const;

private:
    void DrawHandGestures(const Fonts& fonts, float width);
    void DrawSwitch();
    void DrawCameraChoice(const Fonts& fonts);
    void DrawSpeed(const Fonts& fonts);
    void DrawStatus(const input::TrackingSnapshot& snapshot);
    void RefreshCameras();
    int ChosenCamera() const;
    // Saves the settings and starts, restarts or stops tracking to match.
    void Apply();
    bool Tracking() const { return m_settings.enabled && !m_gameRunning; }
    float S(float px) const { return px * m_scale; }

    input::HandGestureServices& m_hands;
    const ControlsSettings& m_store;
    float m_scale;
    input::HandGestureSettings m_settings;
    bool m_phoneInVr = false;
    bool m_gameRunning = false;
    std::vector<input::CameraInfo> m_cameras;
    HandPreview m_preview;
};

}  // namespace gazeora::app
