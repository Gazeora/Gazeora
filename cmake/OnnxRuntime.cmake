# ONNX Runtime 1.20.1: runs the hand tracking models on the CPU. The prebuilt Windows release
# has only an MSVC import library, so just its C API header is compiled against and
# onnxruntime.dll is loaded at run time (any compiler can do that).

include(FetchContent)
FetchContent_Declare(onnxruntime
    URL https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-win-x64-1.20.1.zip
    URL_HASH SHA256=78d447051e48bd2e1e778bba378bec4ece11191c9e538cf7b2c4a4565e8f5581
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(onnxruntime)

add_library(onnxruntime_headers INTERFACE)
target_include_directories(onnxruntime_headers INTERFACE ${onnxruntime_SOURCE_DIR}/include)
if(NOT MSVC)
    # The one SAL annotation the header uses that MinGW's sal.h lacks.
    target_compile_definitions(onnxruntime_headers INTERFACE _Frees_ptr_opt_=)
endif()
set(ONNXRUNTIME_DLL ${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll)
