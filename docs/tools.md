# Tools

## Stream probe

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

## Test app

`gazeora_testapp.exe` opens a 1280×720 window with a moving square, for testing capture without
a real game. It uses Direct3D 11 by default; pass `--d3d8` (MinGW builds only), `--d3d9`,
`--d3d10`, `--d3d12` or `--opengl` for the other APIs. In D3D11 and D3D12 it also draws a
depth-tested 3D scene, for testing 3D from depth: D3D11 with 24-bit depth (0 near), D3D12 with
reversed 32-bit float depth that goes to a shader-readable state after drawing, like a deferred
renderer. `--d3d12-depth-write` keeps it in the depth write state instead.
