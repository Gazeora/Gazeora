# Frame recorder: an example Gazeora plugin

A plugin to start from. It adds:

- **Record to file**, a protocol that writes the game's frames to an `.h264` or `.h265` file in
  `Videos\Gazeora` instead of streaming them;
- a **Frame recorder** card on the **Settings** page: recording on or off, the folder, how the
  recording stands, and **Start a new file**;
- `frame-recorder.ini`, its own settings, in Gazeora's settings folder.

It uses nothing of Gazeora's but the plugin interface,
[`gazeora_plugin.h`](../../../libs/plugin_api/include/gazeora/plugin_api/gazeora_plugin.h), as a
plugin written outside this repository does.

## Build

From a Developer PowerShell for VS, in the repository root:

```powershell
cmake -S examples/plugins/frame_recorder -B build-recorder -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-recorder
```

This makes `build-recorder\frame-recorder.dll`, for the architecture of the compiler in use
(x64 or x86). Gazeora's own build also builds it (`GAZEORA_BUILD_EXAMPLES`), and
`gazeora_plugin_tests` tests it (`tests/examples/frame_recorder_test.cpp`).

## Install

On Gazeora's **Plugins** page, choose **Install from file**, pick the DLL, and restart Gazeora.
Then choose **Record to file** as the protocol in **Settings → Frame stream** and play a game.

## Learn from it

[Plugin development](../../../docs/plugin-development.md#the-example-plugin) walks through how
it's built: which class does what, how a C++ object sits behind each C struct, threads, and
what Gazeora and the plugin each own.

## License

Like the rest of this repository: `GPL-3.0-only OR LicenseRef-Gazeora-Commercial`. A plugin that
copies this code takes that license with it; one that uses only `gazeora_plugin.h` (MIT) can
have any license. See [License](../../../docs/plugin-development.md#license).
