# Gazeora

Gazeora is a game launcher for Windows that captures a game's frames as it renders them and
streams them live to any device on your local network. The stream can also be laid out side by
side and lens-corrected, so a phone in a Cardboard-style VR viewer shows the game as a flat
screen in front of you.

Capture works by injecting a small hook DLL into the game, which copies each presented frame on
the GPU and hands it to the launcher with little delay. The launcher encodes the frames as H.264
or H.265 with the GPU's hardware encoder and streams them over the protocol chosen in Settings:
RTSP (with RTP over TCP or UDP), which standard players such as VLC, ffplay and OBS open without
extra software; an MPEG-TS stream over HTTP; or, for the Gazeora Android app, bare encoded frames
over UDP or a USB cable. The Android app finds the PC on the network by itself.

## Features

- **Game library.** Add games by exe or shortcut, launch them directly or through Steam, and
  track play time. The launcher follows games that restart themselves through a launcher
  (Steam, Ubisoft Connect, EA app).
- **Frame capture.** Supports Direct3D 8, 9, 10, 11 and 12 and OpenGL. Frames are shared with
  the launcher as GPU textures, not copied through system memory.
- **Network streaming.** A stream that is always on, over one of five protocols (see
  [Choosing a protocol](#choosing-a-protocol)): RTSP with H.264 or H.265 over RTP
  ([RFC 6184](https://www.rfc-editor.org/rfc/rfc6184),
  [RFC 7798](https://www.rfc-editor.org/rfc/rfc7798)) and RTCP reports
  ([RFC 3550](https://www.rfc-editor.org/rfc/rfc3550)), interleaved in the RTSP connection or
  over UDP; MPEG-TS over HTTP; or bare frames over UDP or USB (Android Open Accessory) for the
  Android app. Any device on the LAN can watch. When no game is being captured, the stream shows
  the Gazeora logo.
- **Found by the Android app.** The PC answers the app's search on the network, so nobody types
  an address.
- **H.264 or H.265.** H.264 (High profile) plays on every device. H.265 gives the same picture
  at 70% of the bits, which means fewer packets for Wi-Fi to lose. Chosen in **Settings → Frame
  stream**, with the stream quality (10, 20 or 35 Mbit/s at full quality; 20 by default); see
  [Choosing a codec](#choosing-a-codec).
- **Hardware encoding.** Uses the GPU's encoder through Media Foundation (NVIDIA NVENC, AMD AMF
  or Intel Quick Sync). H.264 falls back to Microsoft's software encoder; H.265 to an installed
  software one, if any, and otherwise isn't offered.
- **Low latency.** About 4 to 25 ms from capture to arrival on the local network, depending on
  frame size. Frames above 1080p are scaled down on the GPU before read-back, and the stream is
  paced to a steady 30 fps.
- **Built for Wi-Fi.** Frames are cut into small slices and packets are paced instead of burst,
  so a lost packet smears a strip of the picture briefly instead of freezing it. The bitrate adapts
  to the loss players report.
- **VR viewer mode.** Each eye's picture with barrel pre-distortion and color-fringe
  correction for the viewer's lenses, adjustable while you watch. The Android app does the
  lens correction itself, so the stream carries the game at full sharpness; other players get
  frames already split and warped. On by default.
- **3D from depth.** In D3D11 and D3D12 games, the hook copies the game's depth buffer and
  Gazeora makes a separate view for each eye from it, so the scene has real depth in the
  viewer.
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
  [Dear ImGui](https://github.com/ocornut/imgui) 1.91.5,
  [libusb](https://github.com/libusb/libusb) 1.0.29 (for the USB protocol) and
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
| `gazeora_bench.exe` | Benchmarks of every stage, from capture to RTP packets (with `GAZEORA_BUILD_BENCH=ON`, the default) |

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

### Choosing a protocol

**Settings → Frame stream** picks the protocol. One runs at a time; changing it restarts the
stream and disconnects anyone watching. The choice is saved in `stream.ini`
(`[stream] protocol=`).

| Protocol | `protocol=` | Address | Players | Notes |
|---|---|---|---|---|
| RTSP over TCP (default) | `rtsp-tcp` | `rtsp://PC:8554/` | Android app, VLC, ffplay, OBS | RTP inside the RTSP connection: nothing lost, a slow link skips frames. Players set to UDP get `461 Unsupported Transport` (VLC then retries over TCP) |
| RTSP over UDP | `rtsp-udp` | `rtsp://PC:8554/` | VLC, ffplay, OBS, Android app | The lowest delay on a good network; a lost packet smears the picture briefly. Doesn't reach an Android emulator |
| UDP | `udp` | `udp://PC:8554` | Android app | Bare encoded frames in datagrams, no RTSP or RTP. The app says hello once a second and the frames go back to it, through NAT too |
| HTTP | `http` | `http://PC:8554/stream.ts` | Android app, VLC, ffplay | A live MPEG-TS stream; a slow link skips frames |
| USB | `usb` | the cable | Android app | Android Open Accessory: the PC switches the phone to accessory mode, which opens the app. See [USB setup](#usb-setup) |

Whatever the protocol, Gazeora answers the Android app's search on UDP port 8556: the app
broadcasts `GAZEORA-DISCOVER 1` (and, in the emulator, asks 10.0.2.2) and lists every PC that
answers. Many Wi-Fi networks drop broadcasts, so every 3 seconds it also asks each address on
its network (up to a /22). Allow Gazeora through Windows Firewall on private networks for this
too.

#### USB setup

1. **Once per PC, install the USB driver:** set the protocol to USB, plug the phone in, and
   click **Settings → Frame stream → Install USB driver**. Windows asks for an administrator.
   This puts Windows' own WinUSB driver on the *Android Accessory Interface* of any Android
   phone in accessory mode.
2. **Without USB debugging on the phone,** the button names the phone ("Install USB driver for
   Redmi Note 11T 5G"): Gazeora can only ask a phone to switch to accessory mode through a
   driver that can send the request, and Windows' file transfer driver (MTP) can't. The driver
   then goes on the phone itself too, for that phone model: **Windows can't browse its files
   over USB on this PC** until you click **Restore file transfer**. A phone that's streaming
   then is reconnected by power-cycling its USB port, so it comes back for file transfer
   without a replug. With USB debugging on, the phone's adb interface does the job and file
   transfer stays as it is.
3. Plug the phone in. It switches to accessory mode, which opens the app on the stream, and
   stays there until it's unplugged.

The driver package is signed with a certificate Gazeora makes for it and adds to the PC's
trusted roots and publishers (named *Gazeora USB accessory driver*); the certificate's key is
deleted as soon as the package is signed, so it can't sign anything else. To remove it later:
`certlm.msc`, Trusted Root Certification Authorities and Trusted Publishers. Each install
replaces the package before it, keeping the phones added earlier.

adb (Android Studio) keeps working while the phone streams: Gazeora opens only the accessory
interface. But while adb holds the phone in its normal mode, Gazeora can't ask it to switch:
run `adb kill-server` for a moment, and Settings says so when that's what's in the way.
Instead of Gazeora's driver, [UsbDk](https://github.com/daynix/UsbDk/releases), or WinUSB for
*Android Accessory Interface* from [Zadig](https://zadig.akeo.ie/), work too; then adb loses
the phone while it streams.

### Stream overlay

**Settings → Stream overlay** draws streaming details into the frames themselves, so every
player shows them, Gazeora's app or VLC alike: the protocol and viewers; codec, resolution and
frame rate; bitrate and packet loss; the encoder; this PC's clock (handy for judging delay:
photograph the PC's clock and the phone together); and a line of your own text. Pick the
details, the corner, the text size and how dark the panel behind it is; changes show in the
stream at once. In VR mode the panel is drawn into each eye, away from the lens edges. The
choices are saved in `stream.ini` under `[overlay]`. It's off until switched on.

### Choosing a codec

**Settings → Frame stream → Video codec** switches between H.264 and H.265. The choice is saved
in `stream.ini` (`[stream] codec=h264` or `h265`). Changing it disconnects anyone watching, as a
player can't switch codecs mid-stream; open the stream again.

**Stream quality** next to it sets the bitrate at full quality: more bits keep fine detail and
motion sharp. Balanced (10 Mbit/s) suits weak or busy Wi-Fi, High (20) good Wi-Fi, Very high
(35) 5 GHz Wi-Fi close to the router, or USB. It's saved as `[stream] max_bitrate_mbps` and
applies at once.

| | H.264 | H.265 |
|---|---|---|
| Bitrate at full quality | The stream quality picked: 10, 20 (default) or 35 Mbit/s | 70% of it: 7, 14 or 24.5 Mbit/s |
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
for the bitrate makes the stream skip frames rather than smear: while more than 512 KB is still
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
viewer's lenses (Google Cardboard 2015 or 2014 presets, or no correction), then play the stream
full screen in landscape and put the phone in the viewer. Changes reach the stream immediately,
so the distortion, picture size, lens separation and color-fringe sliders can be tuned with the
headset on.

**Lens correction done by** picks where the eyes are drawn:

- **The phone (Gazeora app)**, the default. Frames go out flat, in the game's own shape, and
  the app draws them into both eyes for its own screen with OpenGL ES. Every pixel of the stream
  is the game's, so the picture is sharper than a pre-warped frame of the same size. The lens
  settings travel with every frame (see [Stream format](#stream-format)), so slider changes show
  at once. Players other than the app show these frames flat.
- **This PC**, for VLC and other players. Frames go out split, warped and shaped to the phone's
  screen, which you pick here.

#### 3D from depth

**3D from the game's depth** gives each eye its own view. The hook finds the game's main depth
buffer (the one with the most draws in the back buffer's shape) and copies it beside each
frame; Gazeora then shifts every pixel sideways by its depth, in opposite directions for the two
eyes, like a pair of eyes converged on the subject in the middle of the view (the character, in
a third-person game): the subject sits at the screen and stays put, farther things go back,
nearer ones come out. It works out on its own whether the game stores depth reversed (1 near,
as most games now do) or not, and follows the subject's depth, eased over a few frames.

Where one eye sees behind something, the game drew nothing; the background is stretched over
the gap. Near objects' depth is widened by a couple of pixels first, so their antialiased edges
move with them instead of tearing off into the background.

- **3D depth** is how much the two views differ for things far away, as a share of an eye's
  width (0.012, about a degree in a Cardboard-style viewer, by default). More strains the eyes
  and widens the stretched strips beside near objects.
- **Pop-out** is how far things nearer than the subject may come out of the screen, as a share
  of the 3D depth.

The two views go out side by side in one frame, each squeezed to half the width (the same
frame size as 2D), and the app stretches each back for its eye. The game's details page shows
whether 3D is working. Notes:

- D3D11 and D3D12 games only. Other APIs, the idle picture, and games without a depth buffer in
  the back buffer's shape stay 2D.
- Things drawn over the scene without depth, such as the HUD, take the depth of what's behind
  them and bend with it.
- D3D12 games are followed through their depth views, so start them from Gazeora. A D3D12 game
  attached to while already running works only if it moves its depth buffer out of the depth
  write state each frame, as deferred renderers do.

There is no head tracking yet.

### Command line

```
GazeoraCLI list
GazeoraCLI add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] [--capture]
GazeoraCLI remove <id | name>
GazeoraCLI launch <id | name> [--capture]
GazeoraCLI attach <pid | process.exe> [--depth] [--save-after SECONDS]
GazeoraCLI run <game.exe> [--args A] [--depth] [--save-after SECONDS]
```

While attached, press **S** to save the current frame as a BMP and **Q** to quit. `run` starts
an exe hooked from its first instruction, like a game launched from the library, without
adding it there. `--depth` also asks for the game's depth buffer, reports how many frames came
with it, and saves the frame's 3D pair (`frame_N_3d.bmp`) with each frame. `--save-after`
saves once, that many seconds in, and quits. The CLI does not stream; streaming is part of
`Gazeora.exe`.

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
`--d3d10`, `--d3d12` or `--opengl` for the other APIs. In D3D11 and D3D12 it also draws a
depth-tested 3D scene, for testing 3D from depth: D3D11 with 24-bit depth (0 near), D3D12 with
reversed 32-bit float depth that goes to a shader-readable state after drawing, like a deferred
renderer. `--d3d12-depth-write` keeps it in the depth write state instead.

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

## Benchmarks

`gazeora_bench.exe` times every stage on this PC's GPU and encoders and prints the mean, median,
99th percentile and worst case. It lives in `bench/`, apart from the unit tests, and isn't part
of `ctest`; run it from a Release build (as above):

```powershell
cd build\bin
.\gazeora_bench.exe                                 # everything, about a minute
.\gazeora_bench.exe --gtest_filter=Encode.*:Stream.* # some areas
```

| Area | Measures |
|---|---|
| `Convert` | The hook's per-pixel conversion for each CPU readback layout; BGRA8 to NV12 at 720p, 1080p, 1440p |
| `Capture` | The frame transport in one process; `TextureReader` at full size, scaled, 10-bit and VR; BMP screenshots; the real hook in `gazeora_testapp.exe` for each graphics API (opens a window per API) |
| `Encode` | Each codec on the hardware and the software encoder: time per call, input-to-output latency, frame sizes, open time; the whole `VideoPipeline` |
| `Rtp`, `Rtcp`, `Rtsp`, `Bitrate` | Packetizing and depacketizing both codecs, parameter sets, RTCP reports, RTSP messages, the bitrate controller |
| `Stream` | The `FrameMailbox` hand-off, `PacketPacer` timing, and the whole server: `PushFrame` to a loopback UDP player receiving the frame |
| `Library` | Loading, saving and searching a 500-game library |

## Configuration files

| File | Contents |
|---|---|
| `%APPDATA%\Gazeora\library.json` | The game library, shared by `Gazeora.exe` and `GazeoraCLI`. Set `GAZEORA_LIBRARY` to use another path. |
| `%APPDATA%\Gazeora\stream.ini` | The stream codec and protocol (`[stream]`) and VR viewer settings (`[vr]`: lenses, `lens_on_phone`, and `stereo`, `stereo_amount`, `stereo_front` for 3D), stored next to the library file. Plain INI, safe to edit by hand. |
| `%APPDATA%\Gazeora\stream.log` | What stream players did, rewritten at each start. See [Stream log](#stream-log). |

## Project layout

```
libs/                 Libraries, in dependency order
  common/               Shared-memory protocol between the hook and the host (header only)
  hook/                 gazeora_hook.dll, injected into the game
    core/                 API-agnostic: shared channel, hook helpers, pixel conversion, logging
    d3d8/ ... opengl/     One folder per graphics API: d3d8, d3d9, d3d10, d3d11, d3d12, dxgi, opengl
  host/                 gazeora_host_lib, shared by both front ends
    launch/               Starting games (directly or through Steam) and injecting the hook
    capture/              Receiving the hook's frames, GPU scaling, the VR pass and read-back
    encode/               The VideoEncoder interface, Media Foundation behind it, BGRA to NV12
    stream/               The stream server, its protocols and what it's made of (see below)
  library/              The game library behind both front ends
apps/
  gazeora/              Gazeora.exe
    src/                  App, with pages/ (one file per page), ui/ (theme, widgets),
                          platform/ (shell dialogs) and settings/ (stream.ini)
    resources/            Icon and resource script
  cli/                  GazeoraCLI.exe
tools/                The test app and the stream probe
tests/                GoogleTest suites, in the same folders as the code they test
bench/                gazeora_bench, the benchmarks
cmake/                Compiler settings, dependency fetching and formatting targets
```

Headers live under each library's `include/gazeora/<module>/`, sources under `src/`, with the
same subfolders, so `libs/host/src/stream/frame_stream.cpp` implements
`libs/host/include/gazeora/host/stream/frame_stream.h`, included as
`"gazeora/host/stream/frame_stream.h"`.

### The stream server

`FrameStreamServer` coordinates; each part does one job and most can be tested alone:

| Part | Job |
|---|---|
| `FrameMailbox` | Hands frames from the capture thread to the encoder, newest wins |
| `VideoPipeline` | NV12 conversion, encoding, parameter sets, RTP packetization. Takes the encoder from a factory, so tests use a fake one |
| `VideoEncoder` | Interface; `CreateVideoEncoder()` returns the Media Foundation one |
| `RtpPayloadFormat` | Everything that differs between H.264 and H.265, one class per codec |
| `BitrateController` | The bitrate ladder, driven by reported loss. Pure logic |
| `RtspRequest`, `RtspResponse`, `RtspReader`, `TransportSpec` | Parsing and building RTSP messages |
| `RtspConnection` | One player's socket and output queue (RTSP, and HTTP players) |
| `FrameOutput` | Interface for the protocols other than RTSP: `UdpFrameOutput`, `HttpTsOutput`, `UsbAccessoryOutput` |
| `TsMuxer` | Frames into an MPEG transport stream, for HTTP. Pure logic |
| `FrameHeader`, `ChunkFrame`, `StreamMessage` | The GZF1 wire format of the UDP and USB protocols (`frame_packet.h`) |
| `DiscoveryResponder` | Answers the Android app's search for the PC |
| `PacketPacer` | Spreads a frame's UDP packets over about 20 ms |
| `rtcp`, `net`, `StreamLog` | RTCP reports, sockets, the stream log |

RTSP methods are dispatched through a table in `frame_stream.cpp`; supporting another method
is one handler and one entry.

## Stream format

| Property | Value |
|---|---|
| Control | RTSP 1.0 over TCP, port 8554 (falls back to a free port) |
| Transport | RTP/AVP over UDP unicast, or interleaved in the RTSP connection (RTP/AVP/TCP) when the player asks; multicast is refused |
| Payload | H.264 High profile (RFC 6184: payload type 96, packetization mode 1, FU-A for large NAL units), or H.265 Main (RFC 7798: payload type 96, FU for large NAL units); parameter sets (H.265: VPS, SPS, PPS) in the SDP and before every key frame |
| Encoding | Low-latency CBR, no B-frames, about 24 slices per frame, a key frame every second |
| Bitrate | The stream quality (H.264: 20 Mbit/s by default; H.265: 70% of it), stepping down to 75%, 55%, 40% and 28% of it while players report loss or TCP players fall behind |
| Size | Up to 1920×1080, in the game's shape; when the PC does the VR lens correction, the phone's screen shape within that size |
| View info | When the phone does the VR lens correction, every frame carries an SEI message (user data unregistered, payload type 5) just before its slices: the 16-byte id `GazeoraViewInfo1`, then text such as `vr=1 pair=1 k1=0.340 k2=0.550 scale=0.700 sep=0.500 chroma=0.000`. `pair=1` means the frame holds two views side by side, the left eye's on the left, each squeezed to half the width. Other players skip it |
| Pacing | UDP: a frame's packets spread over about 20 ms, at least 4 per millisecond. TCP: frames skipped up to the next key frame while 512 KB is queued |
| Frame rate | A steady 30 fps. Games running slower have their last frame repeated, and the idle logo is sent at 30 fps too, because VLC drops frames as late when a stream runs much slower |
| Timing | RTCP sender reports every second; RTP timestamps mark capture time; receiver reports drive the bitrate |
| Key frame requests | An RTCP Picture Loss Indication or Full Intra Request (RFC 4585, RFC 5104) from a player gets a key frame at once. The Android app sends one when packets are lost for good, and shows nothing (rather than a smeared picture) until the key frame comes |

The other protocols carry the same encoded frames:

| Protocol | Format |
|---|---|
| HTTP | `GET /stream.ts` answers with an endless MPEG-TS (ISO/IEC 13818-1): one program, video on PID 0x100 (stream type 0x1B or 0x24), the PAT and PMT before every key frame, one PES per frame with an access unit delimiter and a PTS 100 ms after its PCR |
| UDP, USB | GZF1 messages: a 24-byte big-endian header (`"GZF1"`, type, key frame and H.265 flags, width, height, the player's loss, frame number, 90 kHz timestamp, then the chunk index and count over UDP or the payload length over USB), then the frame's Annex B access unit with its parameter sets in front of a key frame. Over UDP a frame is cut into datagrams of at most 1400 bytes. Players say hello once a second (UDP players silent for 5 s are dropped), ask for a key frame when they start or lose a frame, and say bye. `libs/host/include/gazeora/host/stream/frame_packet.h` has the details |
| Discovery | UDP port 8556: `GAZEORA-DISCOVER 1` gets back one line of JSON, e.g. `{"service":"gazeora","version":1,"name":"MY-PC","protocol":"http","port":8554,"path":"/stream.ts","codec":"h264"}` |

## Known limitations

- Only 64-bit games can be captured. Vulkan is not supported yet.
- Games with kernel-level anti-cheat may block DLL injection. Don't use capture with online
  games whose terms forbid it.
- Each viewer gets their own copy of the stream (unicast), so several viewers multiply the
  Wi-Fi traffic.
- Windows N editions need the Media Feature Pack for the H.264 encoder.
- Changing the codec or the protocol disconnects players; they have to open the stream again.
- RTSP over UDP can't reach an Android emulator: the RTP goes to an address inside the
  emulator's NAT. Use any other protocol there.
- The USB protocol needs the USB driver (see [USB setup](#usb-setup)), and a phone on a cable
  rather than an emulator. A phone without USB debugging loses Windows' file transfer on this PC
  while it's set up for streaming.
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
