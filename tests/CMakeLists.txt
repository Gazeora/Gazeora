# GoogleTest unit tests. Every test process uses its own PID as the "game", and the hook
# and host each attach to that PID's shared block (common/shared.h), so each side gets
# its own executable:
#  - gazeora_hook_tests:      hook core with a fake host block
#  - gazeora_host_tests:      host library and the shared protocol header
#  - gazeora_transport_tests: real hook and host talking to each other in one process
#  - gazeora_library_tests:   the game library behind Gazeora.exe and GazeoraCLI.exe
#  - gazeora_input_tests:     hand gesture control, with the real models on photos in input/data
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
        host/capture/capture_client_test.cpp
        host/capture/frame_dump_test.cpp
        host/encode/gpu_nv12_converter_test.cpp
        host/encode/video_encoder_test.cpp
        host/stream/bitrate_controller_test.cpp
        host/stream/frame_mailbox_test.cpp
        host/stream/frame_stream_test.cpp
        host/stream/pacing_plan_test.cpp
        host/stream/packet_send_queue_test.cpp
        host/stream/pointer_input_test.cpp
        host/stream/rtcp_test.cpp
        host/stream/rtp_video_test.cpp
        host/stream/rtsp_message_test.cpp
        host/stream/stream_formats_test.cpp
        host/stream/stream_frame_sender_test.cpp
        host/stream/stream_overlay_test.cpp
        host/stream/stream_view_info_test.cpp
        host/stream/video_pipeline_test.cpp
    LIBS gazeora_host_lib)

gazeora_add_test(gazeora_library_tests
    SOURCES library/game_library_test.cpp
    LIBS gazeora_library)

gazeora_add_test(gazeora_transport_tests
    SOURCES transport/frame_transport_test.cpp
    LIBS gazeora_hook_core gazeora_host_lib)


gazeora_add_test(gazeora_input_tests
    SOURCES
        input/support/image_file.cpp
        input/support/synthetic_video.cpp
        input/tracking/image_sampler_test.cpp
        input/tracking/mediapipe_hand_tracker_test.cpp
        input/tracking/palm_anchors_test.cpp
        input/tracking/palm_detector_test.cpp
        input/tracking/roi_calculator_test.cpp
        input/tracking/roi_stabilizer_test.cpp
        input/gestures/hand_gesture_detector_test.cpp
        input/gestures/left_hand_test.cpp
        input/gestures/two_hand_scroll_test.cpp
        input/gestures/landmark_smoother_test.cpp
        input/gestures/air_touch_pointer_test.cpp
        input/services/hand_gesture_controller_test.cpp
        input/services/hand_gesture_session_test.cpp
        input/services/synthetic_video_test.cpp
    LIBS gazeora_input windowscodecs ole32)
target_include_directories(gazeora_input_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/input)
target_compile_definitions(gazeora_input_tests PRIVATE
    GAZEORA_INPUT_TEST_DATA=L"${CMAKE_CURRENT_SOURCE_DIR}/input/data")
