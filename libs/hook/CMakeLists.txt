# The API-agnostic core and the D3D11 frame ring, as a static library so the unit
# tests can link them without injecting the DLL anywhere.
add_library(gazeora_hook_core STATIC
    src/core/capture_target.cpp
    src/core/depth_picker.cpp
    src/core/hook_util.cpp
    src/core/log.cpp
    src/core/pixel_convert.cpp
    src/core/shared_channel.cpp
    src/d3d11/d3d11_frame_ring.cpp)
target_include_directories(gazeora_hook_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gazeora_hook_core PUBLIC gazeora_common minhook d3d11 dxgi)

# Injected into the game. One folder per graphics API, plus the API-agnostic core.
# New APIs are registered in src/dllmain.cpp.
# Only d3d11/dxgi are linked. Every other runtime (d3d8, d3d9, d3d10, d3d12, opengl32,
# vulkan-1) is used through the game's own loaded copy; the hook loads one itself only
# in games the host launched, so it can be hooked before the game first uses it.
add_library(gazeora_hook SHARED
    src/dllmain.cpp
    src/dxgi/dxgi_hooks.cpp
    src/d3d8/d3d8_capture.cpp
    src/d3d8/d3d8_hooks.cpp
    src/d3d9/d3d9_capture.cpp
    src/d3d9/d3d9_hooks.cpp
    src/d3d10/d3d10_capture.cpp
    src/d3d11/d3d11_bridge.cpp
    src/d3d11/d3d11_capture.cpp
    src/d3d11/d3d11_depth.cpp
    src/d3d12/d3d12_capture.cpp
    src/d3d12/d3d12_depth.cpp
    src/d3d12/d3d12_queue_tracker.cpp
    src/opengl/gl_api.cpp
    src/opengl/gl_capture.cpp
    src/opengl/gl_hooks.cpp)
set_target_properties(gazeora_hook PROPERTIES PREFIX "")
target_link_libraries(gazeora_hook PRIVATE gazeora_hook_core)
if(MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # The static 32-bit runtime's _beginthreadex needs libmingw32's SEH handler, which only an
    # exe's startup code pulls in; a DLL has to ask for it, before libmingw32 goes by.
    target_link_options(gazeora_hook PRIVATE -Wl,-u,___mingw_SEH_error_handler)
endif()
