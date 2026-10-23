#pragma once

#include <array>

#include "gazeora/input/models/hand_joint.h"
#include "gazeora/input/models/landmark.h"
#include "gazeora/input/models/vec3.h"

namespace gazeora::input {

// The 21 joints of one tracked hand, how sure the model is that a hand is there, and that it's
// a right hand (in the camera's own, unmirrored picture; 0 for a left one). Besides the joints
// in the picture, the model gives them in 3D around the hand's middle, in metres: its guess at
// the hand's real shape, which keeps depth better than the picture's z.
class HandLandmarks {
public:
    using Joints = std::array<Landmark, kHandJointCount>;
    using WorldJoints = std::array<Vec3, kHandJointCount>;

    static constexpr float kRightHand = 1.0f;
    static constexpr float kLeftHand = 0.0f;

    // The joints in the picture stand in for the 3D ones.
    HandLandmarks(Joints joints, float confidence, float rightHand = kRightHand)
        : HandLandmarks(joints, WorldOf(joints), confidence, rightHand) {}
    HandLandmarks(Joints joints, WorldJoints world, float confidence, float rightHand = kRightHand)
        : m_joints(joints), m_world(world), m_confidence(confidence), m_rightHand(rightHand) {}

    const Landmark& At(HandJoint joint) const { return m_joints[int(joint)]; }
    const Joints& All() const { return m_joints; }
    Vec3 World(HandJoint joint) const { return m_world[int(joint)]; }
    const WorldJoints& AllWorld() const { return m_world; }
    float Confidence() const { return m_confidence; }
    float RightHand() const { return m_rightHand; }

private:
    static WorldJoints WorldOf(const Joints& joints) {
        WorldJoints world;
        for (int i = 0; i < kHandJointCount; ++i) {
            world[i] = joints[i].Xyz();
        }
        return world;
    }

    Joints m_joints;
    WorldJoints m_world;
    float m_confidence;
    float m_rightHand;
};

}  // namespace gazeora::input
