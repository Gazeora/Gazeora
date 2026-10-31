# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Command-line RTSP player that measures Gazeora's frame stream (fps, bitrate, packet
# loss, latency) without VLC.
add_executable(gazeora_stream_probe src/main.cpp)
target_link_libraries(gazeora_stream_probe PRIVATE gazeora_host_lib ws2_32 ole32)
