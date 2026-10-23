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
  [Choosing a protocol](docs/streaming.md#choosing-a-protocol)): RTSP with H.264 or H.265 over RTP
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
  [Choosing a codec](docs/streaming.md#choosing-a-codec).
- **Hardware encoding.** Uses the GPU's encoder through Media Foundation (NVIDIA NVENC, AMD AMF
  or Intel Quick Sync). H.264 falls back to Microsoft's software encoder; H.265 to an installed
  software one, if any, and otherwise isn't offered.
- **Low latency.** About 4 to 25 ms from capture to arrival on the local network, depending on
  frame size. Frames stay on the GPU from capture to the encoder, packets go out from a thread
  of their own while the next frame encodes, and the stream runs at a steady 60 fps (or 30).
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
- **Hand gestures.** A webcam on the PC turns your hands into a touch screen in the air: the
  right index fingertip moves the pointer; with the left index up, folding the right index
  and opening it again clicks; with the left hand pinching, the right index scrolls. Runs
  MediaPipe's hand models on the CPU.
- **Touch from the phone.** While VR viewer mode is off, touches on the picture in the Android
  app control the Gazeora window, and only it: the PC's own mouse and other programs are left
  alone. A tap clicks and a finger moving scrolls. It works with Gazeora minimized too, as
  long as the stream shows the window. While a game is launching or running, touches and hand
  gestures are both off; they come back when it closes. Touches arrive on the USB cable, or
  over UDP port 8557 for the network protocols.
- **Command line.** `GazeoraCLI` manages the same library and can attach to a running game.

## How it works

```
 Game process                          Gazeora.exe                         Any device on the LAN
┌──────────────────────┐  shared   ┌───────────────────────────────┐  RTSP (TCP)  ┌───────────────┐
│ gazeora_hook.dll     │  GPU      │ CaptureClient                 │◄────────────►│ VLC / ffplay  │
│  hooks Present /     │ textures, │  → TextureReader (GPU scale,  │              │ / OBS         │
│  SwapBuffers,        │──fences──►│     VR lens pass, NV12 on GPU)│  RTP/H.26x   │               │
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
   scales the frame on the GPU (and applies the VR layout), converts it to NV12 there and
   passes the texture to the stream server. A game drawing fewer frames than the stream sends
   has its last frame encoded again, with nothing read or converted.
4. The stream server encodes the frame as H.264 or H.265 on its own thread, on the GPU's
   encoder straight from the texture, and sends RTP packets to every player that has pressed
   play. Frames that arrive faster than they can be encoded are dropped, so delay never builds
   up. Without a GPU encoder that takes the textures, or with the stream overlay on, the frame
   is read back and encoded from memory instead.

## Requirements

- Windows 10 or 11, 64-bit. Only 64-bit games can be captured.
- CMake 3.20 or newer.
- A C++17 compiler: MSVC (Visual Studio 2022 or newer) or MinGW-w64 (for example, the one
  bundled with CLion).
- An internet connection for the first configure. CMake downloads
  [MinHook](https://github.com/TsudaKageyu/minhook) 1.3.3,
  [Dear ImGui](https://github.com/ocornut/imgui) 1.91.5,
  [libusb](https://github.com/libusb/libusb) 1.0.29 (for the USB protocol),
  [ONNX Runtime](https://github.com/microsoft/onnxruntime) 1.20.1 and MediaPipe's palm detection
  and hand landmark models as converted by the [OpenCV model zoo](https://github.com/opencv/opencv_zoo)
  (for hand gestures), and [GoogleTest](https://github.com/google/googletest) 1.15.2.

## Building

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Visual Studio generators work too (`-G "Visual Studio 17 2022" -A x64`). Everything is placed in
`build/bin/`. Keep the executables and `gazeora_hook.dll` in the same folder, because the
launcher loads the hook from its own directory. The same goes for `onnxruntime.dll` and the
`models` folder, which hand gestures load from there.

| Output | Description |
|---|---|
| `Gazeora.exe` | The launcher (Dear ImGui on Direct3D 11) and stream server |
| `GazeoraCLI.exe` | Command-line front end for the same library |
| `gazeora_hook.dll` | The capture hook, injected into games |
| `gazeora_testapp.exe` | A test "game" that renders with each supported API |
| `gazeora_stream_probe.exe` | An RTSP client that measures the stream |
| `gazeora_*_tests.exe` | Unit tests (with `GAZEORA_BUILD_TESTS=ON`, the default) |
| `gazeora_bench.exe` | Benchmarks of every stage, from capture to RTP packets (with `GAZEORA_BUILD_BENCH=ON`, the default) |

## Getting started

Start `Gazeora.exe`, add games with **Add game** or by dragging exes or shortcuts onto the
window, and launch them with **Play with capture**. The game's session card shows the capture
status, frame rate, resolution, graphics API and the stream address.

Then open the stream address from **Settings → Frame stream** on a phone or another PC;
see [Streaming](docs/streaming.md).

## Documentation

| Guide | Covers |
|---|---|
| [Streaming](docs/streaming.md) | Watching the stream, protocols, USB setup, overlay, codecs, Wi-Fi |
| [VR viewer mode](docs/vr-viewer.md) | Lens correction and 3D from depth |
| [Hand gestures](docs/hand-gestures.md) | Controlling the pointer with a webcam |
| [Command line](docs/command-line.md) | `GazeoraCLI` commands |
| [Tools](docs/tools.md) | The stream probe and the test app |
| [Configuration files](docs/configuration.md) | What Gazeora stores and where |
| [Stream format](docs/stream-format.md) | The wire formats of every protocol |
| [Development](docs/development.md) | Testing, benchmarks, project layout, contributing |

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
- The USB protocol needs the USB driver (see [USB setup](docs/streaming.md#usb-setup)), and a
  phone on a cable rather than an emulator. A phone without USB debugging loses Windows' file
  transfer on this PC while it's set up for streaming.
- The stream is unencrypted and unauthenticated. Anyone on the local network can watch it.
