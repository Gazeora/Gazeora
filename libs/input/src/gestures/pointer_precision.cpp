#include "gazeora/input/gestures/pointer_precision.h"

#include <algorithm>

namespace gazeora::input {

namespace {

// A fingertip creeping along gets this share...
constexpr float kSlowShare = 0.3f;
// ...rising evenly to all of it at this speed: a quick turn of the finger, about a quarter of
// a second to cross the screen.
constexpr float kFullSpeedPalmsPerSecond = 2.0f;

}  // namespace

float PointerPrecision::Share(float palmsPerSecond) const {
    return kSlowShare + (1.0f - kSlowShare) * Quickness(palmsPerSecond);
}

float PointerPrecision::Quickness(float palmsPerSecond) const {
    return std::clamp(palmsPerSecond / kFullSpeedPalmsPerSecond, 0.0f, 1.0f);
}

}  // namespace gazeora::input
