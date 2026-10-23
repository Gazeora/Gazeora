# Gazeora.exe: the launcher UI (Dear ImGui on Direct3D 11).
include(ImGui)
enable_language(RC)

add_executable(Gazeora WIN32
    src/main.cpp
    src/app.cpp
    src/app_actions.cpp
    src/game_session.cpp
    src/textures.cpp
    src/pages/chrome.cpp
    src/pages/details_page.cpp
    src/pages/home_page.cpp
    src/pages/library_page.cpp
    src/pages/settings_page.cpp
    src/platform/shell.cpp
    src/platform/usb_driver.cpp
    src/settings/stream_settings.cpp
    src/ui/logo_frame.cpp
    src/ui/text_format.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
    resources/gazeora.rc)
target_include_directories(Gazeora PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Gazeora PRIVATE
    gazeora_host_lib gazeora_library imgui
    d3d11 dxgi dwmapi windowscodecs shell32 ole32 uuid
    crypt32 wintrust setupapi)  # the USB driver install
if(NOT MSVC)
    target_link_options(Gazeora PRIVATE -municode)  # wWinMain entry point
endif()
add_dependencies(Gazeora gazeora_hook)

# windres doesn't report the icon as a dependency of the .rc, so rebuild when it changes.
set_source_files_properties(resources/gazeora.rc PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/gazeora.ico)
