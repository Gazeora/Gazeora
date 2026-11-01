# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Building the bundled plugins (plugins/<name>/). Each plugin's CMakeLists.txt makes a static
# library of its own code (for the tests, tools and benchmarks too) and calls
# gazeora_add_bundled_plugin for its DLL. plugins/CMakeLists.txt then writes bundled.json.

# Beside bin\ in the build, as in an install (PluginFolder::Bundled).
set(GAZEORA_BUNDLED_PLUGIN_DIR ${GAZEORA_RUNTIME_DIR}/../bundled-plugins)

# The plugin API version they're built with, for bundled.json: GAZEORA_PLUGIN_API_VERSION.
file(STRINGS ${PROJECT_SOURCE_DIR}/libs/plugin_api/include/gazeora/plugin_api/gazeora_plugin.h
    api_version_line REGEX "^#define GAZEORA_PLUGIN_API_VERSION [0-9]+u$")
string(REGEX REPLACE ".* ([0-9]+)u$" "\\1" GAZEORA_PLUGIN_API_VERSION "${api_version_line}")

# gazeora_add_bundled_plugin(<target> ID <id> CATEGORY <category> NAME <name> SUMMARY <summary>
#                            SOURCES <sources>... [LIBS <libraries>...])
# The DLL is <id>.dll in bundled-plugins\, with GazeoraPluginInfo reporting <name>
# (libs/plugin_kit/src/bundled/bundled_plugin_info.cpp); its entry in bundled.json has the
# category, name and summary for the Plugins page. Names don't repeat the category, which the
# page shows beside them.
function(gazeora_add_bundled_plugin target)
    cmake_parse_arguments(ARG "" "ID;CATEGORY;NAME;SUMMARY" "SOURCES;LIBS" ${ARGN})
    add_library(${target} SHARED ${ARG_SOURCES}
        ${PROJECT_SOURCE_DIR}/libs/plugin_kit/src/bundled/bundled_plugin_info.cpp)
    target_link_libraries(${target} PRIVATE gazeora_plugin_kit ${ARG_LIBS})
    target_compile_definitions(${target} PRIVATE GAZEORA_BUNDLED_PLUGIN_NAME="${ARG_NAME}")
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${ARG_ID}
        RUNTIME_OUTPUT_DIRECTORY ${GAZEORA_BUNDLED_PLUGIN_DIR})
    gazeora_sign(${target})
    set(entry "{\"id\": \"${ARG_ID}\", \"category\": \"${ARG_CATEGORY}\", ")
    string(APPEND entry "\"name\": \"${ARG_NAME}\", \"summary\": \"${ARG_SUMMARY}\", ")
    string(APPEND entry "\"version\": \"${GAZEORA_VERSION_TEXT}\", \"author\": \"Gazeora\", ")
    string(APPEND entry "\"license\": \"GPL-3.0-only\", ")
    string(APPEND entry "\"api_version\": ${GAZEORA_PLUGIN_API_VERSION}}")
    set_property(GLOBAL APPEND PROPERTY GAZEORA_BUNDLED_PLUGINS ${target})
    set_property(GLOBAL APPEND PROPERTY GAZEORA_BUNDLED_MANIFEST_ENTRIES "${entry}")
endfunction()

# gazeora_embed_text(<target> <file> <header> <name>): <file>'s text as `constexpr char <name>[]`
# in a generated <header> that <target> can include. Editing the file reconfigures and rebuilds.
function(gazeora_embed_text target file header name)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${file})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    file(READ ${source} text)
    set(folder ${CMAKE_CURRENT_BINARY_DIR}/embedded/${target})
    file(CONFIGURE OUTPUT ${folder}/${header}
        CONTENT "#pragma once\n\nconstexpr char ${name}[] = R\"gazeora(${text})gazeora\";\n"
        @ONLY)
    target_include_directories(${target} PRIVATE ${folder})
endfunction()

# Writes bundled.json, what the Plugins page lists under Available, in the plugin list's format
# (docs/plugins.md), from every gazeora_add_bundled_plugin so far.
function(gazeora_write_bundled_manifest)
    get_property(entries GLOBAL PROPERTY GAZEORA_BUNDLED_MANIFEST_ENTRIES)
    list(JOIN entries ",\n    " joined)
    file(GENERATE OUTPUT ${GAZEORA_BUNDLED_PLUGIN_DIR}/bundled.json CONTENT
        "{\n  \"platform\": \"windows\",\n  \"api_version\": ${GAZEORA_PLUGIN_API_VERSION},\n  \"plugins\": [\n    ${joined}\n  ]\n}\n")
endfunction()
