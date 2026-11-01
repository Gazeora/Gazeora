# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# gazeora_plugin_api: the C header plugin DLLs are built against (MIT, see LICENSE).
add_library(gazeora_plugin_api INTERFACE)
target_include_directories(gazeora_plugin_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
