#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "gazeora/host/stream/frame_packet.h"
#include "gazeora/host/stream/pointer_event.h"

namespace gazeora {

// The payload of a GZF1 Pointer message (frame_packet.h), big-endian, after the header whose
// last word is the payload length (over UDP and USB alike):
//    0  u8   action (PointerAction)
//    1  u8   button (PointerButton)
//    2  u16  x across the picture, 0..65535
//    4  u16  y down the picture, 0..65535
//    6  i16  vertical wheel, Windows units
//    8  i16  horizontal wheel, Windows units
// The Android app has its own copy (protocols/pointer_message.h); keep the two in step.
constexpr size_t kPointerPayloadSize = 10;
// Where the app sends pointer messages over the network, whatever the stream's protocol.
constexpr uint16_t kPointerPort = 8557;

// Pointer messages to and from bytes.
class PointerMessage {
public:
    // False if the payload is short or its action or button is unknown.
    static bool Read(const uint8_t* payload, size_t size, PointerEvent& out);
    // A whole message, header and payload, as the app sends it (for tests).
    static std::string Write(const PointerEvent& event);

private:
    static constexpr size_t kActionAt = 0;
    static constexpr size_t kButtonAt = 1;
    static constexpr size_t kXAt = 2;
    static constexpr size_t kYAt = 4;
    static constexpr size_t kWheelVerticalAt = 6;
    static constexpr size_t kWheelHorizontalAt = 8;
};

}  // namespace gazeora
