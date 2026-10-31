# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Ways to control Gazeora besides the PC's own mouse and keyboard: game controllers.
include(Sdl)

add_library(gazeora_input STATIC
    # gamepad: game controllers, whatever the make
    src/gamepad/gamepad_state_filter.cpp
    src/models/gamepad_state.cpp
    # services: the controller polling
    src/services/gamepad_session.cpp
    # platform: SDL
    src/platform/sdl_gamepad_driver.cpp)
target_include_directories(gazeora_input
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_input
    PUBLIC gazeora_common
    PRIVATE SDL3::SDL3-static)
