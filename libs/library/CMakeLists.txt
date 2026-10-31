# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The user's games, stored as JSON in %APPDATA%\Gazeora\library.json.
include(NlohmannJson)

add_library(gazeora_library STATIC
    src/mappers/game_json_mapper.cpp
    src/mappers/launch_via_names.cpp
    src/platform/library_location.cpp
    src/platform/shell_link_resolver.cpp
    src/platform/version_info_name_resolver.cpp
    src/platform/win32_file_checker.cpp
    src/repositories/json_game_repository.cpp
    src/services/game_library.cpp
    src/services/launch_spec_factory.cpp
    src/services/missing_game_pruner.cpp)
target_include_directories(gazeora_library
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_library
    PUBLIC gazeora_host_lib version shell32 ole32 uuid
    PRIVATE nlohmann_json::nlohmann_json)
