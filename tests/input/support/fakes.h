#pragma once

#include <cmath>
#include <deque>
#include <optional>
#include <vector>

#include "gazeora/input/interfaces/camera_catalog.h"
#include "gazeora/input/interfaces/hand_tracker.h"
#include "gazeora/input/interfaces/hand_tracker_factory.h"
#include "gazeora/input/interfaces/monotonic_clock.h"
#include "gazeora/input/interfaces/pointer_device.h"
#include "support/hands.h"

namespace gazeora::input::testing {

// A pointer that remembers what it was told; `position` can be moved as the mouse would.
class FakePointer final : public IPointerDevice {
public:
    Vec2 Position() const override { return position; }
    float ScreenAspect() const override { return 1.0f; }
    void MoveTo(Vec2 to) override {
        position = to;
        moves.push_back(to);
    }
    void Click() override { ++clicks; }
    void Scroll(int vertical, int horizontal) override {
        scrolledUp += vertical;
        scrolledRight += horizontal;
    }

    Vec2 position{0.5f, 0.5f};
    std::vector<Vec2> moves;
    int clicks = 0;
    int scrolledUp = 0;  // wheel units, as Scroll turned them
    int scrolledRight = 0;
};

// Hands out the hands it was given, one picture's at a time, then none.
class ScriptedTracker final : public IHandTracker {
public:
    static constexpr int kRightHand = 0;
    static constexpr int kLeftHand = 1;

    std::vector<TrackedHand> Track(const CameraFrame&) override {
        if (pictures.empty()) {
            return {};
        }
        std::vector<TrackedHand> hands = pictures.front();
        pictures.pop_front();
        return hands;
    }
    // A picture with `hand` alone, or no hand.
    void Add(const std::optional<HandLandmarks>& hand) {
        pictures.push_back(hand ? std::vector<TrackedHand>{{kRightHand, *hand}}
                                : std::vector<TrackedHand>{});
    }
    // A picture with both hands.
    void Add(const HandLandmarks& right, const HandLandmarks& left) {
        pictures.push_back({{kRightHand, right}, {kLeftHand, left}});
    }

    std::deque<std::vector<TrackedHand>> pictures;
};

// A camera that sends the same blank picture forever.
class BlankCamera final : public ICameraSource {
public:
    bool Read(CameraFrame& frame) override {
        frame = CameraFrame(kSide, kSide, std::vector<uint8_t>(kSide * kSide * 4));
        return true;
    }
    static constexpr int kSide = 1000;
};

class FakeCameraCatalog final : public ICameraCatalog {
public:
    std::vector<CameraInfo> List() const override { return {{L"fake", L"Fake camera"}}; }
    std::unique_ptr<ICameraSource> Open(const std::wstring&) const override {
        if (failure) {
            throw InputError(failure);
        }
        return std::make_unique<BlankCamera>();
    }

    const wchar_t* failure = nullptr;
};

// Trackers that always see the same pointing hand.
class SteadyHandTrackerFactory final : public IHandTrackerFactory {
public:
    class Tracker final : public IHandTracker {
    public:
        std::vector<TrackedHand> Track(const CameraFrame&) override {
            return {{0, Hands::Pointing({0.0f, -1.0f, 0.0f})}};
        }
    };
    std::unique_ptr<IHandTracker> Create() override { return std::make_unique<Tracker>(); }
};

class ManualClock final : public IMonotonicClock {
public:
    double NowSeconds() const override { return now; }
    double now = 0.0;
};

}  // namespace gazeora::input::testing
