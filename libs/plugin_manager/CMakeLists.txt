# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The Plugins page's work: the plugin list on gazeora.github.io, downloads, and changing the
# plugins folder (as an administrator).
include(NlohmannJson)

add_library(gazeora_plugin_manager STATIC
    src/mappers/plugin_listing_json_mapper.cpp
    src/models/plugin_category.cpp
    src/models/plugin_entry.cpp
    src/models/plugin_id.cpp
    src/platform/plugin_download_location.cpp
    src/repositories/bundled_plugin_index.cpp
    src/repositories/web_plugin_index.cpp
    src/services/plugin_directory.cpp
    src/services/plugin_folder_editor.cpp)
target_include_directories(gazeora_plugin_manager
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_plugin_manager
    PUBLIC gazeora_plugins gazeora_update
    PRIVATE gazeora_plugin_api nlohmann_json::nlohmann_json)
