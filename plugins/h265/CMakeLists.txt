# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# H.265 (HEVC): how the stream carries H.265 (H265Format, H265PayloadFormat). Gazeora's own
# Media Foundation encoder encodes it.
add_library(gazeora_h265 STATIC src/h265_format.cpp src/h265_payload_format.cpp)
target_include_directories(gazeora_h265 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_h265 PUBLIC gazeora_host_lib)

gazeora_add_bundled_plugin(gazeora_h265_plugin
    ID gazeora-h265 CATEGORY codec NAME "H.265 (HEVC)"
    SUMMARY "H.265 video: a sharper picture on weak Wi-Fi, for newer phones."
    SOURCES src/h265_plugin.cpp
    LIBS gazeora_h265)
