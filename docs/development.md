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
| `gazeora_host_tests` | Capture client, injector, launcher, frame read-back, GPU scaling and the VR pass, NV12 conversion on the GPU, H.264 and H.265 encoding (software and hardware, from memory and from GPU textures), repeated frames, slicing, and RTP round trips through the system decoders, the RTSP server |
| `gazeora_transport_tests` | The real hook and host exchanging frames in one process |
| `gazeora_library_tests` | The game library shared by both front ends |
| `gazeora_input_tests` | Hand gestures: crops and anchors, finger poses and clicks, the air touch pointer and smoothing, synthetic video (photos moved, turned and tipped) through the real models, the camera-to-pointer loop, and the real models on photos in `tests/input/data` (needs the build's `onnxruntime.dll` and `models`) |

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
  input/                Hand gestures: webcam, hand tracking, gestures, the pointer
    tracking/             MediaPipe's palm detector and hand landmark model, and following a hand
    gestures/             Finger aim, hand poses to steer, hold and click (a state machine), smoothing
    services/             The camera-to-pointer loop and its background session
    platform/             Media Foundation cameras, ONNX Runtime, SendInput
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
| `PacingPlan`, `PacketSendQueue` | How to spread a frame's UDP packets (at 4x the bitrate, within half a frame interval), and the thread that sends them so, off the encode thread |
| `rtcp`, `net`, `StreamLog` | RTCP reports, sockets, the stream log |

RTSP methods are dispatched through a table in `frame_stream.cpp`; supporting another method
is one handler and one entry.

## Contributing

The code is formatted with clang-format using the `.clang-format` file at the root. Run
`cmake --build build --target format` to format everything, or `--target format-check` to
check. The [pre-commit](https://pre-commit.com) configuration runs the same checks before each
commit:

```
pip install pre-commit
pre-commit install
```
