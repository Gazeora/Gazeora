# `format` rewrites all project sources with clang-format; `format-check` fails if any
# file isn't formatted. Both use the .clang-format file at the repository root.

find_program(CLANG_FORMAT_EXE NAMES clang-format
    HINTS "$ENV{VCINSTALLDIR}/Tools/Llvm/x64/bin")

if(NOT CLANG_FORMAT_EXE)
    message(STATUS "clang-format not found; format targets disabled")
    return()
endif()

file(GLOB_RECURSE GAZEORA_FORMAT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/common/*.h
    ${CMAKE_SOURCE_DIR}/hook/*.h ${CMAKE_SOURCE_DIR}/hook/*.cpp
    ${CMAKE_SOURCE_DIR}/host/*.h ${CMAKE_SOURCE_DIR}/host/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/tools/*.h ${CMAKE_SOURCE_DIR}/tools/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.h ${CMAKE_SOURCE_DIR}/tests/*.cpp)

add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${GAZEORA_FORMAT_SOURCES}
    COMMENT "Formatting sources with clang-format"
    VERBATIM)

add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${GAZEORA_FORMAT_SOURCES}
    COMMENT "Checking source formatting"
    VERBATIM)
