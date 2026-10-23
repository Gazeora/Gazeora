#pragma once

#include "gazeora/input/platform/media_foundation_camera_catalog.h"
#include "gazeora/input/platform/onnx_runtime_engine.h"
#include "gazeora/input/platform/runtime_files.h"
#include "gazeora/input/platform/send_input_pointer.h"
#include "gazeora/input/platform/steady_clock.h"
#include "gazeora/input/services/hand_gesture_session.h"
#include "gazeora/input/tracking/mediapipe_hand_tracker_factory.h"

namespace gazeora::input {

// Facade pattern: hand gesture control wired to the real system (webcams, ONNX Runtime,
// the Windows pointer). The launcher's composition root for it.
class HandGestureServices final {
public:
    HandGestureServices()
        : m_engine(RuntimeFiles::OnnxRuntimeDll()),
          m_trackers(m_engine, RuntimeFiles::PalmModel(), RuntimeFiles::LandmarkModel()),
          m_session(m_cameras, m_trackers, m_pointer, m_clock) {}

    HandGestureServices(const HandGestureServices&) = delete;
    HandGestureServices& operator=(const HandGestureServices&) = delete;

    const ICameraCatalog& Cameras() const { return m_cameras; }
    HandGestureSession& Session() { return m_session; }

private:
    // Declared before m_session, which holds references to them.
    MediaFoundationCameraCatalog m_cameras;
    OnnxRuntimeEngine m_engine;
    MediaPipeHandTrackerFactory m_trackers;
    SendInputPointer m_pointer;
    SteadyClock m_clock;
    HandGestureSession m_session;
};

}  // namespace gazeora::input
