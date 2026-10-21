# Launches games, injects the hook into them, receives their frames and streams them to
# the local network over RTSP/RTP. A static library shared by Gazeora.exe, GazeoraCLI.exe
# (src/) and the unit tests.
add_library(gazeora_host_lib STATIC
    # launch: starting games and injecting the hook into them
    src/launch/injector.cpp
    src/launch/launcher.cpp
    # capture: receiving the hook's frames and reading them back from the GPU
    src/capture/capture_client.cpp
    src/capture/frame_dump.cpp
    # encode: H.264 / H.265 through Media Foundation
    src/encode/color_convert.cpp
    src/encode/media_foundation_encoder.cpp
    # stream: the RTSP server and RTP packetization
    src/stream/frame_stream.cpp
    src/stream/rtp_video.cpp)
target_include_directories(gazeora_host_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_host_lib PUBLIC gazeora_common d3d11 d3dcompiler dxgi
    ws2_32 iphlpapi mfplat mfuuid winmm ole32 uuid)
