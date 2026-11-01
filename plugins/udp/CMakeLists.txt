# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# UDP Direct: Gazeora's own UDP protocol for the Gazeora app (UdpFrameOutput).
add_library(gazeora_udp STATIC src/udp_frame_output.cpp)
target_include_directories(gazeora_udp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_udp PUBLIC gazeora_host_lib PRIVATE bcrypt)

gazeora_add_bundled_plugin(gazeora_udp_plugin
    ID gazeora-udp CATEGORY protocol NAME "UDP Direct"
    SUMMARY "Streams to the Gazeora app over UDP: the least delay and overhead on Wi-Fi."
    SOURCES src/udp_plugin.cpp
    LIBS gazeora_udp)
