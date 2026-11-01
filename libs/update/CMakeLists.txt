# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Updates for Gazeora.exe: finds a newer release in the website's release feed, downloads its
# MSI, checks it against the published SHA-256 and starts it.
include(NlohmannJson)

add_library(gazeora_update STATIC
    src/mappers/release_json_mapper.cpp
    src/models/release.cpp
    src/models/release_version.cpp
    src/platform/bcrypt_file_hasher.cpp
    src/platform/msiexec_runner.cpp
    src/platform/update_location.cpp
    src/platform/winhttp_client.cpp
    src/platform/winhttp_request.cpp
    src/repositories/web_release_feed.cpp
    src/services/installer_downloader.cpp
    src/services/update_checker.cpp)
target_include_directories(gazeora_update
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_update
    PUBLIC gazeora_common
    PRIVATE nlohmann_json::nlohmann_json winhttp bcrypt shell32)
