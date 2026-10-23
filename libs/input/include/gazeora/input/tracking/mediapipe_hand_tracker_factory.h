#pragma once

#include <string>

#include "gazeora/input/interfaces/hand_tracker_factory.h"
#include "gazeora/input/interfaces/inference_engine.h"

namespace gazeora::input {

// Factory pattern: MediaPipe hand trackers, their two models loaded through `engine`.
class MediaPipeHandTrackerFactory final : public IHandTrackerFactory {
public:
    MediaPipeHandTrackerFactory(IInferenceEngine& engine, std::wstring palmModelPath,
                                std::wstring landmarkModelPath)
        : m_engine(engine),
          m_palmModelPath(std::move(palmModelPath)),
          m_landmarkModelPath(std::move(landmarkModelPath)) {}

    std::unique_ptr<IHandTracker> Create() override;

private:
    IInferenceEngine& m_engine;
    std::wstring m_palmModelPath;
    std::wstring m_landmarkModelPath;
};

}  // namespace gazeora::input
