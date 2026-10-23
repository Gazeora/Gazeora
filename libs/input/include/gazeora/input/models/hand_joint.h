#pragma once

namespace gazeora::input {

// MediaPipe's 21 hand landmarks, in the model's output order.
enum class HandJoint {
    Wrist,
    ThumbCmc,
    ThumbMcp,
    ThumbIp,
    ThumbTip,
    IndexMcp,
    IndexPip,
    IndexDip,
    IndexTip,
    MiddleMcp,
    MiddlePip,
    MiddleDip,
    MiddleTip,
    RingMcp,
    RingPip,
    RingDip,
    RingTip,
    PinkyMcp,
    PinkyPip,
    PinkyDip,
    PinkyTip,
};

inline constexpr int kHandJointCount = 21;

}  // namespace gazeora::input
