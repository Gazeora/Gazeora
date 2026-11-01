# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

# The UI plugins add to Gazeora's pages: a panel's layout, read from its XML, and the values it
# binds to. Drawing it is Gazeora.exe's (apps/gazeora/src/plugin_ui), with ImGui.
include(PugiXml)

add_library(gazeora_plugin_ui STATIC
    # models: the layout and what it names
    src/models/ui_condition.cpp
    src/models/ui_ids.cpp
    # parsing: XML to a layout
    src/parsing/ui_element_factory.cpp
    src/parsing/ui_xml_node.cpp
    src/parsing/ui_xml_parser.cpp
    # services: routing a panel's keys
    src/services/ui_bindings.cpp)
target_include_directories(gazeora_plugin_ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gazeora_plugin_ui PRIVATE pugixml::static)
