// Telling the hands apart, and what the left one says.
#include <gtest/gtest.h>

#include "gazeora/input/gestures/hand_roles.h"
#include "gazeora/input/gestures/left_hand_signs.h"
#include "support/hands.h"

namespace gazeora::input {
namespace {

using testing::Hands;

const Vec3 kUp{0.0f, -1.0f, 0.0f};
// The left hand, on the camera's right.
const Vec3 kLeftSide{300.0f, 0.0f, 0.0f};

TEST(HandRolesTest, TheRightHandMovesThePointer) {
    FoldThresholds thresholds;
    HandRoles roles(thresholds);
    HandLandmarks right = Hands::Pointing(kUp);
    HandLandmarks left = Hands::AsLeft(Hands::Pointing(kUp, kLeftSide));
    HandRoles::Roles both = roles.Assign({{7, left}, {3, right}});
    EXPECT_EQ(both.pointer->id, 3);
    EXPECT_EQ(both.left->id, 7);
}

TEST(HandRolesTest, ALeftHandAloneNeverMovesThePointer) {
    FoldThresholds thresholds;
    HandRoles roles(thresholds);
    HandRoles::Roles alone = roles.Assign({{7, Hands::AsLeft(Hands::Pointing(kUp))}});
    EXPECT_FALSE(alone.pointer);
    EXPECT_EQ(alone.left->id, 7);
}

TEST(HandRolesTest, OneOddGuessDoesntSwapTheHands) {
    FoldThresholds thresholds;
    HandRoles roles(thresholds);
    HandLandmarks right = Hands::Pointing(kUp);
    for (int picture = 0; picture < 10; ++picture) {
        roles.Assign({{3, right}});
    }
    EXPECT_EQ(roles.Assign({{3, Hands::AsLeft(right)}}).pointer->id, 3);
}

TEST(HandRolesTest, OfTwoTakenForRightTheOneOnTheUsersLeftIsTheLeftHand) {
    FoldThresholds thresholds;
    HandRoles roles(thresholds);
    HandRoles::Roles both =
        roles.Assign({{7, Hands::Pointing(kUp, kLeftSide)}, {3, Hands::Pointing(kUp)}});
    EXPECT_EQ(both.pointer->id, 3);
    EXPECT_EQ(both.left->id, 7);
}

TEST(LeftHandSignsTest, APinchScrollsAndTheIndexUpHolds) {
    FoldThresholds thresholds;
    LeftHandSigns signs(thresholds);
    HandLandmarks pinched = Hands::ThumbTo(Hands::Pointing(kUp, kLeftSide));
    HandLandmarks indexUp = Hands::Pointing(kUp, kLeftSide);
    EXPECT_EQ(signs.Update(&pinched, 0.0), LeftSign::None);  // not yet
    EXPECT_EQ(signs.Update(&pinched, 0.15), LeftSign::Pinch);
    EXPECT_EQ(signs.Update(&indexUp, 0.2), LeftSign::None);  // the pinch ends at once
    EXPECT_EQ(signs.Update(&indexUp, 0.45), LeftSign::IndexUp);
}

TEST(LeftHandSignsTest, NothingElseMeansAnything) {
    FoldThresholds thresholds;
    LeftHandSigns signs(thresholds);
    for (const HandLandmarks& hand :
         {Hands::Open(), Hands::Folding(testing::kStraight, testing::kStraight),
          Hands::Folding(testing::kFolded, testing::kFolded)}) {
        EXPECT_EQ(signs.Update(&hand, 0.0), LeftSign::None);
        EXPECT_EQ(signs.Update(&hand, 1.0), LeftSign::None);
    }
}

TEST(LeftHandSignsTest, AHandOutOfViewSaysNothingAfterADroppedPicture) {
    FoldThresholds thresholds;
    LeftHandSigns signs(thresholds);
    HandLandmarks pinched = Hands::ThumbTo(Hands::Pointing(kUp, kLeftSide));
    signs.Update(&pinched, 0.0);
    signs.Update(&pinched, 0.15);
    EXPECT_EQ(signs.Update(nullptr, 0.3), LeftSign::Pinch);
    EXPECT_EQ(signs.Update(nullptr, 0.6), LeftSign::None);
}

}  // namespace
}  // namespace gazeora::input
