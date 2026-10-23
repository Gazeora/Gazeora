#include "gazeora/host/stream/pointer_message.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gazeora {

namespace {

constexpr float kCoordinateRange = 65535.0f;

uint16_t Get16(const uint8_t* in) { return uint16_t(in[0] << 8 | in[1]); }

void Put16(uint8_t* out, uint16_t value) {
    out[0] = uint8_t(value >> 8);
    out[1] = uint8_t(value);
}

uint16_t Coordinate(float fraction) {
    return uint16_t(std::lround(std::clamp(fraction, 0.0f, 1.0f) * kCoordinateRange));
}

int16_t Wheel(int units) {
    return int16_t(std::clamp<int>(units, std::numeric_limits<int16_t>::min(),
                                   std::numeric_limits<int16_t>::max()));
}

bool KnownAction(uint8_t action) {
    return action >= uint8_t(PointerAction::Move) && action <= uint8_t(PointerAction::Click);
}

bool KnownButton(uint8_t button) { return button <= uint8_t(PointerButton::Right); }

}  // namespace

bool PointerMessage::Read(const uint8_t* payload, size_t size, PointerEvent& out) {
    if (size < kPointerPayloadSize || !KnownAction(payload[kActionAt]) ||
        !KnownButton(payload[kButtonAt])) {
        return false;
    }
    out.action = PointerAction(payload[kActionAt]);
    out.button = PointerButton(payload[kButtonAt]);
    out.x = Get16(payload + kXAt) / kCoordinateRange;
    out.y = Get16(payload + kYAt) / kCoordinateRange;
    out.wheelVertical = int16_t(Get16(payload + kWheelVerticalAt));
    out.wheelHorizontal = int16_t(Get16(payload + kWheelHorizontalAt));
    return true;
}

std::string PointerMessage::Write(const PointerEvent& event) {
    uint8_t payload[kPointerPayloadSize];
    payload[kActionAt] = uint8_t(event.action);
    payload[kButtonAt] = uint8_t(event.button);
    Put16(payload + kXAt, Coordinate(event.x));
    Put16(payload + kYAt, Coordinate(event.y));
    Put16(payload + kWheelVerticalAt, uint16_t(Wheel(event.wheelVertical)));
    Put16(payload + kWheelHorizontalAt, uint16_t(Wheel(event.wheelHorizontal)));
    FrameHeader header;
    header.type = FrameMessage::Pointer;
    return StreamMessage(header, std::string(reinterpret_cast<char*>(payload), sizeof(payload)));
}

}  // namespace gazeora
