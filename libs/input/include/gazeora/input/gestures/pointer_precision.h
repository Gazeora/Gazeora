#pragma once

namespace gazeora::input {

// Pointer precision, as Windows does it for a mouse: slow moves take only part of the
// pointer's speed, so aiming and the shake of the fingertip move it little, and quick moves
// take all of it.
class PointerPrecision {
public:
    // The share of the full speed for a fingertip moving `palmsPerSecond`.
    float Share(float palmsPerSecond) const;
    // How quick a move is: 0 creeping along, 1 at full speed and above.
    float Quickness(float palmsPerSecond) const;
};

}  // namespace gazeora::input
