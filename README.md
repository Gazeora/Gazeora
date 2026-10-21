# Gazeora

Gazeora is a game launcher for Windows that captures a game's frames as it renders them and
streams them live to any device on your local network. The stream can also be laid out side by
side and lens-corrected, so a phone in a Cardboard-style VR viewer shows the game as a flat
screen in front of you.

Capture works by injecting a small hook DLL into the game, which copies each presented frame on
the GPU and hands it to the launcher with little delay. The launcher encodes the frames as H.264
or H.265 with the GPU's hardware encoder and serves them over RTSP, with the video carried as RTP over
UDP (or over TCP for players set to it), so standard players such as VLC, ffplay and OBS can open
the stream without extra software.

## Features

- **Game library.** Add games by exe or shortcut, launch them directly or through Steam, and
  track play time. The launcher follows games that restart themselves through a launcher
  (Steam, Ubisoft Connect, EA app).
- **Frame capture.** Supports Direct3D 8, 9, 10, 11 and 12 and OpenGL. Frames are shared with
  the launcher as GPU textures, not copied through system memory.
- **Network streaming.** An RTSP server that is always on, with H.264 or H.265 over RTP and UDP
  ([RFC 6184](https://www.rfc-editor.org/rfc/rfc6184),
  [RFC 7798](https://www.rfc-editor.org/rfc/rfc7798)) and RTCP reports
  ([RFC 3550](https://www.rfc-editor.org/rfc/rfc3550)), or RTP interleaved in the RTSP
  connection for players set to TCP. Any device on the LAN can watch. When no game is being
  captured, the stream shows the Gazeora logo.
- **H.264 or H.265.** H.264 (8 Mbit/s for 1080p30, High profile) plays on every device. H.265
  gives the same picture at 70% of the bits (5.6 Mbit/s), which means fewer packets for Wi-Fi to
  lose. Chosen in **Settings → Frame stream**; see [Choosing a codec](#choosing-a-codec).
- **Hardware encoding.** Uses the GPU's encoder through Media Foundation (NVIDIA NVENC, AMD AMF
  or Intel Quick Sync). H.264 falls back to Microsoft's software encoder; H.265 to an installed
  software one, if any, and otherwise isn't offered.
- **Low latency.** About 4 to 25 ms from capture to arrival on the local network, depending on
  frame size. Frames above 1080p are scaled down on the GPU before read-back, and the stream is
  paced to a steady 30 fps.
- **Built for Wi-Fi.** Frames are cut into small slices and packets are paced instead of burst,
  so a lost packet smears a strip of the picture briefly instead of freezing it. The bitrate adapts
  to the loss players report.
- **VR viewer mode.** Side-by-side output shaped to the phone's screen, with barrel
  pre-distortion and color-fringe correction for the viewer's lenses, adjustable while you
  watch. On by default.
- **Command line.** `GazeoraCLI` manages the same library and can attach to a running game.

## How it works

```
 Game process                          Gazeora.exe                         Any device on the LAN
┌──────────────────────┐  shared   ┌───────────────────────────────┐  RTSP (TCP)  ┌───────────────┐
│ gazeora_hook.dll     │  GPU      │ CaptureClient                 │◄────────────►│ VLC / ffplay  │
│  hooks Present /     │ textures, │  → TextureReader (GPU scale,  │              │ / OBS         │
│  SwapBuffers,        │──fences──►│     VR lens pass, read-back)  │  RTP/H.26x   │               │
│  copies the frame    │ + shared  │  → FrameStreamServer          │══(UDP)══════►│               │
│  into a frame ring   │  memory   │     (H.26x encode, RTP, RTCP) │              │               │
└──────────────────────┘           └───────────────────────────────┘              └───────────────┘
```

1. The launcher starts the game suspended, or attaches to it if it is already running, and
   injects `gazeora_hook.dll`.
2. The hook detects the graphics API, hooks its present call and copies each frame into a
   small ring of shared textures. A shared memory block carries the frame metadata and the
   handles; GPU fences order the copies without stalling the game.
3. The launcher opens those textures on its own D3D11 device. While someone is watching, it
   scales the frame on the GPU (and applies the VR layout), reads it back and passes it to the
   stream server.
4. The stream server converts the frame to NV12, encodes it as H.264 or H.265 on its own thread and sends
   RTP packets to every player that has pressed play. Frames that arrive faster than they can be
   encoded are dropped, so delay never builds up.

## Requirements

- Windows 10 or 11, 64-bit. Only 64-bit games can be captured.
- CMake 3.20 or newer.
- A C++17 compiler: MSVC (Visual Studio 2022 or newer) or MinGW-w64 (for example, the one
  bundled with CLion).
- An internet connection for the first configure. CMake downloads
  [MinHook](https://github.com/TsudaKageyu/minhook) 1.3.3,
  [Dear ImGui](https://github.com/ocornut/imgui) 1.91.5 and
  [GoogleTest](https://github.com/google/googletest) 1.15.2.

## Building

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Visual Studio generators work too (`-G "Visual Studio 17 2022" -A x64`). Everything is placed in
`build/bin/`. Keep the executables and `gazeora_hook.dll` in the same folder, because the
launcher loads the hook from its own directory.

| Output | Description |
|---|---|
| `Gazeora.exe` | The launcher (Dear ImGui on Direct3D 11) and stream server |
| `GazeoraCLI.exe` | Command-line front end for the same library |
| `gazeora_hook.dll` | The capture hook, injected into games |
| `gazeora_testapp.exe` | A test "game" that renders with each supported API |
| `gazeora_stream_probe.exe` | An RTSP client that measures the stream |
| `gazeora_*_tests.exe` | Unit tests (with `GAZEORA_BUILD_TESTS=ON`, the default) |

## Using Gazeora

### Launching and capturing games

Start `Gazeora.exe`, add games with **Add game** or by dragging exes or shortcuts onto the
window, and launch them with **Play with capture**. The game's session card shows the capture
status, frame rate, resolution, graphics API and the stream address.

### Watching the stream

The stream address is shown in **Settings → Capture → Frame stream**, one entry for each network
address of the PC, for example:

```
rtsp://192.168.1.20:8554/
```

Open it in VLC (**Media → Open Network Stream**) on any device on the same network. The first
time Gazeora starts, allow it through Windows Firewall on **private networks**, or other devices
cannot connect.

VLC buffers one second of every network stream by default, which is most of the delay you will
see. On a wired PC, 0 to 100 ms of network caching works well. Phones on Wi-Fi need some slack
or the picture stalls whenever the network hiccups, so start around 300 ms there:

| Platform | Setting |
|---|---|
| Windows / macOS / Linux | Open Network Stream → Show more options → Caching, or Preferences → All → Input / Codecs → Network caching |
| Android | Settings → Advanced → Network caching value |
| iOS | Settings → Network Caching Level → Low Latency (Lowest Latency stalls on most Wi-Fi) |

On a weak link the stream adapts by itself; see [Wi-Fi and packet loss](#wi-fi-and-packet-loss).

### Choosing a codec

**Settings → Frame stream → Video codec** switches between H.264 and H.265. The choice is saved
in `stream.ini` (`[stream] codec=h264` or `h265`). Changing it disconnects anyone watching, as a
player can't switch codecs mid-stream; open the stream again.

| | H.264 | H.265 |
|---|---|---|
| Bitrate at 1080p30 | 8 Mbit/s | 5.6 Mbit/s |
| Picture | Reference | About the same at 70% of the bits (measured: 47.6 dB PSNR at 5.6 Mbit/s against 47.1 dB for H.264 at 8, on panning detailed content with NVENC) |
| Players | Every device | VLC on PC, iOS and Android; iPhones from the 6s and most recent Android phones decode it in hardware |
| Encoder | Always available | Needs a GPU encoder (NVIDIA GTX 950 or newer, AMD RX 400 series or newer, Intel 6th gen or newer) or an installed software one |

Both are cut into about 24 slices per frame. Which one copes better with Wi-Fi loss depends on
the link: H.265 sends fewer packets, so fewer frames get hit, but its loss concealment in VLC may
be weaker than H.264's. Try both with the phone you use.

### Wi-Fi and packet loss

UDP doesn't resend lost packets. Gazeora is built so that a lost packet costs as little as
possible:

- **Small slices.** Each frame is cut into about 24 slices of whole macroblock rows, mostly a
  packet or a few each. A lost packet damages one strip, which the player fills in from the
  previous frame; the rest of the frame decodes normally. (FFmpeg, which VLC decodes with,
  ignores slices beyond the 32nd, so there are never more.)
- **Key frames every second.** A key frame every second repairs whatever loss has smeared.
- **Paced sending.** Each frame's packets are spread over about 20 ms, in bursts a millisecond
  apart, instead of going out at once, so Wi-Fi access points and phones don't overflow.
- **Adaptive bitrate.** Players report their packet loss in RTCP receiver reports. While a player
  reports 2% loss or more, the bitrate steps down from 8 to 6, 4.5 and 3 Mbit/s. Once the reports
  have been clean for 8 seconds it climbs back a step every 3 seconds. One player with a bad link
  lowers the bitrate for everyone. (VLC's first report claims about 98% loss with a negative
  total; reports with a negative total count as no loss.)

The session card and **Settings → Frame stream** show a *Weak network* warning with the current
loss and bitrate while this is happening. A 5 GHz Wi-Fi network, and the PC on a cable rather than
Wi-Fi (otherwise every packet crosses the air twice), make the biggest difference. To try a bad
link on a good one, run the stream probe with `--drop PERCENT`.

#### RTSP over TCP

If a phone's picture keeps smearing or stopping, set the player to RTSP over TCP (VLC for iOS:
the **RTSP over TCP** switch in Settings; desktop VLC: `--rtsp-tcp`; other players: their own
RTSP transport setting). RTP then travels inside the RTSP connection and nothing is lost. A link too slow
for the bitrate makes the stream skip frames rather than smear: while more than 256 KB is still
queued for the player, frames are skipped up to the next key frame, and the skipped share
counts as loss, so the bitrate steps down. TCP can add delay when Wi-Fi retransmits, so a
little more network caching helps.

#### Stream log

`%APPDATA%\Gazeora\stream.log` records what every player did, rewritten at each start: connects
and disconnects (with the reason), the transport each chose, every receiver report (loss,
jitter), bitrate changes, skipped frames, and UDP players that stopped sending reports, which
usually means they stopped receiving. When a stream stalls on one device, this shows whether
the player went quiet, the connection closed, or the link got worse.

### VR viewer mode

VR mode is on by default and can be switched off in **Settings → VR headset**. Choose the
viewer's lenses (Google Cardboard 2015 or 2014 presets, or no correction) and the phone's screen
shape, then play the stream full screen in landscape and put the phone in the viewer. Changes
reach the stream immediately, so the distortion, picture size, lens separation and color-fringe
sliders can be tuned with the headset on. A preview of the output is shown below the sliders.

Both eyes see the same image, so a game appears as a flat screen in front of you. There is no
stereo depth or head tracking.

### Command line

```
GazeoraCLI list
GazeoraCLI add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] [--capture]
GazeoraCLI remove <id | name>
GazeoraCLI launch <id | name> [--capture]
GazeoraCLI attach <pid | process.exe>
```

While attached, press **S** to save the current frame as a BMP and **Q** to quit. The CLI does
not stream; streaming is part of `Gazeora.exe`.

## Tools

### Stream probe

`gazeora_stream_probe.exe` plays a stream the way VLC does and reports what arrived: frame rate,
bitrate, packet loss, frames damaged by loss, key frames, gaps between frames, and
capture-to-arrival latency, which it computes from the RTCP sender reports. Like VLC, it sends
receiver reports, so the server adapts to the loss it sees. `--drop PERCENT` discards that share
of arriving packets, to simulate a weak Wi-Fi link. `--tcp` asks for RTP over TCP, like VLC's
RTSP over TCP setting. `--save` writes everything received as a raw H.264 or H.265 stream; play it in VLC
to see what the loss did to the picture.

```
gazeora_stream_probe.exe [rtsp://host:port/] [--seconds N] [--save stream.h264|.h265] [--drop PERCENT]
                         [--tcp]
```

Latency is exact when the probe runs on the streaming PC. Run from another machine, it is off by
the difference between the two clocks.

### Test app

`gazeora_testapp.exe` opens a 1280×720 window with a moving square, for testing capture without
a real game. It uses Direct3D 11 by default; pass `--d3d8` (MinGW builds only), `--d3d9`,
`--d3d10`, `--d3d12` or `--opengl` for the other APIs.

## Testing

```powershell
cd build
ctest --output-on-failure
```

The tests use GoogleTest and run Direct3D on WARP, so no GPU is required. They are split into
separate executables because the hook and the host each attach to a per-process shared memory
block:

| Suite | Covers |
|---|---|
| `gazeora_hook_tests` | Hook core against a fake host |
| `gazeora_host_tests` | Capture client, injector, launcher, frame read-back, GPU scaling and the VR pass, H.264 and H.265 encoding (software and hardware), slicing, and RTP round trips through the system decoders, the RTSP server |
| `gazeora_transport_tests` | The real hook and host exchanging frames in one process |
| `gazeora_library_tests` | The game library shared by both front ends |

## Configuration files

| File | Contents |
|---|---|
| `%APPDATA%\Gazeora\library.json` | The game library, shared by `Gazeora.exe` and `GazeoraCLI`. Set `GAZEORA_LIBRARY` to use another path. |
| `%APPDATA%\Gazeora\stream.ini` | The stream codec (`[stream]`) and VR viewer settings (`[vr]`), stored next to the library file. Plain INI, safe to edit by hand. |
| `%APPDATA%\Gazeora\stream.log` | What stream players did, rewritten at each start. See [Stream log](#stream-log). |

## Project layout

```
common/          Shared-memory protocol between the hook and the host (header only)
hook/            gazeora_hook.dll
  core/            API-agnostic: shared channel, hook helpers, pixel conversion, logging
  d3d8/ ... dxgi/  One folder per graphics API: d3d8, d3d9, d3d10, d3d11, d3d12, dxgi, opengl
host/            gazeora_host_lib, shared by both front ends
  launch/          Starting games (directly or through Steam) and injecting the hook
  capture/         Receiving the hook's frames, GPU scaling, the VR pass and read-back
  encode/          H.264 / H.265 encoding through Media Foundation
  stream/          The RTSP server, RTP packetization, RTCP
src/
  app/             Gazeora.exe
  cli/             GazeoraCLI.exe
  library/         The game library behind both
tools/           The test app and the stream probe
tests/           GoogleTest suites, in the same folders as the code they test
cmake/           Compiler settings, dependency fetching and formatting targets
```

Headers live under each module's `include/gazeora/<module>/`, sources under `src/`, with the same
subfolders, so `host/src/stream/frame_stream.cpp` implements
`host/include/gazeora/host/stream/frame_stream.h`.

## Stream format

| Property | Value |
|---|---|
| Control | RTSP 1.0 over TCP, port 8554 (falls back to a free port) |
| Transport | RTP/AVP over UDP unicast, or interleaved in the RTSP connection (RTP/AVP/TCP) when the player asks; multicast is refused |
| Payload | H.264 High profile (RFC 6184: payload type 96, packetization mode 1, FU-A for large NAL units), or H.265 Main (RFC 7798: payload type 96, FU for large NAL units); parameter sets (H.265: VPS, SPS, PPS) in the SDP and before every key frame |
| Encoding | Low-latency CBR, no B-frames, about 24 slices per frame, a key frame every second |
| Bitrate | H.264: 8 Mbit/s, stepping down to 6, 4.5 and 3 Mbit/s while players report loss or TCP players fall behind. H.265: 70% of those |
| Size | Up to 1920×1080; in VR mode the phone's screen shape within that size |
| Pacing | UDP: a frame's packets spread over about 20 ms, at least 4 per millisecond. TCP: frames skipped up to the next key frame while 256 KB is queued |
| Frame rate | A steady 30 fps. Games running slower have their last frame repeated, and the idle logo is sent at 30 fps too, because VLC drops frames as late when a stream runs much slower |
| Timing | RTCP sender reports every second; RTP timestamps mark capture time; receiver reports drive the bitrate |

## Known limitations

- Only 64-bit games can be captured. Vulkan is not supported yet.
- Games with kernel-level anti-cheat may block DLL injection. Don't use capture with online
  games whose terms forbid it.
- Each viewer gets their own copy of the stream (unicast), so several viewers multiply the
  Wi-Fi traffic.
- Windows N editions need the Media Feature Pack for the H.264 encoder.
- Changing the codec disconnects players; they have to open the stream again.
- The stream is unencrypted and unauthenticated. Anyone on the local network can watch it.

## Contributing

The code is formatted with clang-format using the `.clang-format` file at the root. Run
`cmake --build build --target format` to format everything, or `--target format-check` to
check. The [pre-commit](https://pre-commit.com) configuration runs the same checks before each
commit:

```
pip install pre-commit
pre-commit install
```
