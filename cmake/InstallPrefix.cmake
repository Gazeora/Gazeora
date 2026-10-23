# The default install folder: Program Files for the x64 release, Program Files (x86) for the
# x86 one. CMake's own default depends on the toolchain (MinGW and some MSVC setups pick
# Program Files (x86) for both). Only when no prefix was given, which CMake says on the first
# configure only: so this runs right after CompilerSettings, before anything that could stop
# that configure.

if(GAZEORA_ARCH STREQUAL "x64")
    set(GAZEORA_PROGRAM_FILES_VAR "ProgramW6432")
else()
    set(GAZEORA_PROGRAM_FILES_VAR "ProgramFiles(x86)")
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND DEFINED ENV{${GAZEORA_PROGRAM_FILES_VAR}})
    file(TO_CMAKE_PATH "$ENV{${GAZEORA_PROGRAM_FILES_VAR}}/Gazeora" GAZEORA_DEFAULT_PREFIX)
    set(CMAKE_INSTALL_PREFIX "${GAZEORA_DEFAULT_PREFIX}" CACHE PATH "Install folder" FORCE)
endif()
