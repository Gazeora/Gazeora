#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "gazeora/input/interfaces/camera_catalog.h"
#include "gazeora/input/interfaces/hand_tracker_factory.h"
#include "gazeora/input/interfaces/monotonic_clock.h"
#include "gazeora/input/interfaces/pointer_device.h"
#include "gazeora/input/models/hand_gesture_settings.h"
#include "gazeora/input/models/tracking_snapshot.h"
#include "gazeora/input/services/frame_rate_meter.h"
#include "gazeora/input/services/hand_gesture_controller.h"

namespace gazeora::input {

// Hand gesture control running in the background: opens the camera and loads the models on
// its own thread, then turns each picture into pointer moves and clicks until stopped.
class HandGestureSession {
public:
    HandGestureSession(const ICameraCatalog& cameras, IHandTrackerFactory& trackers,
                       IPointerDevice& pointer, const IMonotonicClock& clock);
    ~HandGestureSession();

    HandGestureSession(const HandGestureSession&) = delete;
    HandGestureSession& operator=(const HandGestureSession&) = delete;

    // Starts over with `settings`' camera, stopping a session already running.
    void Start(const HandGestureSettings& settings);
    void Stop();
    // Takes effect from the next picture.
    void SetSpeed(float speed) { m_speed = speed; }
    bool Running() const { return m_thread.joinable(); }
    TrackingSnapshot Snapshot() const;

private:
    void Run(std::wstring cameraId);
    void Loop(ICameraSource& camera, HandGestureController& controller);
    void Publish(TrackingSnapshot snapshot);
    void Fail(std::wstring message);

    const ICameraCatalog& m_cameras;
    IHandTrackerFactory& m_trackers;
    IPointerDevice& m_pointer;
    const IMonotonicClock& m_clock;

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<float> m_speed{HandGestureSettings::kDefaultSpeed};
    mutable std::mutex m_mutex;
    TrackingSnapshot m_snapshot;  // guarded by m_mutex
};

}  // namespace gazeora::input
