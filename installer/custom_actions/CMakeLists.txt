# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# gazeora_setup_actions.dll: the MSI's own custom actions. The logic is a library of its own so
# the tests can run it against a fake installer session.
add_library(gazeora_setup_actions_core STATIC
    src/disk_user_data_folder.cpp
    src/user_data_removal.cpp)
target_include_directories(gazeora_setup_actions_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(gazeora_setup_actions SHARED EXCLUDE_FROM_ALL
    src/entry_points.cpp
    src/msi_installer_session.cpp
    src/setup_actions.def)  # undecorated names, as the MSI's CustomAction table gives them
set_target_properties(gazeora_setup_actions PROPERTIES PREFIX "")
target_link_libraries(gazeora_setup_actions PRIVATE gazeora_setup_actions_core msi)
if(MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # As for gazeora_hook.dll: a DLL has to ask for libmingw32's SEH handler itself. And the .def
    # names the exports without their @N, which the linker matches up on its own.
    target_link_options(gazeora_setup_actions PRIVATE
        -Wl,-u,___mingw_SEH_error_handler -Wl,--enable-stdcall-fixup)
endif()
gazeora_sign(gazeora_setup_actions)
