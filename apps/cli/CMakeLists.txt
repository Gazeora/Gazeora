# GazeoraCLI.exe: console front end.
add_executable(GazeoraCLI src/main.cpp)
target_link_libraries(GazeoraCLI PRIVATE gazeora_host_lib gazeora_library)
if(NOT MSVC)
    target_link_options(GazeoraCLI PRIVATE -municode)  # wmain entry point
endif()
add_dependencies(GazeoraCLI gazeora_hook)
