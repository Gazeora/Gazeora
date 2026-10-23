cmake_minimum_required(VERSION 3.20)
project(Gazeora LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# The compiler picks the release: a 64-bit one builds the x64 release (for 64-bit games), a
# 32-bit one the x86 release (for 32-bit games). Each is the whole of Gazeora, built on its own.
include(CompilerSettings)  # toolchain checks, GAZEORA_ARCH, static runtime, common definitions
include(InstallPrefix)     # Program Files or Program Files (x86), by GAZEORA_ARCH
include(MinHook)
include(LibUsb)
include(ClangFormat)       # format / format-check targets

# Keep the executables and gazeora_hook.dll side by side; they load the DLL from their own folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Where they end up, for the files copied beside them: bin\, or bin\<config>\ with a multi-config
# generator (Visual Studio, Ninja Multi-Config).
get_property(GAZEORA_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GAZEORA_MULTI_CONFIG)
    set(GAZEORA_RUNTIME_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>)
else()
    set(GAZEORA_RUNTIME_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

add_subdirectory(libs)   # common, hook (gazeora_hook.dll), host, library
add_subdirectory(apps)   # Gazeora.exe (launcher UI) and GazeoraCLI.exe
add_subdirectory(tools)  # gazeora_testapp.exe, gazeora_stream_probe.exe

option(GAZEORA_BUILD_TESTS "Build the GoogleTest unit tests" ON)
if(GAZEORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)  # gazeora_*_tests.exe: run with ctest
endif()

option(GAZEORA_BUILD_BENCH "Build the benchmarks (gazeora_bench.exe)" ON)
if(GAZEORA_BUILD_BENCH)
    add_subdirectory(bench)  # gazeora_bench.exe: run directly, in a Release build
endif()

include(Install)  # cmake --install: the programs and what they load, in one folder
