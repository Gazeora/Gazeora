# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# GoogleTest unit tests. Every test process uses its own PID as the "game", and the hook
# and host each attach to that PID's shared block (common/shared.h), so each side gets
# its own executable:
#  - gazeora_hook_tests:      hook core with a fake host block
#  - gazeora_host_tests:      host library and the shared protocol header
#  - gazeora_transport_tests: real hook and host talking to each other in one process
#  - gazeora_library_tests:   the game library behind Gazeora.exe and GazeoraCLI.exe
#  - gazeora_input_tests:     game controllers
#  - gazeora_app_tests:       the launcher's game controller support and on-screen keyboard,
#                             on a headless ImGui
#  - gazeora_installer_tests: the MSI's custom actions, on a fake installer session
# D3D11 tests run on WARP, so no GPU is needed.

include(FetchGoogleTest)
include(GoogleTest)  # CMake's own: gtest_discover_tests

function(gazeora_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE ${ARG_LIBS} GTest::gtest_main)
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
endfunction()

gazeora_add_test(gazeora_hook_tests
    SOURCES
        hook/fake_host.cpp
        hook/core/capture_target_test.cpp
        hook/core/depth_picker_test.cpp
        hook/core/hook_util_test.cpp
        hook/core/pixel_convert_test.cpp
        hook/core/shared_channel_test.cpp
        hook/d3d11/d3d11_frame_ring_test.cpp
    LIBS gazeora_hook_core)

gazeora_add_test(gazeora_host_tests
    SOURCES
        common/shared_test.cpp
        host/launch/injector_test.cpp
        host/launch/launcher_test.cpp
        host/capture/capture_test_app_test.cpp
        host/capture/capture_client_test.cpp
        host/capture/frame_dump_test.cpp
        host/encode/gpu_nv12_converter_test.cpp
        host/encode/video_encoder_test.cpp
        host/stream/bitrate_controller_test.cpp
        host/stream/frame_mailbox_test.cpp
        host/stream/frame_stream_test.cpp
        host/stream/head_angles_test.cpp
        host/stream/head_pose_monitor_test.cpp
        host/stream/pacing_plan_test.cpp
        host/stream/packet_send_queue_test.cpp
        host/stream/remote_input_test.cpp
        host/stream/rtcp_test.cpp
        host/stream/rtp_video_test.cpp
        host/stream/rtsp_message_test.cpp
        host/stream/stream_formats_test.cpp
        host/stream/stream_frame_sender_test.cpp
        host/stream/stream_overlay_test.cpp
        host/stream/stream_view_info_test.cpp
        host/stream/video_pipeline_test.cpp
    LIBS gazeora_host_lib)
# The capture test launches the test app with the hook, both from this build.
add_dependencies(gazeora_host_tests gazeora_testapp gazeora_hook)
# Windows' D3D8 is 32-bit only, and the test app has it only where the compiler has d3d8.h.
if(TESTAPP_HAVE_D3D8_H AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_compile_definitions(gazeora_host_tests PRIVATE GAZEORA_TEST_D3D8)
endif()

gazeora_add_test(gazeora_library_tests
    SOURCES library/game_library_test.cpp
    LIBS gazeora_library)

gazeora_add_test(gazeora_installer_tests
    SOURCES installer/user_data_removal_test.cpp
    LIBS gazeora_setup_actions_core)

gazeora_add_test(gazeora_transport_tests
    SOURCES transport/frame_transport_test.cpp
    LIBS gazeora_hook_core gazeora_host_lib)


gazeora_add_test(gazeora_input_tests
    SOURCES
        input/gamepad/gamepad_state_test.cpp
        input/gamepad/gamepad_state_filter_test.cpp
        input/services/gamepad_session_test.cpp
        input/platform/sdl_gamepad_driver_test.cpp
    LIBS gazeora_input)
target_include_directories(gazeora_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/input)

# The launcher's own sources under test; the rest of Gazeora.exe stays out.
set(GAZEORA_APP_SOURCE_DIR ${PROJECT_SOURCE_DIR}/apps/gazeora/src)
gazeora_add_test(gazeora_app_tests
    SOURCES
        ${GAZEORA_APP_SOURCE_DIR}/gamepad/button_repeat.cpp
        ${GAZEORA_APP_SOURCE_DIR}/gamepad/gamepad_control.cpp
        ${GAZEORA_APP_SOURCE_DIR}/gamepad/gamepad_event_queue.cpp
        ${GAZEORA_APP_SOURCE_DIR}/gamepad/gamepad_presses.cpp
        ${GAZEORA_APP_SOURCE_DIR}/gamepad/imgui_gamepad_input.cpp
        ${GAZEORA_APP_SOURCE_DIR}/gamepad/pane_navigation.cpp
        ${GAZEORA_APP_SOURCE_DIR}/head/head_look.cpp
        ${GAZEORA_APP_SOURCE_DIR}/head/mouse_look_driver.cpp
        ${GAZEORA_APP_SOURCE_DIR}/keyboard/keyboard_layout.cpp
        ${GAZEORA_APP_SOURCE_DIR}/keyboard/on_screen_keyboard.cpp
        ${GAZEORA_APP_SOURCE_DIR}/platform/shell.cpp
        ${GAZEORA_APP_SOURCE_DIR}/settings/controls_settings.cpp
        ${GAZEORA_APP_SOURCE_DIR}/settings/settings_file.cpp
        app/gamepad/button_repeat_test.cpp
        app/gamepad/gamepad_control_test.cpp
        app/gamepad/gamepad_event_queue_test.cpp
        app/gamepad/gamepad_presses_test.cpp
        app/gamepad/imgui_gamepad_input_test.cpp
        app/gamepad/pane_navigation_test.cpp
        app/head/head_look_test.cpp
        app/head/mouse_look_driver_test.cpp
        app/keyboard/keyboard_layout_test.cpp
        app/keyboard/on_screen_keyboard_test.cpp
    LIBS gazeora_input gazeora_library gazeora_host_lib imgui shell32 ole32 uuid)
target_include_directories(gazeora_app_tests PRIVATE
    ${GAZEORA_APP_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/input)
