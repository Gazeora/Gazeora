# Stream format

The wire formats Gazeora uses to stream frames and receive input. This document is the
canonical description of the protocol between `Gazeora.exe` and the
[Gazeora Android app](https://github.com/Gazeora/Gazeora-Android). The source of truth is the
code in [`libs/host/include/gazeora/host/stream/`](../libs/host/include/gazeora/host/stream);
the Android app keeps its own copies of these layouts, which must stay in step.

For choosing a protocol and its settings, see [Streaming](streaming.md).

## Ports

| Port | Transport | Purpose |
|---|---|---|
| 8554 | TCP and UDP | The stream, whatever the protocol. Falls back to any free port if 8554 is taken |
| 8556 | UDP | [Discovery](#discovery) |
| 8557 | UDP | [Input messages](#input-messages) from the app, for every network protocol |

## Encoded video

The same encoded frames are carried by every protocol.

| Property | Value |
|---|---|
| Codecs | H.264 High profile, or H.265 Main |
| Bitstream | Annex B access units. Parameter sets (H.265: VPS, SPS, PPS; H.264: SPS, PPS) come before every key frame |
| Encoding | Media Foundation, low-latency constant bitrate, no B-frames |
| Slices | About 24 per frame, each of whole rows |
| Key frames | One per second (an interval of the frame rate's worth of frames), and on request |
| Frame rate | Steady, 60 fps by default (configurable 10 to 120). A game drawing fewer frames has its last frame repeated |
| Size | At most 1920×1080, in the game's aspect ratio. When the PC does the VR lens correction, the phone screen's aspect ratio within that size |
| Bitrate | The stream quality for H.264 (20 Mbit/s by default), 70% of it for H.265, reduced to 75%, 55%, 40% or 28% while players report loss |

### View info

When VR mode is on and the phone does the lens correction, every frame carries an SEI message
(user data unregistered, payload type 5) before its slices. The payload is the 16-byte ID
`GazeoraViewInfo1` (no terminator), then ASCII text such as:

```text
vr=1 pair=1 k1=0.340 k2=0.550 scale=0.700 sep=0.500 chroma=0.000 src=app
```

| Field | Meaning |
|---|---|
| `vr` | `1`: lay the frame out for a VR viewer |
| `pair` | `1`: the frame holds two views side by side (left eye on the left), each squeezed to half the width. `0`: one view for both eyes |
| `k1`, `k2` | Lens distortion coefficients (see [VR viewer mode](vr-viewer.md)) |
| `scale` | Picture size in each eye |
| `sep` | Distance between lens centers, as a share of the screen width |
| `chroma` | Color-fringe correction |
| `src=app` | Present only on frames of Gazeora's own window. The app then allows the hand pointer on it |

Players other than the Gazeora app ignore this message.

## RTSP and RTP

Used by the `rtsp-tcp` and `rtsp-udp` protocols.

| Property | Value |
|---|---|
| Control | RTSP 1.0 over TCP, URL `rtsp://PC:8554/` |
| Transport | `rtsp-tcp`: RTP/AVP/TCP, interleaved in the RTSP connection. `rtsp-udp`: RTP/AVP over UDP unicast. Multicast is refused |
| Payload | H.264 per [RFC 6184](https://www.rfc-editor.org/rfc/rfc6184) (payload type 96, `packetization-mode=1`, FU-A for large NAL units). H.265 per [RFC 7798](https://www.rfc-editor.org/rfc/rfc7798) (payload type 96, FU for large NAL units) |
| Parameter sets | In the SDP, and in-band before every key frame |
| Packet size | RTP packets of at most 1400 bytes |
| Timestamps | 90 kHz RTP timestamps marking capture time |
| RTCP | Sender reports every second ([RFC 3550](https://www.rfc-editor.org/rfc/rfc3550)). Receiver reports drive the [adaptive bitrate](streaming.md#wi-fi-and-packet-loss) |
| Key frame requests | An RTCP Picture Loss Indication or Full Intra Request ([RFC 4585](https://www.rfc-editor.org/rfc/rfc4585), [RFC 5104](https://www.rfc-editor.org/rfc/rfc5104)) gets a key frame immediately |
| Session timeout | 60 seconds without a keepalive |
| Connections | At most 16 RTSP connections at once |
| UDP pacing | A frame's packets go out in bursts 1 ms apart, from a separate thread |
| TCP backlog | While more than 512 KB is queued for a player, frames are skipped up to the next key frame |

## HTTP (MPEG-TS)

Used by the `http` protocol. `GET /stream.ts` (or `GET /`) answers with an endless MPEG
transport stream (ISO/IEC 13818-1):

- One program, video on PID `0x100`, stream type `0x1B` (H.264) or `0x24` (H.265).
- The PAT and PMT before every key frame.
- One PES packet per frame, with an access unit delimiter, and a PTS 100 ms after its PCR.
- Other paths get `404 Not Found`.
- While more than 512 KB is queued for a player, frames are skipped.

## GZF1 messages

Used by the `udp` and `usb` protocols for frames, and by the app for input over every
protocol. Defined in
[`frame_packet.h`](../libs/host/include/gazeora/host/stream/frame_packet.h).

### Header

Every message starts with a 24-byte header. All multi-byte fields are big-endian.

| Offset | Type | Field |
|---|---|---|
| 0 | 4 bytes | Magic `"GZF1"` |
| 4 | u8 | Message type (see below) |
| 5 | u8 | Flags: bit 0 key frame, bit 1 H.265 (else H.264) |
| 6 | u16 | Width |
| 8 | u16 | Height |
| 10 | u16 | In a Hello: the share of frames the player lost recently, per mille. Otherwise 0 |
| 12 | u32 | Frame number, counting up from the first frame sent |
| 16 | u32 | In a Frame: timestamp, 90 kHz. In a Challenge, and in the player's messages over UDP: the cookie |
| 20 | u32 | UDP frames: chunk index (high 16 bits) and chunk count (low 16 bits). USB and input messages: payload length |

### Message types

| Value | Name | Direction | Payload |
|---|---|---|---|
| 1 | Frame | PC → player | The frame's Annex B access unit |
| 2 | Hello | player → PC | None |
| 3 | KeyFrameRequest | player → PC | None |
| 4 | Bye | player → PC | None |
| 5 | Pointer | player → PC | [Pointer payload](#pointer-type-5) |
| 6 | Challenge | PC → player, UDP only | None |
| 7 | HeadPose | player → PC | [Head pose payload](#headpose-type-7) |
| 8 | CameraStatus | player → PC | [Camera status payload](#camerastatus-type-8) |

### UDP session

1. The player sends a Hello to port 8554.
2. The PC answers with a Challenge: a header the size of the Hello, carrying a random cookie for
   the player's address.
3. The player includes the cookie in the cookie field of every later message. Messages without
   the right cookie are ignored. This stops a forged Hello from directing the stream to another
   address.
4. The player sends a Hello once a second. The PC drops a player that is silent for 5 seconds.
5. The player sends a KeyFrameRequest when it starts or loses a frame, and a Bye when it stops.

Each Frame is cut into chunks, one per datagram of at most 1400 bytes, sent in order. Each
datagram is a header with the chunk index and count, followed by its piece of the access unit.

### USB session

The PC switches the phone to Android Open Accessory mode and exchanges a byte stream over the
accessory's bulk endpoints. Each message is a header whose last field is the payload length,
followed by the payload. There is no challenge over USB.

## Input messages

The app sends input as GZF1 messages: over USB on the same cable, and otherwise to UDP port
8557 whatever the stream protocol. Over the network, input is accepted only from an address
that is currently watching the stream. Each message is a header (payload length in the last
field) followed by the payload below.

### Pointer (type 5)

A touch on the flat picture, or the hand pointer in VR, as a mouse action on Gazeora's window.
Defined in [`pointer_message.h`](../libs/host/include/gazeora/host/stream/pointer_message.h).
10 bytes, big-endian:

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | Action: 1 Move, 2 Press, 3 Release, 4 Scroll, 5 Click |
| 1 | u8 | Button: 0 Left, 1 Right |
| 2 | u16 | x across the picture, 0 to 65535 from the left |
| 4 | u16 | y down the picture, 0 to 65535 from the top |
| 6 | i16 | Vertical wheel, Windows units (120 per notch, positive scrolls up) |
| 8 | i16 | Horizontal wheel, Windows units (positive scrolls right) |

Messages with an unknown action or button are ignored.

### HeadPose (type 7)

Where the viewer's head points. Defined in
[`head_pose_message.h`](../libs/host/include/gazeora/host/stream/head_pose_message.h) and
[`head_pose.h`](../libs/host/include/gazeora/host/stream/head_pose.h). 12 bytes, big-endian:

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | Sample number, increasing with every pose, so late arrivals can be discarded |
| 4 | i16 × 4 | Quaternion x, y, z, w, each −1 to 1 scaled to −32767 to 32767 |

The quaternion is a unit rotation from head axes to world axes. The world has Y up and an
arbitrary heading; only changes in heading are used. The head looks down its −Z axis, with X to
its right and Y up, as in OpenGL's camera convention. Messages whose quaternion is far from unit
length are ignored.

### CameraStatus (type 8)

The state of the phone's back camera used for hand tracking. The app sends it when it changes
and every second. Defined in
[`camera_status_message.h`](../libs/host/include/gazeora/host/stream/camera_status_message.h)
and [`camera_report.h`](../libs/host/include/gazeora/host/stream/camera_report.h). 2 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | State: 0 Off, 1 Starting, 2 On, 3 Dark (off to save power), 4 Checking (briefly on to test the light), 5 Unavailable (no permission, in use, or no camera) |
| 1 | u8 | Brightness: the camera picture's mean luma, 0 to 255 |

Messages with an unknown state are ignored.

## Discovery

The app sends the ASCII text `GAZEORA-DISCOVER 1` in a UDP datagram to port 8556. The PC
replies to the sender with one line of JSON:

```json
{"service":"gazeora","version":1,"name":"MY-PC","protocol":"rtsp-tcp","port":8554,"path":"/","codec":"h264"}
```

| Field | Meaning |
|---|---|
| `service` | Always `gazeora` |
| `version` | Discovery format version, currently `1` |
| `name` | The PC's name |
| `protocol` | The current protocol ID: `rtsp-tcp`, `rtsp-udp`, `udp`, `http` or `usb` |
| `port` | The stream port, `0` for USB |
| `path` | The stream path, such as `/` for RTSP or `/stream.ts` for HTTP |
| `codec` | `h264` or `h265` |

The address to connect to is the address the reply came from.

## Security

No protocol is encrypted or authenticated. Anyone on the local network can watch the stream,
and anyone watching can send input messages. The UDP challenge only prevents a forged Hello from
sending the stream to an address that did not ask for it.
