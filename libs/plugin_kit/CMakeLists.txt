# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Between the C plugin interface (gazeora_plugin_api) and Gazeora's C++ codecs, outputs, game
# controller drivers and UI panels, both ways: a plugin's codec as a VideoCodecFormat
# (adapters/), and Gazeora's own as plugin structs (exports/), for the bundled plugins
# (plugins/) and the frames Gazeora hands a plugin output. bundled/ is what the bundled plugins
# share: their info and registering through the host.
add_library(gazeora_plugin_kit STATIC
    src/adapters/plugin_codec_format.cpp
    src/adapters/plugin_gamepad_state.cpp
    src/adapters/plugin_rtp_payload_format.cpp
    src/bundled/bundled_plugin.cpp
    src/exports/exported_codec.cpp
    src/exports/exported_codec_cache.cpp
    src/exports/exported_gamepad_driver.cpp
    src/exports/exported_gamepad_driver_provider.cpp
    src/exports/exported_output.cpp
    src/exports/exported_output_calls.cpp
    src/exports/exported_protocol.cpp
    src/exports/exported_ui_panel.cpp
    src/exports/input_forwarder.cpp)
target_include_directories(gazeora_plugin_kit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# What every bundled plugin reports as its version (BundledPlugin::Info): Gazeora's.
target_compile_definitions(gazeora_plugin_kit PRIVATE
    GAZEORA_BUNDLED_PLUGIN_VERSION="${GAZEORA_VERSION_TEXT}")
target_link_libraries(gazeora_plugin_kit
    PUBLIC gazeora_host_lib gazeora_input gazeora_plugin_ui gazeora_plugin_api)
