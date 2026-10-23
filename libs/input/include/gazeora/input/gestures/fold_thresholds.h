#pragma once

namespace gazeora::input {

// When a finger counts as straight or folded, and how gestures are timed. Straight fingers
// measure 0.99 to 1 (FingerStraightness, on MediaPipe's sample photos) and folded ones 0.26
// to 0.54; a finger bent halfway, about 0.75, already counts as folded. Their FingerReach is
// 0.6 to 0.85 straight and 0.2 to 0.4 folded, from the front, the side or tipped. Between the
// lines a finger keeps what it was, so it doesn't flicker.
struct FoldThresholds {
    float folded = 0.8f;          // FingerStraightness below this: folded, at least halfway
    float extended = 0.9f;        // above, and reaching out: straight
    float foldedReach = 0.45f;    // FingerReach below this: folded, however straight it looks
    float extendedReach = 0.55f;  // above, and straight: straight
    // With the index and middle fingers up, the middle fingertip is near the index one in the
    // picture: 0.2 palm lengths apart typically held together (on video of a hand seen from
    // the knuckles), 0.4 in a V; with the middle finger folded, 0.6 typically, however
    // straight the model draws it. Up to this far apart, it can count as up.
    float togetherPalms = 0.5f;
    // All fingers open this long puts the pointer back under the fingertip. Seen from the
    // knuckles, the model draws a fist open for up to a quarter of a second at a time.
    double openHoldSeconds = 0.5;
    // The thumb and index fingertips closer than this in the picture, in palm lengths, pinch
    // (0.03 to 0.24 in a user's pinches), and further than the second, part. On video without
    // pinches they came this close only briefly, so a pinch has to last a moment.
    float pinchedPalms = 0.25f;
    float releasedPalms = 0.35f;
    // A fist brings the fingertips close too, but its thumb lies across the index knuckle
    // (0.01 to 0.11 palm lengths from it, on fists seen from many sides; 0.37 and more in a
    // user's pinches), and its index tip curls into the palm (0.34 to 0.49 from the palm's
    // middle; 0.59 and more in pinches). Nearer than these, it's no pinch.
    float fistThumbPalms = 0.25f;
    float fistTipPalms = 0.52f;
    // The left hand pinched this long turns scrolling on: a moment, so a pinch passing by on
    // its way to a fist doesn't.
    double scrollOnSeconds = 0.1;
    // How far each picture's guess at which hand it is moves the hand's average
    // (HandRoles): about a third of a second to be sure of a new hand.
    float handednessBlend = 0.2f;
    // Tracking drops a picture now and then; only a longer gap counts as a hand leaving.
    double lostHandSeconds = 0.3;
    // Folded for longer than this is resting, not clicking.
    double maxClickSeconds = 1.0;
    // The left index finger up this long holds the pointer: a glimpse of it, misread in a few
    // pictures, mustn't stop the pointer.
    double holdDelaySeconds = 0.2;
};

}  // namespace gazeora::input
