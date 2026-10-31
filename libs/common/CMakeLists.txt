# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Header-only: the shared-memory protocol between the hook and the host.
add_library(gazeora_common INTERFACE)
target_include_directories(gazeora_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
