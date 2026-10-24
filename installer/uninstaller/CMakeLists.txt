# Uninstall.exe: in the install folder, for those who look for one there. The MSI adds it.
enable_language(RC)

set(GAZEORA_ICON_FILE ${PROJECT_SOURCE_DIR}/apps/gazeora/resources/gazeora.ico)
configure_file(resources/release_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/release_info.h @ONLY)

add_executable(gazeora_uninstall WIN32 EXCLUDE_FROM_ALL
    src/main.cpp
    src/installed_product.cpp
    src/uninstall_launcher.cpp
    resources/uninstall.rc)
set_target_properties(gazeora_uninstall PROPERTIES OUTPUT_NAME Uninstall)
target_include_directories(gazeora_uninstall PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(gazeora_uninstall PRIVATE msi shell32)
if(NOT MSVC)
    target_link_options(gazeora_uninstall PRIVATE -municode)  # wWinMain entry point
endif()
set_source_files_properties(resources/uninstall.rc PROPERTIES OBJECT_DEPENDS
    "${GAZEORA_ICON_FILE};${CMAKE_CURRENT_BINARY_DIR}/release_info.h")
gazeora_sign(gazeora_uninstall)
