# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Launches games, injects the hook into them, receives their frames and streams them to
# the local network over RTSP/RTP. A static library shared by Gazeora.exe, GazeoraCLI.exe
# (apps/) and the unit tests.
add_library(gazeora_host_lib STATIC
    # launch: starting games and injecting the hook into them
    src/launch/injector.cpp
    src/launch/launcher.cpp
    src/launch/remote_thread_injector.cpp
    # capture: receiving the hook's frames and reading them back from the GPU
    src/capture/capture_client.cpp
    src/capture/controller_turn_channel.cpp
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
    src/stream/head_angles.cpp
    src/stream/head_pose_gate.cpp
    src/stream/head_pose_message.cpp
    src/stream/head_pose_monitor.cpp
    src/stream/http_ts_output.cpp
    src/stream/net.cpp
    src/stream/pacing_plan.cpp
    src/stream/packet_send_queue.cpp
    src/stream/pointer_gate.cpp
    src/stream/pointer_message.cpp
    src/stream/remote_input_receiver.cpp
    src/stream/rtcp.cpp
    src/stream/rtp_video.cpp
    src/stream/rtsp_connection.cpp
    src/stream/rtsp_message.cpp
    src/stream/stream_log.cpp
    src/stream/stream_overlay.cpp
    src/stream/stream_frame_sender.cpp
    src/stream/stream_protocol.cpp
    src/stream/stream_view_info.cpp
    src/stream/ts_muxer.cpp
    src/stream/udp_frame_output.cpp
    src/stream/usb_accessory_output.cpp
    src/stream/video_pipeline.cpp)
target_include_directories(gazeora_host_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_host_lib PUBLIC gazeora_common d3d11 d3dcompiler dxgi
    ws2_32 iphlpapi mfplat mfuuid winmm ole32 uuid gdi32
    PRIVATE libusb setupapi winusb bcrypt)
if(MSVC)
    # The Windows SDK declares the Media Foundation codecs' CLSIDs (wmcodecdsp.h) and defines
    # them here; MinGW has no such library, so with it they're instantiated in the sources
    # (initguid.h).
    target_link_libraries(gazeora_host_lib PUBLIC wmcodecdspuuid)
endif()

# OpenMP splits the per-frame pixel loops (NV12 conversion, read-back) over a few cores.
if(MSVC)
    find_package(OpenMP REQUIRED COMPONENTS CXX)  # the OpenMP code is all C++
    target_link_libraries(gazeora_host_lib PRIVATE OpenMP::OpenMP_CXX)
    # MSVC's OpenMP runtime is a DLL only, which Visual Studio has for shipping beside the
    # programs: copied into bin\ and installed with them, so they run without the redistributable.
    file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" GAZEORA_VC_REDIST_DIR)
    file(GLOB GAZEORA_VCOMP_CANDIDATES
        "${GAZEORA_VC_REDIST_DIR}/${GAZEORA_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll")
    if(NOT GAZEORA_VCOMP_CANDIDATES)
        # cl.exe is in VC\Tools\MSVC\<version>\bin\Host<arch>\<arch>\, the runtimes in VC\Redist.
        get_filename_component(GAZEORA_VC_DIR "${CMAKE_CXX_COMPILER}/../../../../../.." ABSOLUTE)
        file(GLOB GAZEORA_VCOMP_CANDIDATES
            "${GAZEORA_VC_DIR}/Redist/MSVC/*/${GAZEORA_ARCH}/Microsoft.VC*.OpenMP/vcomp140.dll")
        list(SORT GAZEORA_VCOMP_CANDIDATES ORDER DESCENDING)  # the newest first
    endif()
    if(GAZEORA_VCOMP_CANDIDATES)
        list(GET GAZEORA_VCOMP_CANDIDATES 0 GAZEORA_VCOMP_DLL)
        message(STATUS "OpenMP runtime: ${GAZEORA_VCOMP_DLL}")
        add_custom_target(gazeora_openmp_runtime
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GAZEORA_VCOMP_DLL}
                ${GAZEORA_RUNTIME_DIR}
            VERBATIM)
        add_dependencies(gazeora_host_lib gazeora_openmp_runtime)
        include(GNUInstallDirs)
        install(FILES ${GAZEORA_VCOMP_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
    else()
        message(WARNING "vcomp140.dll not found: the programs will need the Visual C++ "
                        "Redistributable installed to run")
    endif()
else()
    # Just the flag, to compile and to link: GCC's driver then links its OpenMP runtime itself,
    # statically like the rest (-static). FindOpenMP hands out the runtime DLL's import library
    # on some MinGW builds (MSYS2's), which would make every program need libgomp-1.dll. The
    # link flag goes on to the executables, as a static library doesn't link anything itself.
    target_compile_options(gazeora_host_lib PRIVATE -fopenmp)
    target_link_options(gazeora_host_lib INTERFACE -fopenmp)
endif()
