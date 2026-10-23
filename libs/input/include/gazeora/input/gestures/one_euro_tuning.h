#pragma once

namespace gazeora::input {

// OneEuroFilter's settings. The defaults suit finger directions (unit vectors), whose
// tracking noise is about a degree a picture.
struct OneEuroTuning {
    float minCutoffHz = 0.5f;  // smoothing at rest: lower is steadier
    float beta = 3.0f;         // how fast turning lifts the cutoff: higher lags less
    float speedCutoffHz = 1.0f;
};

}  // namespace gazeora::input
