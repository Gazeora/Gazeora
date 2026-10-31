# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# gazeora_bench.exe: how long each stage takes, from the hook's capture to RTP packets. Kept
# apart from the unit tests and not registered with ctest: it takes about a minute, uses the
# real GPU and encoders, and only prints. Run it from a Release build; --gtest_filter picks
# areas, e.g. --gtest_filter=Encode.*:Stream.*
include(FetchGoogleTest)

add_executable(gazeora_bench
    src/bench_main.cpp
    src/capture_bench.cpp
    src/convert_bench.cpp
    src/encode_bench.cpp
    src/library_bench.cpp
    src/protocol_bench.cpp
    src/stream_bench.cpp)
target_link_libraries(gazeora_bench PRIVATE gazeora_hook_core gazeora_host_lib gazeora_library
    GTest::gtest)
# Capture.RealHookPerGraphicsApi runs the hook in the test app, both from the same bin folder.
add_dependencies(gazeora_bench gazeora_hook gazeora_testapp)
