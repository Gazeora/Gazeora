# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The bundled plugins' protocols and codec in-process, without their DLLs: for the tests, tools
# (gazeora_stream_probe) and benchmarks, which run without plugins. Gazeora.exe gets them from
# the plugins instead.
add_library(gazeora_bundled_parts STATIC
    src/bundled_protocols.cpp
    src/bundled_video_codecs.cpp)
target_include_directories(gazeora_bundled_parts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_bundled_parts
    PUBLIC gazeora_host_lib gazeora_udp gazeora_http gazeora_usb gazeora_h265)
