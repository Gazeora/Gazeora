# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# SDL 3.4.18: reads game controllers of every kind (XInput, DirectInput, Windows.Gaming.Input
# and HID ones such as the DualSense) as one standard layout. Only its joystick and gamepad
# parts are used; it's linked statically, so nothing ships beside the exe. Video stays in the
# build because SDL's DirectInput support depends on it, but it's never started.

include(FetchContent)
FetchContent_Declare(sdl
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
foreach(subsystem AUDIO GPU RENDER CAMERA HAPTIC POWER SENSOR DIALOG TRAY)
    set(SDL_${subsystem} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_MakeAvailable(sdl)
