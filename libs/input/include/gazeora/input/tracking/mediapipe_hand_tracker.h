#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "gazeora/input/interfaces/hand_tracker.h"
#include "gazeora/input/interfaces/inference_model.h"
#include "gazeora/input/tracking/image_sampler.h"
#include "gazeora/input/tracking/landmark_estimator.h"
#include "gazeora/input/tracking/palm_detector.h"
#include "gazeora/input/tracking/roi_calculator.h"
#include "gazeora/input/tracking/roi_stabilizer.h"

namespace gazeora::input {

// MediaPipe's two-step hand tracking, for up to two hands: the palm detector finds a hand,
// then the landmark model follows it from picture to picture around where it last was. The
// slower detector only looks again while fewer hands than that are followed (every few
// pictures while one is).
class MediaPipeHandTracker final : public IHandTracker {
public:
    static constexpr float kDefaultMinConfidence = 0.5f;

    MediaPipeHandTracker(std::unique_ptr<IInferenceModel> palmModel,
                         std::unique_ptr<IInferenceModel> landmarkModel,
                         float minConfidence = kDefaultMinConfidence);

    std::vector<TrackedHand> Track(const CameraFrame& frame) override;

private:
    // One hand being followed: where to look for it next.
    struct HandTrack {
        int id;
        std::optional<HandRoi> roi;
        RoiStabilizer stabilizer;
    };

    // Follows each hand into `frame`, dropping those lost or that ran into another.
    void Follow(const CameraFrame& frame, std::vector<TrackedHand>& hands);
    // Looks for hands not followed yet.
    void Detect(const CameraFrame& frame, std::vector<TrackedHand>& hands);
    bool TimeToDetect();
    // The joints in `roi`, setting where `track` looks next; nothing if no hand is there.
    std::optional<HandLandmarks> EstimateIn(const CameraFrame& frame, const HandRoi& roi,
                                            HandTrack& track);
    // Whether `roi` is on a hand already followed (one of the first `count` tracks).
    bool Covered(const HandRoi& roi, size_t count) const;

    // Declared before the detector and estimator, which hold references to them.
    std::unique_ptr<IInferenceModel> m_palmModel;
    std::unique_ptr<IInferenceModel> m_landmarkModel;
    ImageSampler m_sampler;
    PalmDetector m_palms;
    LandmarkEstimator m_landmarks;
    RoiCalculator m_rois;
    float m_minConfidence;
    std::vector<HandTrack> m_tracks;
    int m_nextId = 0;
    int m_picturesSinceDetect = 0;
};

}  // namespace gazeora::input
