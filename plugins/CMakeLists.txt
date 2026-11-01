# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The bundled plugins, one folder each, with only that plugin's code: Gazeora's UDP, HTTP and
# USB protocols, its H.265 codec and its game controller support. They come with Gazeora in
# bundled-plugins\, with bundled.json saying what they are, and load only once installed from
# the Plugins page into plugins\. What they share is gazeora_plugin_kit (libs/plugin_kit).
include(BundledPlugins)

add_subdirectory(udp)         # gazeora_udp + gazeora-udp.dll
add_subdirectory(http)        # gazeora_http + gazeora-http.dll
add_subdirectory(usb)         # gazeora_usb + gazeora-usb.dll
add_subdirectory(h265)        # gazeora_h265 + gazeora-h265.dll
add_subdirectory(controller)  # gazeora_controller + gazeora-controller.dll

gazeora_write_bundled_manifest()

# Every bundled plugin's DLL: Gazeora.exe depends on them, and cmake --install puts them, with
# bundled.json, in bundled-plugins\ (Install.cmake).
get_property(GAZEORA_BUNDLED_PLUGINS GLOBAL PROPERTY GAZEORA_BUNDLED_PLUGINS)
set(GAZEORA_BUNDLED_PLUGINS ${GAZEORA_BUNDLED_PLUGINS} PARENT_SCOPE)
set(GAZEORA_BUNDLED_PLUGIN_DIR ${GAZEORA_BUNDLED_PLUGIN_DIR} PARENT_SCOPE)
