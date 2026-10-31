// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "remote/imgui_pointer_input.h"

#include <imgui_internal.h>

namespace gazeora::app {

namespace {

// Windows' wheel units in one notch (WHEEL_DELTA); ImGui counts in notches.
constexpr float kWheelNotch = 120.0f;
// How long after the last touch the pointer stays where it was, not where the mouse is.
constexpr uint64_t kHoldPositionMs = 1000;

ImGuiMouseButton ToImGui(PointerButton button) {
    return button == PointerButton::Right ? ImGuiMouseButton_Right : ImGuiMouseButton_Left;
}

}  // namespace

void ImGuiPointerInput::Apply(const std::vector<PointerEvent>& events, ImGuiIO& io,
                              uint64_t nowMs) {
    if (events.empty()) {
        if (HoldsPosition(nowMs)) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(m_position.x, m_position.y);
        }
        return;
    }
    for (const PointerEvent& event : events) {
        if (event.action != PointerAction::Scroll) {
            FlushWheel(io);
        }
        ApplyOne(event, io);
    }
    FlushWheel(io);
    m_lastTouchMs = nowMs;
}

bool ImGuiPointerInput::HasQueuedInput() {
    // ImGui takes queued input a step per frame: frames have to go on until it's all in.
    return ImGui::GetCurrentContext()->InputEventsQueue.Size > 0;
}

void ImGuiPointerInput::ApplyOne(const PointerEvent& event, ImGuiIO& io) {
    switch (event.action) {
        case PointerAction::Move:
            MoveTo(event, io);
            break;
        case PointerAction::Press:
            MoveTo(event, io);
            io.AddMouseButtonEvent(ToImGui(event.button), true);
            m_pressed = true;
            break;
        case PointerAction::Release:
            io.AddMouseButtonEvent(ToImGui(event.button), false);
            m_pressed = false;
            break;
        case PointerAction::Click:
            MoveTo(event, io);
            io.AddMouseButtonEvent(ToImGui(event.button), true);
            io.AddMouseButtonEvent(ToImGui(event.button), false);
            break;
        case PointerAction::Scroll:
            // One wheel event for a run of them, so they don't queue up a frame each.
            m_wheelVertical += event.wheelVertical;
            m_wheelHorizontal += event.wheelHorizontal;
            break;
    }
}

void ImGuiPointerInput::MoveTo(const PointerEvent& event, ImGuiIO& io) {
    m_position = {event.x * io.DisplaySize.x, event.y * io.DisplaySize.y};
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMousePosEvent(m_position.x, m_position.y);
}

void ImGuiPointerInput::FlushWheel(ImGuiIO& io) {
    if (m_wheelVertical == 0 && m_wheelHorizontal == 0) {
        return;
    }
    // ImGui's horizontal wheel runs the other way: positive scrolls left.
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    io.AddMouseWheelEvent(-m_wheelHorizontal / kWheelNotch, m_wheelVertical / kWheelNotch);
    m_wheelVertical = 0;
    m_wheelHorizontal = 0;
}

bool ImGuiPointerInput::HoldsPosition(uint64_t nowMs) const {
    return m_lastTouchMs != 0 && (m_pressed || nowMs - m_lastTouchMs < kHoldPositionMs);
}

}  // namespace gazeora::app
