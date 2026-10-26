# Development

Testing, benchmarks, the code layout and how to contribute.

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
| `gazeora_host_tests` | Capture client, injector (refusing a game of the other bitness), launcher, the test app captured end to end with the real hook (D3D9, and D3D8 in MinGW x86 builds), frame read-back, GPU scaling and the VR pass, NV12 conversion on the GPU, H.264 and H.265 encoding (software and hardware, from memory and from GPU textures), repeated frames, slicing, and RTP round trips through the system decoders, the RTSP server |
| `gazeora_transport_tests` | The real hook and host exchanging frames in one process |
| `gazeora_library_tests` | The game library shared by both front ends |
| `gazeora_installer_tests` | The MSI's custom action that deletes the user's data on uninstall, against a fake installer session |
| `gazeora_input_tests` | Game controllers: steadying the sticks, the polling session, and SDL starting without a window |
| `gazeora_app_tests` | The launcher's game controller support: ImGui navigation driven by controller states on a headless ImGui (across the sidebar and the page, pressing, the stick's hysteresis), the queue to the UI thread, and the switch in `controls.ini` |

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
| `Encode` | Each codec on the hardware and the software encoder: time per call, input-to-output latency, frame sizes, open time; the whole `VideoPipeline`; a 1080p game frame to packets, read back or kept on the GPU (time and CPU per frame) |
| `Rtp`, `Rtcp`, `Rtsp`, `Bitrate` | Packetizing and depacketizing both codecs, parameter sets, RTCP reports, RTSP messages, the bitrate controller |
| `Stream` | The `FrameMailbox` hand-off, `PacketSendQueue` pacing against its `PacingPlan`, and the whole server at 30 and 60 fps: `PushFrame` to a loopback UDP player receiving the frame |
| `Library` | Loading, saving and searching a 500-game library |

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
  input/                Game controllers
    gamepad/              Steadying a controller's sticks
    services/             The controller polling and its session
    platform/             SDL controllers
apps/
  gazeora/              Gazeora.exe
    src/                  App, with pages/ (one file per page), ui/ (theme, widgets),
                          platform/ (shell dialogs, the USB driver, games opened from
                          Explorer), gamepad/ (controllers to ImGui navigation) and
                          settings/ (stream.ini, controls.ini)
    resources/            Icon and resource script
  cli/                  GazeoraCLI.exe
tools/                The test app and the stream probe
tests/                GoogleTest suites, in the same folders as the code they test
installer/            The MSI: WiX sources and the custom-action DLL (see installer.md)
bench/                gazeora_bench, the benchmarks
cmake/                Compiler settings, dependency fetching, formatting targets and the install
                      rules (InstallPrefix.cmake, Install.cmake)
```

### x64 and x86 releases

The hook runs inside the game, so it has the game's bitness, and so does the program that
injects it: `CreateRemoteThread` + `LoadLibraryW` only works between processes of the same
bitness. So Gazeora is built twice, as two releases: x64 for 64-bit games and x86 for 32-bit
ones, each the whole of Gazeora. The compiler picks which (`GAZEORA_ARCH`, in
`CompilerSettings.cmake`), and so does what depends on it: the default install folder. `InjectDll` refuses a game of the other bitness with a message naming
the release that captures it.

To test both on one PC, build both: the tests and the test app of each run as that release.
With MSVC, run `vcvarsall.bat x64` or `x86` first; with MinGW, use a 64-bit or a 32-bit
toolchain (MSYS2's `mingw-w64-i686-gcc` for x86). Code that hands Win32 a callback must give it
the callback calling convention (`CALLBACK` / `WINAPI`, `__stdcall` on x86): a lambda only
converts to a `__cdecl` function pointer there, so callbacks are named functions.

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
| `PacingPlan`, `PacketSendQueue` | How to spread a frame's UDP packets (at 4x the bitrate, within half a frame interval), and the thread that sends them so, off the encode thread |
| `rtcp`, `net`, `StreamLog` | RTCP reports, sockets, the stream log |

RTSP methods are dispatched through a table in `frame_stream.cpp`; supporting another method
is one handler and one entry.

## Releasing

The version lives in one place: the `VERSION` file at the root, as `MAJOR.MINOR.PATCH`
(`1.1.0`), or `MAJOR.MINOR.PATCH-rc.N` for a release candidate (`1.1.0-rc.1`). It is always
shown with a `v` in front: `v1.1.0`, `v1.1.0-rc.1`. From there it reaches the version info of
`Gazeora.exe`, the About card on the Settings page and the MSI's file name. Windows Installer
compares only the three numbers, so `v1.1.0-rc.1` and `v1.1.0` install as the same version and
either replaces the other. To release:

1. Change `VERSION`, for example to `1.1.0-rc.1`, and commit it (`chore: release v1.1.0-rc.1`).
   The next build picks it up; there's no need to configure again.
2. Publish a GitHub release titled `Gazeora v1.1.0-rc.1` with a new tag `v1.1.0-rc.1`. A
   release candidate is published as a **pre-release**, a stable version as a normal release.
3. CI (`.github/workflows/ci.yml`) builds and tests both releases and attaches
   `Gazeora-v1.1.0-rc.1-x64.msi` and `Gazeora-v1.1.0-rc.1-x86.msi` to the release. It fails
   if the tag doesn't match `VERSION`, or if a release candidate isn't a pre-release (or a
   stable version is). To build the MSIs locally instead, run `--target gazeora_msi` in an x64
   and an x86 build folder.
4. CI then sends the release to [gazeora.github.io](https://gazeora.github.io), which lists it
   on its download page. Editing or deleting the release later updates the site too
   (`.github/workflows/website.yml`). Both need the `GAZEORA_SITE_TOKEN` secret: a fine-grained
   token for the Gazeora organization with access to `Gazeora/gazeora.github.io` only and
   **Contents: Read and write**.

When a candidate is good, set `VERSION` to `1.1.0` and release `v1.1.0` the same way.

CI also runs on every pull request, every Monday at 00:00 UTC, and by hand from the Actions tab
(`workflow_dispatch`); the MSIs of each run are kept as workflow artifacts for 7 days (90 for a
release run). The MSIs attached to a release never expire.

A newer MSI replaces the installed version, keeping the user's library and settings.

## Contributing

How to report problems, the code rules and the pull request steps are in
[CONTRIBUTING.md](../CONTRIBUTING.md).

The code is formatted with clang-format using the `.clang-format` file at the root. Run
`cmake --build build --target format` to format everything, or `--target format-check` to
check. The [pre-commit](https://pre-commit.com) configuration runs the same checks before each
commit:

```
pip install pre-commit
pre-commit install
```
