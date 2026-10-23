#include "gazeora/input/tracking/mediapipe_hand_tracker_factory.h"

#include "gazeora/input/tracking/mediapipe_hand_tracker.h"

namespace gazeora::input {

std::unique_ptr<IHandTracker> MediaPipeHandTrackerFactory::Create() {
    return std::make_unique<MediaPipeHandTracker>(m_engine.Load(m_palmModelPath),
                                                  m_engine.Load(m_landmarkModelPath));
}

}  // namespace gazeora::input
