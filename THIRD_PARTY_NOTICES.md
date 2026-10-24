# Third-party software

Gazeora is built with the components below. Each is under its own license, all compatible with
the GNU GPL version 3. `cmake --install` copies each one's license text beside this file, into
`third-party\`.

| Component | Version | Used for | License | In Gazeora as |
|---|---|---|---|---|
| [libusb](https://libusb.info) | 1.0.29 | USB stream to Android phones | LGPL 2.1 or later | Linked statically into Gazeora.exe |
| [MinHook](https://github.com/TsudaKageyu/minhook) | 1.3.3 | Hooking the game's graphics API | BSD 2-Clause | Linked statically into gazeora_hook.dll |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.5 | The launcher UI | MIT | Linked statically into Gazeora.exe |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | The game library file | MIT | Compiled in (header-only) |
| [ONNX Runtime](https://onnxruntime.ai) | 1.20.1 | Running the hand tracking models | MIT (its own components: see its ThirdPartyNotices) | onnxruntime.dll |
| [MediaPipe](https://github.com/google-ai-edge/mediapipe) palm detection and hand landmark models, converted to ONNX by the [OpenCV model zoo](https://huggingface.co/opencv) | 2023feb | Hand tracking | Apache 2.0 | models\\*.onnx |

[GoogleTest](https://github.com/google/googletest) (BSD 3-Clause) builds the unit tests and
benchmarks only; it is not part of what is installed.

## libusb (LGPL)

libusb is linked statically. As the LGPL asks, you can rebuild Gazeora with a modified libusb:
the complete source of Gazeora and its build (which fetches libusb from its release, see
`cmake/LibUsb.cmake`) are available under the GNU GPL version 3.
