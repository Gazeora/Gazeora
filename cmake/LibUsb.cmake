# libusb: reaches Android phones in USB accessory mode, for the USB stream protocol. Built
# from source here, like MinHook: it has no CMake build of its own.

include(FetchContent)
FetchContent_Declare(libusb
    URL https://github.com/libusb/libusb/releases/download/v1.0.29/libusb-1.0.29.tar.bz2
    URL_HASH SHA256=5977fc950f8d1395ccea9bd48c06b3f808fd3c2c961b44b0c2e6e29fc3a70a85
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(libusb)

set(LIBUSB_SOURCE_DIR ${libusb_SOURCE_DIR}/libusb)
add_library(libusb STATIC
    ${LIBUSB_SOURCE_DIR}/core.c
    ${LIBUSB_SOURCE_DIR}/descriptor.c
    ${LIBUSB_SOURCE_DIR}/hotplug.c
    ${LIBUSB_SOURCE_DIR}/io.c
    ${LIBUSB_SOURCE_DIR}/strerror.c
    ${LIBUSB_SOURCE_DIR}/sync.c
    ${LIBUSB_SOURCE_DIR}/os/events_windows.c
    ${LIBUSB_SOURCE_DIR}/os/threads_windows.c
    ${LIBUSB_SOURCE_DIR}/os/windows_common.c
    ${LIBUSB_SOURCE_DIR}/os/windows_usbdk.c
    ${LIBUSB_SOURCE_DIR}/os/windows_winusb.c)
target_include_directories(libusb PUBLIC ${LIBUSB_SOURCE_DIR}
    PRIVATE ${LIBUSB_SOURCE_DIR}/os)
# Built the way its own build does: windows.h's min/max macros on (the project turns them
# off), and the ANSI Windows API.
foreach(definition NOMINMAX UNICODE _UNICODE)
    target_compile_options(libusb PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/U${definition},-U${definition}>)
endforeach()

if(MSVC)
    target_include_directories(libusb PRIVATE ${libusb_SOURCE_DIR}/msvc)
    # Its own code, as released: compares an endpoint direction with a request type (C5287).
    target_compile_options(libusb PRIVATE /wd5287)
else()
    # What libusb's configure script works out on Windows with MinGW.
    set(LIBUSB_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/libusb_config)
    file(WRITE ${LIBUSB_CONFIG_DIR}/config.h
        "#define DEFAULT_VISIBILITY\n"
        "#define ENABLE_LOGGING 1\n"
        "#define PLATFORM_WINDOWS 1\n"
        "#define PRINTF_FORMAT(a, b) __attribute__((__format__(__printf__, a, b)))\n")
    target_include_directories(libusb PRIVATE ${LIBUSB_CONFIG_DIR})
endif()
