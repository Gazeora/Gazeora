#include "gazeora/input/platform/send_input_pointer.h"

#include <windows.h>

#include <algorithm>

namespace gazeora::input {

namespace {

// SendInput's absolute coordinates span the main screen from 0 to 65535.
constexpr float kAbsoluteRange = 65535.0f;
constexpr float kPixelCenter = 0.5f;

INPUT MouseInput(DWORD flags) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    return input;
}

}  // namespace

Vec2 SendInputPointer::Position() const {
    POINT cursor = {};
    GetCursorPos(&cursor);
    float width = float(std::max(GetSystemMetrics(SM_CXSCREEN), 1));
    float height = float(std::max(GetSystemMetrics(SM_CYSCREEN), 1));
    return {(cursor.x + kPixelCenter) / width, (cursor.y + kPixelCenter) / height};
}

float SendInputPointer::ScreenAspect() const {
    float width = float(std::max(GetSystemMetrics(SM_CXSCREEN), 1));
    float height = float(std::max(GetSystemMetrics(SM_CYSCREEN), 1));
    return width / height;
}

void SendInputPointer::MoveTo(Vec2 position) {
    INPUT move = MouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE);
    move.mi.dx = LONG(position.x * kAbsoluteRange);
    move.mi.dy = LONG(position.y * kAbsoluteRange);
    SendInput(1, &move, sizeof(INPUT));
}

void SendInputPointer::Scroll(int vertical, int horizontal) {
    INPUT wheels[2] = {};
    UINT count = 0;
    if (vertical != 0) {
        wheels[count] = MouseInput(MOUSEEVENTF_WHEEL);
        wheels[count++].mi.mouseData = DWORD(vertical);
    }
    if (horizontal != 0) {
        wheels[count] = MouseInput(MOUSEEVENTF_HWHEEL);
        wheels[count++].mi.mouseData = DWORD(horizontal);
    }
    SendInput(count, wheels, sizeof(INPUT));
}

void SendInputPointer::Click() {
    INPUT click[] = {MouseInput(MOUSEEVENTF_LEFTDOWN), MouseInput(MOUSEEVENTF_LEFTUP)};
    SendInput(ARRAYSIZE(click), click, sizeof(INPUT));
}

}  // namespace gazeora::input
