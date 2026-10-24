# `cmake --install`: what Gazeora needs to run, in one folder, as the programs find their hook,
# ONNX Runtime and models beside their own exe. Two components:
#  - Runtime (the default): Gazeora.exe, GazeoraCLI.exe, gazeora_hook.dll, onnxruntime.dll
#    and models\, plus the README, guides and licenses.
#  - Tools, only when asked for (--component Tools): the test apps and the stream probe.
# Tests and benchmarks are never installed.

# The default folder is set early on, in InstallPrefix.cmake.
include(GNUInstallDirs)
include(FetchContent)

install(TARGETS Gazeora GazeoraCLI gazeora_hook
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

# onnxruntime.dll and models\ come from libs/input, which knows where they are.

install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT Runtime)

# The license, its additional terms, and every shipped component's license (GPLv3 section 4
# and theirs ask for them to travel with the program).
install(FILES LICENSE NOTICE TRADEMARKS.md THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT Runtime)
foreach(dependency libusb minhook imgui sdl nlohmann_json onnxruntime)
    FetchContent_GetProperties(${dependency} SOURCE_DIR ${dependency}_dir)
endforeach()
set(GAZEORA_THIRD_PARTY_DIR ${CMAKE_INSTALL_DOCDIR}/third-party)
install(FILES ${libusb_dir}/COPYING
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME libusb-LICENSE.txt COMPONENT Runtime)
install(FILES ${minhook_dir}/LICENSE.txt
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME minhook-LICENSE.txt COMPONENT Runtime)
install(FILES ${imgui_dir}/LICENSE.txt
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME imgui-LICENSE.txt COMPONENT Runtime)
install(FILES ${sdl_dir}/LICENSE.txt
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME sdl-LICENSE.txt COMPONENT Runtime)
install(FILES ${nlohmann_json_dir}/LICENSE.MIT
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME nlohmann_json-LICENSE.txt COMPONENT Runtime)
install(FILES ${onnxruntime_dir}/LICENSE
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME onnxruntime-LICENSE.txt COMPONENT Runtime)
install(FILES ${onnxruntime_dir}/ThirdPartyNotices.txt
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} RENAME onnxruntime-ThirdPartyNotices.txt
    COMPONENT Runtime)
install(FILES licenses/hand-models-Apache-2.0.txt
    DESTINATION ${GAZEORA_THIRD_PARTY_DIR} COMPONENT Runtime)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT Runtime
    FILES_MATCHING PATTERN "*.md")

install(TARGETS gazeora_testapp gazeora_stream_probe
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools EXCLUDE_FROM_ALL)
