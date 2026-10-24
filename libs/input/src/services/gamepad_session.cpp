#include "gazeora/input/services/gamepad_session.h"

#include <chrono>

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

// 125 polls a second: quicker than the UI draws, and next to nothing for the CPU.
constexpr auto kPollInterval = std::chrono::milliseconds(8);

}  // namespace

GamepadSession::GamepadSession(IGamepadDriverFactory& drivers, IGamepadListener& listener)
    : m_drivers(drivers), m_listener(listener) {}

GamepadSession::~GamepadSession() { Stop(); }

void GamepadSession::Start() {
    Stop();
    m_stop = false;
    GamepadSnapshot running;
    running.state = GamepadSessionState::Running;
    Publish(running);
    m_thread = std::thread(&GamepadSession::Run, this);
}

void GamepadSession::Stop() {
    m_stop = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    Publish(GamepadSnapshot());
}

GamepadSnapshot GamepadSession::Snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void GamepadSession::Run() {
    try {
        std::unique_ptr<IGamepadDriver> driver = m_drivers.Create();
        Loop(*driver);
    } catch (const InputError& error) {
        Fail(error.Message());
    } catch (const std::exception&) {
        Fail(L"Game controllers stopped working unexpectedly.");
    }
    Report(GamepadState());
}

void GamepadSession::Loop(IGamepadDriver& driver) {
    while (!m_stop) {
        Poll(driver);
        std::this_thread::sleep_for(kPollInterval);
    }
}

void GamepadSession::Poll(IGamepadDriver& driver) {
    driver.Update();
    PublishConnected(driver.Connected());
    Report(m_filter.Apply(driver.State()));
}

void GamepadSession::Report(const GamepadState& state) {
    if (state == m_reported) {
        return;
    }
    m_reported = state;
    m_listener.OnGamepadState(state);
}

void GamepadSession::PublishConnected(std::vector<GamepadInfo> connected) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_snapshot.connected != connected) {
        m_snapshot.connected = std::move(connected);
    }
}

void GamepadSession::Publish(GamepadSnapshot snapshot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot = std::move(snapshot);
}

void GamepadSession::Fail(std::wstring message) {
    GamepadSnapshot failed;
    failed.state = GamepadSessionState::Error;
    failed.message = std::move(message);
    Publish(std::move(failed));
}

}  // namespace gazeora::input
