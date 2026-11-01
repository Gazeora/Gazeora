# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# Game Controller: reads game controllers through SDL (SdlGamepadDriver), with the plugin's own
# switch (ControllerSettings) and its card on the Controls page (ui/controller_panel.xml). SDL is
# linked into this DLL only.
include(Sdl)

add_library(gazeora_controller STATIC src/sdl_gamepad_driver.cpp)
target_include_directories(gazeora_controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_controller PUBLIC gazeora_input PRIVATE SDL3::SDL3-static)

gazeora_add_bundled_plugin(gazeora_controller_plugin
    ID gazeora-controller CATEGORY controller NAME "Game Controller"
    SUMMARY "Move around Gazeora with an Xbox, PlayStation, Switch Pro or other controller."
    SOURCES src/controller_panel_values.cpp src/controller_plugin.cpp
            src/controller_settings.cpp
    LIBS gazeora_controller)
# The card's layout as a string in the DLL (kControllerPanelXml).
gazeora_embed_text(gazeora_controller_plugin ui/controller_panel.xml
    controller_panel_xml.h kControllerPanelXml)
