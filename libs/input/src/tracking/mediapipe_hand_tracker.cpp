#include "gazeora/input/tracking/mediapipe_hand_tracker.h"

#include <algorithm>

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

// Besides the likeliest palm, the detector's next guesses get a look: something palm-like,
// such as a print on a shirt, can outscore a hand seen from an unusual side, and the landmark
// model tells them apart.
constexpr size_t kPalmsToTry = 3;
// While one hand is followed, the detector looks for the other every this many pictures, so
// a second hand turns up within a tenth of a second without slowing every picture.
constexpr int kDetectEveryPictures = 3;
// Two regions whose middles are closer than this share of their size are on the same hand.
constexpr float kSameHandShare = 0.5f;

IInferenceModel& Required(const std::unique_ptr<IInferenceModel>& model) {
    if (!model) {
        throw InputError(L"A hand tracking model is missing.");
    }
    return *model;
}

}  // namespace

MediaPipeHandTracker::MediaPipeHandTracker(std::unique_ptr<IInferenceModel> palmModel,
                                           std::unique_ptr<IInferenceModel> landmarkModel,
                                           float minConfidence)
    : m_palmModel(std::move(palmModel)),
      m_landmarkModel(std::move(landmarkModel)),
      m_palms(Required(m_palmModel), m_sampler),
      m_landmarks(Required(m_landmarkModel), m_sampler),
      m_minConfidence(minConfidence) {}

std::vector<TrackedHand> MediaPipeHandTracker::Track(const CameraFrame& frame) {
    std::vector<TrackedHand> hands;
    Follow(frame, hands);
    if (m_tracks.size() < kMaxHands && TimeToDetect()) {
        Detect(frame, hands);
    }
    return hands;
}

void MediaPipeHandTracker::Follow(const CameraFrame& frame, std::vector<TrackedHand>& hands) {
    for (size_t i = 0; i < m_tracks.size();) {
        HandTrack& track = m_tracks[i];
        std::optional<HandLandmarks> hand = EstimateIn(frame, *track.roi, track);
        if (!hand || Covered(*track.roi, i)) {
            m_tracks.erase(m_tracks.begin() + std::ptrdiff_t(i));
            continue;
        }
        hands.push_back({track.id, *hand});
        ++i;
    }
}

void MediaPipeHandTracker::Detect(const CameraFrame& frame, std::vector<TrackedHand>& hands) {
    for (const Palm& palm : m_palms.Detect(frame, kPalmsToTry)) {
        HandRoi found = m_rois.FromPalm(palm);
        if (m_tracks.size() >= kMaxHands || Covered(found, m_tracks.size())) {
            continue;
        }
        HandTrack track{m_nextId};
        // Once more from the region the next pictures will use: the detector's region places
        // the joints a few pixels differently, which would look like a move.
        if (!EstimateIn(frame, found, track)) {
            continue;
        }
        std::optional<HandLandmarks> hand = EstimateIn(frame, *track.roi, track);
        if (hand && !Covered(*track.roi, m_tracks.size())) {
            hands.push_back({m_nextId++, *hand});
            m_tracks.push_back(std::move(track));
        }
    }
}

bool MediaPipeHandTracker::TimeToDetect() {
    if (m_tracks.empty() || ++m_picturesSinceDetect >= kDetectEveryPictures) {
        m_picturesSinceDetect = 0;
        return true;
    }
    return false;
}

std::optional<HandLandmarks> MediaPipeHandTracker::EstimateIn(const CameraFrame& frame,
                                                              const HandRoi& roi,
                                                              HandTrack& track) {
    HandLandmarks hand = m_landmarks.Estimate(frame, roi);
    if (hand.Confidence() < m_minConfidence) {
        return std::nullopt;
    }
    track.roi = track.stabilizer.Update(m_rois.FromLandmarks(hand));
    return hand;
}

bool MediaPipeHandTracker::Covered(const HandRoi& roi, size_t count) const {
    for (size_t i = 0; i < count; ++i) {
        const HandRoi& other = *m_tracks[i].roi;
        float within = kSameHandShare * std::max(roi.Size(), other.Size());
        if ((roi.Center() - other.Center()).Length() < within) {
            return true;
        }
    }
    return false;
}

}  // namespace gazeora::input
