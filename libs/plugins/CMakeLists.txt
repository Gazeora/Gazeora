# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Loads plugin DLLs (built against gazeora_plugin_api) and adds what they offer to the
# host's registries and catalogs: game stores, video encoders, codecs and protocols.
add_library(gazeora_plugins STATIC
    src/adapters/plugin_encoder_provider.cpp
    src/adapters/plugin_frame_output.cpp
    src/adapters/plugin_game_store.cpp
    src/adapters/plugin_input_router.cpp
    src/adapters/plugin_output_factory.cpp
    src/adapters/plugin_video_encoder.cpp
    src/platform/plugin_folder.cpp
    src/platform/win32_plugin_module.cpp
    src/platform/win32_plugin_module_loader.cpp
    src/services/plugin_codec_check.cpp
    src/services/plugin_loader.cpp
    src/services/plugin_registrar.cpp
    src/plugin_services.cpp)
target_include_directories(gazeora_plugins
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_plugins
    PUBLIC gazeora_host_lib
    PRIVATE gazeora_plugin_api gazeora_plugin_kit)
