# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Example plugins, built against the plugin interface alone, as plugins outside this repository
# are. They go in a folder of their own: Gazeora.exe loads plugins only from plugins\.
set(GAZEORA_EXAMPLE_PLUGIN_DIR ${CMAKE_BINARY_DIR}/example_plugins)
set(GAZEORA_EXAMPLE_PLUGIN_DIR ${GAZEORA_EXAMPLE_PLUGIN_DIR} PARENT_SCOPE)

add_subdirectory(plugins/frame_recorder)
