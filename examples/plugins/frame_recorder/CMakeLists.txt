# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Frame recorder: an example plugin that records the stream to a file, with a card on the
# Settings page (README.md). It uses nothing of Gazeora's but the plugin interface,
# gazeora_plugin.h, so it builds with Gazeora (examples/CMakeLists.txt) or on its own:
#
#   cmake -S examples/plugins/frame_recorder -B build-recorder -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-recorder
cmake_minimum_required(VERSION 3.20)
project(GazeoraFrameRecorder LANGUAGES CXX)

# The C runtime goes inside the DLL, so it needs no Visual C++ redistributable on the PC.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# On its own, the header comes from this repository, or from GAZEORA_PLUGIN_API_INCLUDE_DIR.
if(NOT TARGET gazeora_plugin_api)
    set(GAZEORA_PLUGIN_API_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/plugin_api/include
        CACHE PATH "The folder that holds gazeora/plugin_api/gazeora_plugin.h")
    add_library(gazeora_plugin_api INTERFACE)
    target_include_directories(gazeora_plugin_api INTERFACE ${GAZEORA_PLUGIN_API_INCLUDE_DIR})
endif()

add_library(gazeora_frame_recorder SHARED
    src/annexb_file.cpp
    src/file_start.cpp
    src/frame_recorder.cpp
    src/frame_recorder_plugin.cpp
    src/recorder_output.cpp
    src/recorder_panel.cpp
    src/recorder_protocol.cpp
    src/recorder_settings.cpp
    src/recording.cpp
    src/recording_name.cpp
    src/settings_file.cpp
    src/utf8_path.cpp
    src/videos_folder.cpp)
target_compile_features(gazeora_frame_recorder PRIVATE cxx_std_17)
target_compile_definitions(gazeora_frame_recorder PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN
                                                          NOMINMAX)
target_link_libraries(gazeora_frame_recorder PRIVATE gazeora_plugin_api shell32 ole32)
# frame-recorder.dll: Install from file on the Plugins page names the plugin after the file.
set_target_properties(gazeora_frame_recorder PROPERTIES PREFIX "" OUTPUT_NAME frame-recorder)
if(DEFINED GAZEORA_EXAMPLE_PLUGIN_DIR)
    set_target_properties(gazeora_frame_recorder PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${GAZEORA_EXAMPLE_PLUGIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${GAZEORA_EXAMPLE_PLUGIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${GAZEORA_EXAMPLE_PLUGIN_DIR})
endif()

# The card's layout as a string in the DLL: kRecorderPanelXml in recorder_panel_xml.h. Editing
# the XML reconfigures and rebuilds.
set(layout ${CMAKE_CURRENT_SOURCE_DIR}/ui/recorder_panel.xml)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${layout})
file(READ ${layout} layout_text)
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/recorder_panel_xml.h
    CONTENT "#pragma once\n\nconstexpr char kRecorderPanelXml[] = R\"xml(${layout_text})xml\";\n"
    @ONLY)
target_include_directories(gazeora_frame_recorder PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
