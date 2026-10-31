# Gazeora

Gazeora is a Windows game launcher that captures a game's frames as it renders them and
streams them live to devices on the local network. With the
[Gazeora Android app](https://github.com/Gazeora/Gazeora-Android) and a Cardboard-style VR
viewer, the stream can be shown as a screen in front of you, with head tracking.

Gazeora injects a capture hook (`gazeora_hook.dll`) into the game, copies each presented frame
on the GPU, encodes it as H.264 or H.265 with Media Foundation, and serves it over RTSP, HTTP,
UDP or USB.

## Current capabilities

- **Game library.** Add games by `.exe` or shortcut, launch them directly or through Steam,
  and track play time. Shared by `Gazeora.exe` and the `GazeoraCLI` command line.
- **Frame capture.** Direct3D 8, 9, 10, 11 and 12, and OpenGL. Vulkan is not supported.
- **Two releases.** The x64 release captures 64-bit games and the x86 release captures 32-bit
  games. Direct3D 8 games are always 32-bit.
- **Streaming.** One protocol at a time: RTSP with RTP over TCP or UDP, MPEG-TS over HTTP, or
  Gazeora's own UDP and USB formats for the Android app. The stream is always on. While no
  game runs, it shows Gazeora's own window.
- **Encoding.** H.264 or H.265, using the GPU's encoder through Media Foundation where one is
  available. The bitrate steps down when players report packet loss.
- **VR viewer mode.** Side-by-side output with lens distortion and color-fringe correction.
  The correction runs on the phone (the default) or on the PC for other players.
- **3D from depth.** In Direct3D 11 and 12 games, a separate view for each eye, built from the
  game's depth buffer.
- **Head tracking.** Phone rotation from the Android app becomes mouse movement, or right-stick
  movement for supported controllers, in the game in front.
- **Game controllers and touch.** A controller can drive the launcher, with an on-screen
  keyboard. Touches and the hand pointer in the Android app can also drive the launcher window.

See [Known limitations](#known-limitations) before relying on any of these.

## Requirements

To run Gazeora:

- Windows. The x64 release needs 64-bit Windows. No minimum Windows version is enforced or
  verified. CI builds and tests on GitHub's `windows-latest` runner.
- The release that matches the game: x64 for 64-bit games, x86 for 32-bit games.
- For H.265, a hardware or installed software H.265 encoder. Without one, only H.264 is
  offered.

To build Gazeora, see [Development](docs/development.md#building). In brief:

- CMake 3.20 or newer and a C++17 compiler: MSVC (Visual Studio 2022 or newer) or MinGW-w64.
- An internet connection for the first configure, which downloads the dependencies.
- The .NET SDK, only to build the MSI installer.

## Quick start

1. Install `Gazeora-v<version>-x64.msi` (or `-x86.msi` for 32-bit games) from the
   [releases page](https://github.com/Gazeora/Gazeora/releases), or build from source.
2. Start Gazeora. Add a game with **Add game**, by dragging an `.exe` or shortcut onto the
   window, or with **Open with Gazeora** in Explorer's right-click menu (MSI installs only).
3. Press **Play** on the game's page. The session card shows the capture status, frame rate,
   resolution, graphics API and stream address.
4. On another device on the same network, open the address from **Settings → Frame stream**
   in VLC, or open the Gazeora Android app, which finds the PC by itself.

[Getting started](docs/getting-started.md) walks through these steps in more detail.

## Documentation

| Guide | Covers |
|---|---|
| [Getting started](docs/getting-started.md) | Installing, the first game, the first stream |
| [Streaming](docs/streaming.md) | Watching the stream, protocols, codecs, USB setup, overlay, Wi-Fi |
| [VR viewer mode](docs/vr-viewer.md) | Lens correction, 3D from depth, head tracking |
| [Game controller](docs/game-controller.md) | Driving the launcher with a gamepad, the on-screen keyboard |
| [Command line](docs/command-line.md) | `GazeoraCLI` commands |
| [Configuration files](docs/configuration.md) | Every file and setting Gazeora stores |
| [Troubleshooting](docs/troubleshooting.md) | Common problems and how to diagnose them |
| [Installer](docs/installer.md) | MSI features, silent installs, uninstalling |
| [Tools](docs/tools.md) | The stream probe and the test app |
| [Stream format](docs/stream-format.md) | Wire formats of every protocol (canonical for the Android app) |
| [Architecture](docs/architecture.md) | Components, the capture-to-stream pipeline, code layout |
| [Development](docs/development.md) | Building, testing, benchmarks, formatting, releasing |

## Known limitations

- Each release captures only games of its own bitness. Use the x86 release for 32-bit games.
- Vulkan games are not captured.
- Games with kernel-level anti-cheat may block DLL injection. Do not use capture in online
  games whose terms forbid it. Game compatibility is not verified beyond the reports in the
  issue tracker.
- The stream is not encrypted or authenticated. Anyone on the local network can watch it and
  send pointer and head-tracking input to it.
- Each viewer receives its own copy of the stream (unicast), so several viewers multiply the
  network traffic.
- Changing the codec or protocol disconnects all players.
- RTSP over UDP cannot reach an Android emulator.
- The USB protocol needs a physical phone and a one-time driver setup
  (see [USB setup](docs/streaming.md#usb-setup)).
- Windows N editions do not include Media Foundation's H.264 encoder unless the Media Feature
  Pack is installed.

## Contributing

Bug reports, game compatibility reports and pull requests are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

Copyright (C) 2026 Nithin Aashik. Gazeora is free software under the
[GNU General Public License version 3](LICENSE).

[NOTICE](NOTICE) adds terms under section 7 of the GPL: modified versions keep the attribution
"Gazeora by Nithin Aashik" in their About screen, are marked as modified, and do not use the
Gazeora name or logo as their own (see [TRADEMARKS.md](TRADEMARKS.md)). Third-party components
are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Gazeora is also available under a commercial license. See
[COMMERCIAL-LICENSE.md](COMMERCIAL-LICENSE.md).
