# GoogleTest, fetched once for the unit tests (tests/) and the benchmarks (bench/). Named so it
# doesn't hide CMake's own GoogleTest module (gtest_discover_tests).

include_guard(GLOBAL)
include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
