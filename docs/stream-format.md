# Stream format

| Property | Value |
|---|---|
| Control | RTSP 1.0 over TCP, port 8554 (falls back to a free port) |
| Transport | RTP/AVP over UDP unicast, or interleaved in the RTSP connection (RTP/AVP/TCP) when the player asks; multicast is refused |
| Payload | H.264 High profile (RFC 6184: payload type 96, packetization mode 1, FU-A for large NAL units), or H.265 Main (RFC 7798: payload type 96, FU for large NAL units); parameter sets (H.265: VPS, SPS, PPS) in the SDP and before every key frame |
| Encoding | Low-latency CBR, no B-frames, about 24 slices per frame, a key frame every second |
| Bitrate | The stream quality (H.264: 20 Mbit/s by default; H.265: 70% of it), stepping down to 75%, 55%, 40% and 28% of it while players report loss or TCP players fall behind |
| Size | Up to 1920×1080, in the game's shape; when the PC does the VR lens correction, the phone's screen shape within that size |
| View info | When the phone does the VR lens correction, every frame carries an SEI message (user data unregistered, payload type 5) just before its slices: the 16-byte id `GazeoraViewInfo1`, then text such as `vr=1 pair=1 k1=0.340 k2=0.550 scale=0.700 sep=0.500 chroma=0.000`. `pair=1` means the frame holds two views side by side, the left eye's on the left, each squeezed to half the width. Frames of Gazeora's own window end with ` src=app`: the app lets the viewer's hand point at them, with a cursor over the picture. Other players skip it |
| Pacing | UDP: a frame's packets go out in bursts a millisecond apart, at least 4 a burst, at 4 times the bitrate and within half a frame interval (a 1080p frame at 20 Mbit/s in about 10 ms, a key frame in 16 ms at 30 fps or 8 at 60), from a thread of their own while the next frame encodes. TCP: frames skipped up to the next key frame while 512 KB is queued |
| Frame rate | A steady 60 fps, or 30 (`[stream] fps`). Games running slower have their last frame repeated, and Gazeora's window, streamed while no game is, is sent at the same rate, because VLC drops frames as late when a stream runs much slower |
| Timing | RTCP sender reports every second; RTP timestamps mark capture time; receiver reports drive the bitrate |
| Key frame requests | An RTCP Picture Loss Indication or Full Intra Request (RFC 4585, RFC 5104) from a player gets a key frame at once. The Android app sends one when packets are lost for good, and shows nothing (rather than a smeared picture) until the key frame comes |

The other protocols carry the same encoded frames:

| Protocol | Format |
|---|---|
| HTTP | `GET /stream.ts` answers with an endless MPEG-TS (ISO/IEC 13818-1): one program, video on PID 0x100 (stream type 0x1B or 0x24), the PAT and PMT before every key frame, one PES per frame with an access unit delimiter and a PTS 100 ms after its PCR |
| UDP, USB | GZF1 messages: a 24-byte big-endian header (`"GZF1"`, type, key frame and H.265 flags, width, height, the player's loss, frame number, 90 kHz timestamp, then the chunk index and count over UDP or the payload length over USB), then the frame's Annex B access unit with its parameter sets in front of a key frame. Over UDP a frame is cut into datagrams of at most 1400 bytes. Players say hello once a second (UDP players silent for 5 s are dropped), ask for a key frame when they start or lose a frame, and say bye. `libs/host/include/gazeora/host/stream/frame_packet.h` has the details |
| Discovery | UDP port 8556: `GAZEORA-DISCOVER 1` gets back one line of JSON, e.g. `{"service":"gazeora","version":1,"name":"MY-PC","protocol":"http","port":8554,"path":"/stream.ts","codec":"h264"}` |
