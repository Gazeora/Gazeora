# GoogleTest unit tests. Every test process uses its own PID as the "game", and the hook
# and host each attach to that PID's shared block (common/shared.h), so each side gets
# its own executable:
#  - gazeora_hook_tests:      hook core with a fake host block
#  - gazeora_host_tests:      host library and the shared protocol header
#  - gazeora_transport_tests: real hook and host talking to each other in one process
#  - gazeora_library_tests:   the game library behind Gazeora.exe and GazeoraCLI.exe
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
        host/encode/video_encoder_test.cpp
        host/stream/bitrate_controller_test.cpp
        host/stream/frame_stream_test.cpp
        host/stream/rtcp_test.cpp
        host/stream/rtp_video_test.cpp
        host/stream/rtsp_message_test.cpp
        host/stream/stream_formats_test.cpp
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

