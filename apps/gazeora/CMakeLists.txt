# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Gazeora.exe: the launcher UI (Dear ImGui on Direct3D 11).
include(ImGui)
include(NlohmannJson)
enable_language(RC)

# The Plugins page's work, apart from its UI: the plugin list on gazeora.github.io and the
# bundled one, downloads, and changing the plugins folder (as an administrator). A library of
# its own so its tests can link it; it's only Gazeora.exe's.
add_library(gazeora_plugin_manager STATIC
    src/plugin_manager/mappers/plugin_listing_json_mapper.cpp
    src/plugin_manager/models/plugin_category.cpp
    src/plugin_manager/models/plugin_entry.cpp
    src/plugin_manager/models/plugin_id.cpp
    src/plugin_manager/platform/plugin_download_location.cpp
    src/plugin_manager/repositories/bundled_plugin_index.cpp
    src/plugin_manager/repositories/web_plugin_index.cpp
    src/plugin_manager/services/plugin_directory.cpp
    src/plugin_manager/services/plugin_folder_editor.cpp)
target_include_directories(gazeora_plugin_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_plugin_manager
    PUBLIC gazeora_plugins gazeora_update
    PRIVATE gazeora_plugin_api nlohmann_json::nlohmann_json)

add_executable(Gazeora WIN32
    src/main.cpp
    src/app.cpp
    src/app_actions.cpp
    src/game_session.cpp
    src/textures.cpp
    src/gamepad/button_repeat.cpp
    src/gamepad/gamepad_control.cpp
    src/gamepad/gamepad_event_queue.cpp
    src/gamepad/gamepad_presses.cpp
    src/gamepad/imgui_gamepad_input.cpp
    src/gamepad/pane_navigation.cpp
    src/head/controller_look_driver.cpp
    src/head/device_switching_driver.cpp
    src/head/foreground_game_focus.cpp
    src/head/head_look.cpp
    src/head/head_tracking_control.cpp
    src/head/keyboard_mouse_watch.cpp
    src/head/last_used_device.cpp
    src/head/mouse_look_driver.cpp
    src/head/send_input_mouse_mover.cpp
    src/keyboard/imgui_text_sink.cpp
    src/keyboard/keyboard_layout.cpp
    src/keyboard/keyboard_view.cpp
    src/keyboard/on_screen_keyboard.cpp
    src/pages/chrome.cpp
    src/pages/controls_page.cpp
    src/pages/details_page.cpp
    src/pages/head_tracking_card.cpp
    src/pages/home_page.cpp
    src/pages/launch_via_choices.cpp
    src/pages/library_page.cpp
    src/pages/plugin_card.cpp
    src/pages/plugin_card_text.cpp
    src/pages/plugin_category_filter.cpp
    src/pages/plugin_category_look.cpp
    src/pages/plugin_grid.cpp
    src/pages/plugins_page.cpp
    src/pages/settings_page.cpp
    src/pages/update_card.cpp
    src/platform/elevated_run.cpp
    src/platform/game_open_request.cpp
    src/platform/shell.cpp
    src/platform/usb_driver.cpp
    src/plugin_ui/gamepad_status_cache.cpp
    src/plugin_ui/gamepad_ui_values.cpp
    src/plugin_ui/gazeora_ui_values.cpp
    src/plugin_ui/imgui_ui_renderer.cpp
    src/plugin_ui/plugin_panels_view.cpp
    src/plugin_ui/ui_field_drawer.cpp
    src/plugin_ui/ui_input_buffers.cpp
    src/plugin_ui/ui_look.cpp
    src/plugins/elevated_plugin_installer.cpp
    src/plugins/plugin_command.cpp
    src/plugins/plugin_list_fetch.cpp
    src/plugins/plugin_manager_control.cpp
    src/remote/imgui_pointer_input.cpp
    src/remote/pointer_event_queue.cpp
    src/report/issue_link.cpp
    src/report/windows_description.cpp
    src/settings/controls_settings.cpp
    src/settings/settings_file.cpp
    src/settings/stream_settings.cpp
    src/settings/update_settings.cpp
    src/ui/focus_edge.cpp
    src/ui/gamepad_scroll.cpp
    src/ui/gamepad_status_line.cpp
    src/ui/head_tracking_status_line.cpp
    src/ui/logo_frame.cpp
    src/ui/text_format.cpp
    src/ui/theme.cpp
    src/ui/ui_scale.cpp
    src/ui/update_status_line.cpp
    src/ui/widgets.cpp
    src/update/update_control.cpp
    resources/gazeora.rc)
target_include_directories(Gazeora PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# config/build_features.h
target_compile_definitions(Gazeora PRIVATE
    GAZEORA_STREAM_OVERLAY=$<BOOL:${GAZEORA_STREAM_OVERLAY}>)
# version_info.h, for gazeora.rc's version info.
configure_file(resources/version_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/version_info.h @ONLY)
target_include_directories(Gazeora PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(Gazeora PRIVATE
    gazeora_host_lib gazeora_library gazeora_plugins gazeora_plugin_manager gazeora_input
    gazeora_plugin_ui
    gazeora_update imgui
    d3d11 dxgi dwmapi windowscodecs shell32 ole32 uuid
    crypt32 wintrust setupapi)  # the USB driver install
if(NOT MSVC)
    target_link_options(Gazeora PRIVATE -municode)  # wWinMain entry point
endif()
add_dependencies(Gazeora gazeora_hook)
gazeora_sign(Gazeora)  # a named publisher, not "Unknown", when it asks to run as administrator

# windres doesn't report what the .rc includes as its dependencies, so rebuild when they change.
set_source_files_properties(resources/gazeora.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/gazeora.ico;${CMAKE_CURRENT_BINARY_DIR}/version_info.h")
