# Tools

Two developer tools are built with Gazeora. The MSI installs them only with the
**Developer tools** feature, and `cmake --install` only with `--component Tools`.

## Stream probe

`gazeora_stream_probe.exe` is a minimal RTSP player for measuring the stream without VLC. It
works with the `rtsp-tcp` and `rtsp-udp` protocols only.

```text
gazeora_stream_probe.exe [rtsp://host:port/] [--seconds N] [--save stream.h264|stream.h265]
                         [--drop PERCENT] [--tcp]
```

| Option | Effect |
|---|---|
| URL | The stream to play. Default `rtsp://127.0.0.1:8554/` |
| `--seconds N` | How long to play |
| `--save FILE` | Writes everything received as a raw H.264 or H.265 stream, which VLC and ffplay can play |
| `--drop PERCENT` | Discards that share of arriving packets, to simulate a weak Wi-Fi link |
| `--tcp` | Asks for RTP interleaved in the RTSP connection, like VLC's RTSP-over-TCP option |

It reports frame rate, bitrate, packet loss, frames damaged by loss, key frames, gaps between
frames, and capture-to-arrival latency. Like VLC, it sends RTCP receiver reports, so the server
adapts its bitrate to the loss the probe sees.

Latency is computed from the RTCP sender reports against the local clock. It is exact only when
the probe runs on the streaming PC. On another machine it is off by the difference between the
two clocks.

## Test app

`gazeora_testapp.exe` opens a 1280×720 window with a moving square, for testing capture without
a real game. It uses Direct3D 11 by default.

| Option | Graphics API |
|---|---|
| (none) | Direct3D 11 |
| `--d3d8` | Direct3D 8. Only in builds whose compiler ships `d3d8.h` (MinGW; the Windows SDK no longer does), and only runs in the x86 release |
| `--d3d9` | Direct3D 9 |
| `--d3d10` | Direct3D 10 |
| `--d3d12` | Direct3D 12 |
| `--d3d12-depth-write` | Direct3D 12, keeping the depth buffer in the depth-write state |
| `--opengl` | OpenGL |

In Direct3D 11 and 12 it also draws a depth-tested 3D scene for testing
[3D from depth](vr-viewer.md#3d-from-depth): Direct3D 11 with 24-bit depth (0 near), Direct3D
12 with reversed 32-bit float depth that moves to a shader-readable state after drawing.

The 3D scene's camera turns with the mouse while the window is in front, using raw input at
0.05° per count. At head tracking's default sensitivity of 20 counts per degree, it turns
exactly as far as your head. The window title shows where the camera looks. To check head
tracking end to end, add the test app to the library, play it from Gazeora, and put the phone
in the viewer.
