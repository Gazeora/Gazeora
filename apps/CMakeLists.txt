# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The user-facing programs, both built on gazeora_host_lib and gazeora_library:
#  - GazeoraCLI.exe: command line (library management, launch, attach + capture)
#  - Gazeora.exe:    launcher UI and stream server
add_subdirectory(cli)
add_subdirectory(gazeora)
