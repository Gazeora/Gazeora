# Compiler and platform settings shared by every Gazeora target.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Gazeora must be built for 64-bit Windows (the hook runs inside x64 games).")
endif()

# The hook DLL is loaded into game processes that don't ship our compiler's runtime
# DLLs, so link the C/C++ runtime statically everywhere.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
if(NOT MSVC)
    # MinGW's D3D12 headers otherwise use a non-MSVC ABI for COM methods that return
    # structs (e.g. ID3D12Resource::GetDesc), which crashes at runtime.
    add_compile_definitions(WIDL_EXPLICIT_AGGREGATE_RETURNS)
endif()

# Release already builds with -O3 (CMake's default for GCC); RelWithDebInfo gets it too, so
# profiling builds run the same code. Debug stays -O0 for stepping through. MSVC's /O2 is its
# highest level.
if(NOT MSVC)
    string(REPLACE "-O2" "-O3" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()
