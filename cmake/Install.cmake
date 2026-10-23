# `cmake --install`: what Gazeora needs to run, in one folder, as the programs find their hook,
# ONNX Runtime and models beside their own exe. Two components:
#  - Runtime (the default): Gazeora.exe, GazeoraCLI.exe, gazeora_hook.dll, onnxruntime.dll
#    and models\, plus the README and guides.
#  - Tools, only when asked for (--component Tools): the test apps and the stream probe.
# Tests and benchmarks are never installed.

# The default folder is set early on, in InstallPrefix.cmake.
include(GNUInstallDirs)

install(TARGETS Gazeora GazeoraCLI gazeora_hook
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

# onnxruntime.dll and models\ come from libs/input, which knows where they are.

install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT Runtime)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT Runtime
    FILES_MATCHING PATTERN "*.md")

install(TARGETS gazeora_testapp gazeora_stream_probe
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Tools EXCLUDE_FROM_ALL)
