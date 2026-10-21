cmake_minimum_required(VERSION 3.20)
project(Gazeora LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CompilerSettings)  # toolchain checks, static runtime, common definitions
include(MinHook)
include(ClangFormat)       # format / format-check targets

# Keep the executables and gazeora_hook.dll side by side; they load the DLL from their own folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(common)         # gazeora_common: protocol shared by hook and host
add_subdirectory(hook)           # gazeora_hook.dll: injected into the game
add_subdirectory(host)           # gazeora_host_lib: injects the hook and receives frames
add_subdirectory(src)            # Gazeora.exe (launcher UI) and GazeoraCLI.exe
add_subdirectory(tools/testapp)  # gazeora_testapp.exe: test "game" for each API
add_subdirectory(tools/streamprobe)  # gazeora_stream_probe.exe: measures the RTSP stream

option(GAZEORA_BUILD_TESTS "Build the GoogleTest unit tests" ON)
if(GAZEORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)      # gazeora_*_tests.exe: run with ctest
endif()
