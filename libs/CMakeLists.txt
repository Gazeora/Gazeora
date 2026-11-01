# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The libraries, in dependency order. Each keeps its public headers under
# include/gazeora/<module>/ and its sources under src/, with the same subfolders.
add_subdirectory(common)   # gazeora_common: shared-memory protocol between hook and host
add_subdirectory(hook)     # gazeora_hook_core + gazeora_hook.dll, injected into the game
add_subdirectory(host)     # gazeora_host_lib: launch, capture, encode, stream
add_subdirectory(library)  # gazeora_library: the game library shared by both front ends
add_subdirectory(input)    # gazeora_input: game controllers; gazeora_sdl_input: SDL's driver
add_subdirectory(plugin_ui)   # gazeora_plugin_ui: plugins' UI panels, read from XML
add_subdirectory(plugin_api)  # gazeora_plugin_api: the C header plugin DLLs build against
add_subdirectory(plugin_kit)  # gazeora_plugin_kit: the plugin interface and Gazeora's C++, both ways
add_subdirectory(plugins)  # gazeora_plugins: loads plugin DLLs into the host's registries
add_subdirectory(update)   # gazeora_update: finding, downloading and starting updates
add_subdirectory(plugin_manager)  # gazeora_plugin_manager: the plugin list, installs, removals
