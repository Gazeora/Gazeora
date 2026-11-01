# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# HTTP Live: a live MPEG-TS stream over HTTP (HttpTsOutput, TsMuxer).
add_library(gazeora_http STATIC src/http_ts_output.cpp src/ts_muxer.cpp)
target_include_directories(gazeora_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_http PUBLIC gazeora_host_lib)

gazeora_add_bundled_plugin(gazeora_http_plugin
    ID gazeora-http CATEGORY protocol NAME "HTTP Live"
    SUMMARY "A live MPEG-TS stream over HTTP for the Gazeora app, VLC and ffplay."
    SOURCES src/http_plugin.cpp
    LIBS gazeora_http)
