# Getting started

This guide installs Gazeora, adds a game, and opens the stream on another device.

## 1. Pick a release

Gazeora is built as two separate releases. The hook runs inside the game, so it must match the
game's bitness.

| Release | Captures | MSI |
|---|---|---|
| x64 | 64-bit games | `Gazeora-v<version>-x64.msi` |
| x86 | 32-bit games, including every Direct3D 8 game | `Gazeora-v<version>-x86.msi` |

Both can be installed side by side. If a game is the other bitness, Gazeora refuses to inject
into it and names the release that captures it.

## 2. Install

Download the MSI from the [releases page](https://github.com/Gazeora/Gazeora/releases) and
run it. Installing needs administrator rights. The default features add a Start menu and
desktop shortcut, put `GazeoraCLI` on the PATH, add **Open with Gazeora** to Explorer, and add
a Windows Firewall rule for the stream on private networks. See [Installer](installer.md) for
each feature.

To build from source instead, see [Development](development.md#building).

If you built from source or installed with `cmake --install`, no firewall rule is added. Allow
`Gazeora.exe` on private networks when Windows asks, or other devices cannot connect.

## 3. Add a game

Start Gazeora. On the **Library** page, add a game in any of these ways:

- **Add game**, then pick one or more `.exe` files or shortcuts.
- Drag `.exe` files or shortcuts onto the window.
- In Explorer, right-click the game's `.exe` or shortcut and choose **Open with Gazeora**
  (MSI installs only; on Windows 11 it is under **Show more options**).

Open the game's page to change its name, launch arguments, working directory, cover art, and
**Launch through** (the exe, or Steam when the game is in a Steam library).

If the exe is a launcher that starts the real game as another process, set **Game process** to
the real game's exe name, so Gazeora captures the right program.

## 4. Play

Press **Play** on the game's page. Gazeora starts the game, injects the capture hook, and shows
a session card with the capture status, frame rate, resolution, graphics API and stream
address. While a game is running, the controller, touch and pointer input to the Gazeora window
is turned off, so the game has the controller to itself.

## 5. Watch the stream

The stream is always on. While no game is captured, it shows Gazeora's own window.

- **On a phone with the Gazeora app:** open the app on the same network. It finds the PC by
  itself. For USB, see [USB setup](streaming.md#usb-setup).
- **In VLC or another player:** open the address shown in **Settings → Frame stream**, such as
  `rtsp://192.168.1.20:8554/`.

[Streaming](streaming.md) explains protocols, codecs and network settings.
[VR viewer mode](vr-viewer.md) covers the phone viewer, 3D and head tracking.

If something does not work, see [Troubleshooting](troubleshooting.md).
