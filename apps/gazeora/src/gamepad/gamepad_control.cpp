#include "gamepad/gamepad_control.h"

namespace gazeora::app {

GamepadControl::GamepadControl(input::GamepadSession& session, const ControlsSettings& store)
    : m_session(session), m_store(store), m_enabled(store.LoadGamepadEnabled()) {}

void GamepadControl::Resume() { Apply(); }

void GamepadControl::SetEnabled(bool enabled) {
    m_enabled = enabled;
    m_store.SaveGamepadEnabled(enabled);
    Apply();
}

void GamepadControl::SetGameRunning(bool running) {
    if (running == m_gameRunning) {
        return;
    }
    m_gameRunning = running;
    Apply();
}

bool GamepadControl::Connected() const {
    return m_session.Running() && !m_session.Snapshot().connected.empty();
}

void GamepadControl::Apply() {
    if (Reading()) {
        m_session.Start();
    } else {
        m_session.Stop();
    }
}

}  // namespace gazeora::app
