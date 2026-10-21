# Gazeora.exe: the launcher UI (Dear ImGui on Direct3D 11).
include(ImGui)
enable_language(RC)

add_executable(Gazeora WIN32
    main.cpp
    app.cpp
    game_session.cpp
    textures.cpp
    gazeora.rc)
target_link_libraries(Gazeora PRIVATE
    gazeora_host_lib gazeora_library imgui
    d3d11 dxgi dwmapi windowscodecs shell32 ole32 uuid)
if(NOT MSVC)
    target_link_options(Gazeora PRIVATE -municode)  # wWinMain entry point
endif()
add_dependencies(Gazeora gazeora_hook)

# windres doesn't report the icon as a dependency of the .rc, so rebuild when it changes.
set_source_files_properties(gazeora.rc PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gazeora.ico)
