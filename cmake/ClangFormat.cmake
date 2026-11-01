# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# `format` rewrites all project sources with clang-format; `format-check` fails if any
# file isn't formatted. Both use the .clang-format file at the repository root.

find_program(CLANG_FORMAT_EXE NAMES clang-format
    HINTS "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin")

if(NOT CLANG_FORMAT_EXE)
    message(STATUS "clang-format not found; format targets disabled")
    return()
endif()

file(GLOB_RECURSE GAZEORA_FORMAT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/libs/*.h ${CMAKE_SOURCE_DIR}/libs/*.cpp
    ${CMAKE_SOURCE_DIR}/apps/*.h ${CMAKE_SOURCE_DIR}/apps/*.cpp
    ${CMAKE_SOURCE_DIR}/plugins/*.h ${CMAKE_SOURCE_DIR}/plugins/*.cpp
    ${CMAKE_SOURCE_DIR}/examples/*.h ${CMAKE_SOURCE_DIR}/examples/*.cpp
    ${CMAKE_SOURCE_DIR}/tools/*.h ${CMAKE_SOURCE_DIR}/tools/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.h ${CMAKE_SOURCE_DIR}/tests/*.cpp)

# The files go in a list clang-format reads (--files, clang-format 16 or newer): on the command
# line they're longer than Windows allows one to be. Rewritten when the glob above changes.
list(JOIN GAZEORA_FORMAT_SOURCES "\n" GAZEORA_FORMAT_LIST)
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/format-sources.txt CONTENT "${GAZEORA_FORMAT_LIST}\n"
    @ONLY)

add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i --files=${CMAKE_BINARY_DIR}/format-sources.txt
    COMMENT "Formatting sources with clang-format"
    VERBATIM)

add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror --files=${CMAKE_BINARY_DIR}/format-sources.txt
    COMMENT "Checking source formatting"
    VERBATIM)
