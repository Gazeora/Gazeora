# Troubleshooting

Common problems, what causes them, and how to fix them. When reporting a problem, include your
Gazeora version (**Settings → About**), the protocol and codec, and
`%APPDATA%\Gazeora\stream.log` (see [Stream log](streaming.md#stream-log)).

## Capture

### Settings → Capture shows "Missing: reinstall Gazeora"

`gazeora_hook.dll` is not next to `Gazeora.exe`. The launcher loads the hook from its own
folder. Reinstall, or for a build, keep the contents of `build\bin\` together.

### The game is refused because of its bitness

Each release captures only games of its own bitness, and the message names the release that
captures this game. Install the other release (x86 for 32-bit games, x64 for 64-bit games).
Both can be installed side by side.

### The game starts but nothing is captured

- **Vulkan games** are not supported.
- **Anti-cheat** may block DLL injection. Do not use capture in online games whose terms forbid
  it.
- **Launchers.** If the exe starts a launcher that then starts the real game as another
  process, set **Game process** on the game's page to the real game's exe name.
- **Administrator.** A game running as administrator cannot be injected by a program that is not
  running as administrator.

To check whether capture works on this PC at all, try the [test app](tools.md#test-app).

### "3D unavailable: streaming in 2D"

3D from depth needs a Direct3D 11 or 12 game drawing a 3D scene with a depth buffer at the back
buffer's size. Menus, videos and other graphics APIs stay 2D. See
[3D from depth](vr-viewer.md#3d-from-depth).

## Connecting

### Other devices cannot open the stream

1. Check that the device is on the same network as the PC.
2. Check **Settings → Frame stream**. If it shows *Stream not running* with an error, the
   stream could not start.
3. Use the exact address shown there. If port 8554 was taken when Gazeora started, the stream
   uses another port, and the address shows it.
4. Check Windows Firewall. The MSI's rule allows `Gazeora.exe` on **private** networks from the
   local subnet only. If Windows considers the network public, the stream is blocked. Changing
   the network to private in Windows' network settings allows it; only do this on a network you
   trust, because the stream is not encrypted or authenticated.
5. For RTSP over UDP, the player must be able to receive UDP from the PC. It does not work
   through an Android emulator's NAT. Use RTSP over TCP instead.

### The Gazeora app does not find the PC

Discovery uses UDP port 8556 and needs the same firewall permission as the stream. Some Wi-Fi
networks block broadcasts or isolate clients from each other; in that case the app cannot reach
the PC on that network at all. See [Discovery](stream-format.md#discovery).

### Players disconnect after changing a setting

Changing the protocol or codec restarts the stream and disconnects every player. Open the stream
again.

## Picture quality and delay

### The picture smears, stalls or shows a *Weak network* warning

Packets are being lost or the link is too slow. The bitrate lowers itself while this happens.

- Switch to **RTSP over TCP** or **HTTP**, which lose nothing and skip frames instead.
- Lower **Stream quality**, or use 30 fps.
- Use 5 GHz Wi-Fi, and connect the PC by cable if possible.
- Check `stream.log` for whether the player went quiet, the connection closed, or loss rose.

See [Wi-Fi and packet loss](streaming.md#wi-fi-and-packet-loss).

### VLC has about one second of delay

VLC's default network caching is about one second. Lower it; see
[Player delay](streaming.md#player-delay).

### H.265 is shown as "not supported on this PC"

There is no hardware H.265 encoder and no installed software H.265 encoder. Use H.264.

### No video on a Windows N edition

Windows N editions do not include Media Foundation's codecs. Install the Media Feature Pack for
your Windows version.

## USB

See [USB setup](streaming.md#usb-setup) first.

- **The phone does not switch to accessory mode while adb is running.** adb holds the phone in
  normal mode. Settings says so when this is the cause. Run `adb kill-server`, then reconnect.
- **The phone no longer shows its files in Explorer.** The USB driver was installed for this
  phone model without USB debugging. Click **Settings → Frame stream → Restore file transfer**.
  Afterwards, the phone needs USB debugging to stream over USB.
- **An emulator does not work over USB.** USB needs a physical phone.

## Head tracking

The status line in **Controls → Head tracking** shows where it stops:

| Status | Meaning |
|---|---|
| Waiting for the phone: play in the Gazeora app, in a VR viewer | No head poses are arriving. The app sends them only while VR mode is on with the lens correction done by the phone |
| The phone is tracking your head. Waiting for a game launched from here to be in front | Poses arrive, but no game launched from Gazeora is the foreground window |
| Your head is turning the game's camera with the mouse / with the controller | Working |

If the camera turns too much or too little, adjust the sensitivity; see
[Head tracking](vr-viewer.md#head-tracking). If the game runs as administrator, Gazeora must run
as administrator too.

## Game controller

See [Game controller troubleshooting](game-controller.md#troubleshooting).
