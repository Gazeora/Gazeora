# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The bundled plugins: Gazeora's UDP, HTTP and USB protocols and its H.265 codec, each a plugin
# DLL. They come with Gazeora in bundled-plugins\, with bundled.json saying what they are, and
# load only once installed from the Plugins page into plugins\. Each wraps gazeora_host_lib's
# classes in the plugin interface through gazeora_plugin_kit.
add_library(gazeora_bundled_plugin STATIC common/src/bundled_plugin.cpp)
target_include_directories(gazeora_bundled_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_link_libraries(gazeora_bundled_plugin PUBLIC gazeora_plugin_kit)
target_compile_definitions(gazeora_bundled_plugin PRIVATE
    GAZEORA_BUNDLED_PLUGIN_VERSION="${GAZEORA_VERSION_TEXT}")

# Beside bin\ in the build, as in an install (PluginFolder::Bundled).
set(GAZEORA_BUNDLED_PLUGIN_DIR ${GAZEORA_RUNTIME_DIR}/../bundled-plugins)
set(GAZEORA_BUNDLED_PLUGIN_DIR ${GAZEORA_BUNDLED_PLUGIN_DIR} PARENT_SCOPE)
set(GAZEORA_BUNDLED_PLUGINS "")
set(GAZEORA_BUNDLED_MANIFEST_ENTRIES "")

# gazeora_add_bundled_plugin(<target> <id> <category> <name> <summary> <sources>...): the DLL is
# <id>.dll, and its entry in bundled.json has the category, name and summary for the Plugins page.
# The DLL reports the same name (GAZEORA_BUNDLED_PLUGIN_NAME). Names don't repeat the category,
# which the page shows beside them.
function(gazeora_add_bundled_plugin target id category name summary)
    add_library(${target} SHARED ${ARGN})
    target_link_libraries(${target} PRIVATE gazeora_bundled_plugin)
    target_compile_definitions(${target} PRIVATE GAZEORA_BUNDLED_PLUGIN_NAME="${name}")
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${id}
        RUNTIME_OUTPUT_DIRECTORY ${GAZEORA_BUNDLED_PLUGIN_DIR})
    gazeora_sign(${target})
    set(entry "{\"id\": \"${id}\", \"category\": \"${category}\", \"name\": \"${name}\", ")
    string(APPEND entry "\"summary\": \"${summary}\", ")
    string(APPEND entry "\"version\": \"${GAZEORA_VERSION_TEXT}\", \"author\": \"Gazeora\", ")
    string(APPEND entry "\"license\": \"GPL-3.0-only\", \"api_version\": 2}")
    set(GAZEORA_BUNDLED_MANIFEST_ENTRIES ${GAZEORA_BUNDLED_MANIFEST_ENTRIES} "${entry}" PARENT_SCOPE)
    set(GAZEORA_BUNDLED_PLUGINS ${GAZEORA_BUNDLED_PLUGINS} ${target} PARENT_SCOPE)
endfunction()

gazeora_add_bundled_plugin(gazeora_udp_plugin gazeora-udp protocol "UDP Direct"
    "Streams to the Gazeora app over UDP: the least delay and overhead on Wi-Fi."
    udp/udp_plugin.cpp)
gazeora_add_bundled_plugin(gazeora_http_plugin gazeora-http protocol "HTTP Live"
    "A live MPEG-TS stream over HTTP for the Gazeora app, VLC and ffplay."
    http/http_plugin.cpp)
gazeora_add_bundled_plugin(gazeora_usb_plugin gazeora-usb protocol "USB Cable"
    "Streams to the Gazeora app on a phone plugged into this PC: no Wi-Fi needed."
    usb/usb_plugin.cpp)
gazeora_add_bundled_plugin(gazeora_h265_plugin gazeora-h265 codec "H.265 (HEVC)"
    "H.265 video: a sharper picture on weak Wi-Fi, for newer phones."
    h265/h265_plugin.cpp)

# What the Plugins page lists under Available, in the plugin list's format (docs/plugins.md).
list(JOIN GAZEORA_BUNDLED_MANIFEST_ENTRIES ",\n    " manifest_entries)
file(GENERATE OUTPUT ${GAZEORA_BUNDLED_PLUGIN_DIR}/bundled.json CONTENT
    "{\n  \"platform\": \"windows\",\n  \"api_version\": 2,\n  \"plugins\": [\n    ${manifest_entries}\n  ]\n}\n")

# Every bundled plugin: Gazeora.exe depends on them, and cmake --install puts them, with
# bundled.json, in bundled-plugins\ (Install.cmake).
set(GAZEORA_BUNDLED_PLUGINS ${GAZEORA_BUNDLED_PLUGINS} PARENT_SCOPE)
