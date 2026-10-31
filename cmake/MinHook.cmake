# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# MinHook: built from source here instead of via its own (old) CMakeLists.

include(FetchContent)
FetchContent_Declare(minhook
    URL https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(minhook)

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c)
# Its instruction decoder for the build's bitness.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_sources(minhook PRIVATE ${minhook_SOURCE_DIR}/src/hde/hde64.c)
else()
    target_sources(minhook PRIVATE ${minhook_SOURCE_DIR}/src/hde/hde32.c)
endif()
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)
