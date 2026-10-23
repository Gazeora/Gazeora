# Launches games, injects the hook into them, receives their frames and streams them to
# the local network over RTSP/RTP. A static library shared by Gazeora.exe, GazeoraCLI.exe
# (apps/) and the unit tests.
add_library(gazeora_host_lib STATIC
    # launch: starting games and injecting the hook into them
    src/launch/injector.cpp
    src/launch/launcher.cpp
    # capture: receiving the hook's frames and reading them back from the GPU
    src/capture/capture_client.cpp
    src/capture/frame_dump.cpp
    # encode: H.264 / H.265 through Media Foundation, from memory or GPU textures
    src/encode/color_convert.cpp
    src/encode/dxgi_device_manager.cpp
    src/encode/gpu_nv12_converter.cpp
    src/encode/media_foundation_encoder.cpp
    src/encode/nv12_readback.cpp
    src/encode/nv12_texture_pool.cpp
    # stream: the stream server and its protocols (RTSP/RTP, UDP, HTTP, USB), discovery
    src/stream/bitrate_controller.cpp
    src/stream/discovery.cpp
    src/stream/frame_mailbox.cpp
    src/stream/frame_output.cpp
    src/stream/frame_packet.cpp
    src/stream/frame_stream.cpp
    src/stream/http_ts_output.cpp
    src/stream/net.cpp
    src/stream/packet_pacer.cpp
    src/stream/pointer_gate.cpp
    src/stream/pointer_input_receiver.cpp
    src/stream/pointer_message.cpp
    src/stream/rtcp.cpp
    src/stream/rtp_video.cpp
    src/stream/rtsp_connection.cpp
    src/stream/rtsp_message.cpp
    src/stream/stream_log.cpp
    src/stream/stream_overlay.cpp
    src/stream/stream_protocol.cpp
    src/stream/stream_view_info.cpp
    src/stream/ts_muxer.cpp
    src/stream/udp_frame_output.cpp
    src/stream/usb_accessory_output.cpp
    src/stream/video_pipeline.cpp)
target_include_directories(gazeora_host_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_host_lib PUBLIC gazeora_common d3d11 d3dcompiler dxgi
    ws2_32 iphlpapi mfplat mfuuid winmm ole32 uuid gdi32
    PRIVATE libusb setupapi winusb)

# OpenMP splits the per-frame pixel loops (NV12 conversion, read-back) over a few cores.
find_package(OpenMP REQUIRED)
target_link_libraries(gazeora_host_lib PRIVATE OpenMP::OpenMP_CXX)
if(NOT MSVC)
    # GCC links its OpenMP runtime through this flag, which a static library doesn't pass on
    # to the executables linking it. (MSVC's objects name their runtime themselves.)
    target_link_options(gazeora_host_lib INTERFACE -fopenmp)
endif()
