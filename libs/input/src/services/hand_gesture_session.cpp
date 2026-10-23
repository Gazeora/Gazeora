#include "gazeora/input/services/hand_gesture_session.h"

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

HandGestureSession::HandGestureSession(const ICameraCatalog& cameras, IHandTrackerFactory& trackers,
                                       IPointerDevice& pointer, const IMonotonicClock& clock)
    : m_cameras(cameras), m_trackers(trackers), m_pointer(pointer), m_clock(clock) {}

HandGestureSession::~HandGestureSession() { Stop(); }

void HandGestureSession::Start(const HandGestureSettings& settings) {
    Stop();
    m_stop = false;
    m_speed = settings.speed;
    TrackingSnapshot starting;
    starting.state = TrackingState::Starting;
    Publish(starting);
    m_thread = std::thread(&HandGestureSession::Run, this, settings.cameraId);
}

void HandGestureSession::Stop() {
    m_stop = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    Publish(TrackingSnapshot());
}

TrackingSnapshot HandGestureSession::Snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void HandGestureSession::Run(std::wstring cameraId) {
    try {
        std::unique_ptr<IHandTracker> tracker = m_trackers.Create();
        std::unique_ptr<ICameraSource> camera = m_cameras.Open(cameraId);
        HandGestureController controller(*tracker, m_pointer, m_speed);
        Loop(*camera, controller);
    } catch (const InputError& error) {
        Fail(error.Message());
    } catch (const std::exception&) {
        Fail(L"Hand tracking stopped unexpectedly.");
    }
}

void HandGestureSession::Loop(ICameraSource& camera, HandGestureController& controller) {
    FrameRateMeter rate;
    CameraFrame frame;
    while (!m_stop) {
        if (!camera.Read(frame)) {
            continue;
        }
        double now = m_clock.NowSeconds();
        controller.SetSpeed(m_speed);
        TrackingSnapshot snapshot = controller.Process(frame, now);
        rate.Tick(now);
        snapshot.framesPerSecond = rate.FramesPerSecond();
        Publish(std::move(snapshot));
    }
}

void HandGestureSession::Publish(TrackingSnapshot snapshot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot = std::move(snapshot);
}

void HandGestureSession::Fail(std::wstring message) {
    TrackingSnapshot failed;
    failed.state = TrackingState::Error;
    failed.message = std::move(message);
    Publish(std::move(failed));
}

}  // namespace gazeora::input
