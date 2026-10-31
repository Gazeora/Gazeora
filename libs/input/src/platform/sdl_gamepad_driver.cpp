// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/input/platform/sdl_gamepad_driver.h"

#include <SDL3/SDL.h>

#include <string>

#include "gazeora/common/text/utf8.h"
#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

struct Hint {
    const char* name;
    const char* value;
};

// Gazeora only watches: controllers keep the reports and lights games expect, and are read
// with Gazeora in the background too, where the stream shows it.
constexpr Hint kHints[] = {
    {SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"},
    {SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "0"},
    {SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "0"},
    {SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "0"},
    {SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_PLAYER_LED, "0"},
    {SDL_HINT_NO_SIGNAL_HANDLERS, "1"},
};

struct ButtonMapping {
    GamepadButton button;
    SDL_GamepadButton sdl;
};

constexpr ButtonMapping kButtons[] = {
    {GamepadButton::South, SDL_GAMEPAD_BUTTON_SOUTH},
    {GamepadButton::East, SDL_GAMEPAD_BUTTON_EAST},
    {GamepadButton::West, SDL_GAMEPAD_BUTTON_WEST},
    {GamepadButton::North, SDL_GAMEPAD_BUTTON_NORTH},
    {GamepadButton::Back, SDL_GAMEPAD_BUTTON_BACK},
    {GamepadButton::Start, SDL_GAMEPAD_BUTTON_START},
    {GamepadButton::LeftStick, SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {GamepadButton::RightStick, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {GamepadButton::LeftShoulder, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {GamepadButton::RightShoulder, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {GamepadButton::DpadUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {GamepadButton::DpadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {GamepadButton::DpadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {GamepadButton::DpadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
};

struct AxisMapping {
    GamepadAxis axis;
    SDL_GamepadAxis sdl;
};

constexpr AxisMapping kAxes[] = {
    {GamepadAxis::LeftX, SDL_GAMEPAD_AXIS_LEFTX},
    {GamepadAxis::LeftY, SDL_GAMEPAD_AXIS_LEFTY},
    {GamepadAxis::RightX, SDL_GAMEPAD_AXIS_RIGHTX},
    {GamepadAxis::RightY, SDL_GAMEPAD_AXIS_RIGHTY},
};

constexpr float kAxisRange = float(SDL_JOYSTICK_AXIS_MAX);
constexpr wchar_t kUnnamed[] = L"Game controller";

}  // namespace

SdlGamepadDriver::SdlGamepadDriver() {
    for (const Hint& hint : kHints) {
        SDL_SetHint(hint.name, hint.value);
    }
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        throw InputError(L"Game controllers can't be read: " + Utf8::ToWide(SDL_GetError()));
    }
}

SdlGamepadDriver::~SdlGamepadDriver() {
    for (const auto& entry : m_gamepads) {
        SDL_CloseGamepad(entry.second);
    }
    SDL_Quit();
}

void SdlGamepadDriver::Update() {
    // Polling pumps SDL's events, which reads the controllers; it also reports the ones
    // connected before it started.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            Open(event.gdevice.which);
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            Close(event.gdevice.which);
        }
    }
}

std::vector<GamepadInfo> SdlGamepadDriver::Connected() const {
    std::vector<GamepadInfo> connected;
    for (const auto& entry : m_gamepads) {
        const char* name = SDL_GetGamepadName(entry.second);
        connected.push_back({name ? Utf8::ToWide(name) : kUnnamed});
    }
    return connected;
}

GamepadState SdlGamepadDriver::State() const {
    GamepadState merged;
    for (const auto& entry : m_gamepads) {
        merged.Merge(Read(entry.second));
    }
    return merged;
}

void SdlGamepadDriver::Open(GamepadId id) {
    if (m_gamepads.count(id) != 0) {
        return;
    }
    if (SDL_Gamepad* gamepad = SDL_OpenGamepad(id)) {
        m_gamepads[id] = gamepad;
    }
}

void SdlGamepadDriver::Close(GamepadId id) {
    auto found = m_gamepads.find(id);
    if (found == m_gamepads.end()) {
        return;
    }
    SDL_CloseGamepad(found->second);
    m_gamepads.erase(found);
}

GamepadState SdlGamepadDriver::Read(SDL_Gamepad* gamepad) {
    GamepadState state;
    for (const ButtonMapping& mapping : kButtons) {
        state.SetButton(mapping.button, SDL_GetGamepadButton(gamepad, mapping.sdl));
    }
    for (const AxisMapping& mapping : kAxes) {
        state.SetAxis(mapping.axis, SDL_GetGamepadAxis(gamepad, mapping.sdl) / kAxisRange);
    }
    return state;
}

}  // namespace gazeora::input
