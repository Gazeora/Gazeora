# Standalone window rendering with each supported API, to test the hook against.
add_executable(gazeora_testapp
    src/main.cpp
    src/app.cpp
    src/render_d3d9.cpp
    src/render_d3d10.cpp
    src/render_d3d11.cpp
    src/render_d3d12.cpp
    src/render_opengl.cpp)
target_include_directories(gazeora_testapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_testapp PRIVATE d3d9 d3d10 d3d11 d3d12 dxgi opengl32 gdi32)

# The Windows SDK dropped d3d8.h; MinGW still ships it.
include(CheckIncludeFileCXX)
check_include_file_cxx(d3d8.h TESTAPP_HAVE_D3D8_H)
if(TESTAPP_HAVE_D3D8_H)
    target_sources(gazeora_testapp PRIVATE src/render_d3d8.cpp)
    target_compile_definitions(gazeora_testapp PRIVATE TESTAPP_HAVE_D3D8)
endif()
