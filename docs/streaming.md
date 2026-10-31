# Streaming

How to watch the stream, choose a protocol and codec, and keep the stream stable on Wi-Fi.
The byte-level formats are in [Stream format](stream-format.md).

## Watching the stream

The stream is always on while `Gazeora.exe` runs. While a game is captured it carries the
game. Otherwise, it carries Gazeora's own window.

**Settings → Frame stream** shows the stream address, one per network address of the PC, for
example:

```text
rtsp://192.168.1.20:8554/
```

Gazeora listens on port 8554. If that port is taken, it uses any free port, and the address
shown changes to match.

Open the address in a player on a device on the same network. The Gazeora Android app does not
need the address: it finds the PC through [discovery](#discovery).

Windows Firewall must allow `Gazeora.exe` on the network you use. The MSI adds a rule for
private networks only; see [Installer](installer.md#what-it-installs).

### Player delay

VLC buffers about one second of a network stream by default. To reduce the delay, lower VLC's
**Network caching** setting. Gazeora's own hint suggests 0 to 100 ms on a wired network and
about 300 ms on Wi-Fi. Lower values stall more often on an unstable link.

## Choosing a protocol

**Settings → Frame stream → Protocol** picks the protocol. One protocol runs at a time.
Changing it restarts the stream and disconnects everyone watching. The choice is saved as
`[stream] protocol` in [`stream.ini`](configuration.md#streamini).

| Protocol | `protocol=` | Address | Players | Behavior |
|---|---|---|---|---|
| RTSP over TCP (default) | `rtsp-tcp` | `rtsp://PC:8554/` | Gazeora app, VLC, ffplay, OBS | RTP interleaved in the RTSP connection. Nothing is lost; a slow link skips frames. Players that ask for UDP get `461 Unsupported Transport` |
| RTSP over UDP | `rtsp-udp` | `rtsp://PC:8554/` | Gazeora app, VLC, ffplay, OBS | RTP over UDP unicast. Lowest overhead; a lost packet damages part of the picture until the next key frame. Does not reach an Android emulator |
| UDP | `udp` | `udp://PC:8554` | Gazeora app | GZF1 frames in UDP datagrams, no RTSP or RTP. A lost packet drops its frame |
| HTTP | `http` | `http://PC:8554/stream.ts` | Gazeora app, VLC, ffplay | A live MPEG-TS stream. Nothing is lost; a slow link skips frames |
| USB | `usb` | the USB cable | Gazeora app | GZF1 frames over Android Open Accessory (AOA). See [USB setup](#usb-setup) |

Multicast is not supported.

### Discovery

Whatever the protocol, Gazeora answers the Android app's discovery request on UDP port 8556
with the PC's name, protocol, port, path and codec. See
[Discovery](stream-format.md#discovery). Some Wi-Fi networks drop broadcasts; the app also
asks each address on small networks. See the Android app's
[user guide](https://github.com/Gazeora/Gazeora-Android/blob/master/docs/user-guide.md#found-pcs).

### USB setup

The USB protocol needs a physical Android phone; it does not work with an emulator.

1. **Install the USB driver once per PC.** Set the protocol to USB, plug in the phone, and
   click **Settings → Frame stream → Install USB driver**. Windows asks for administrator
   permission. This installs Windows' WinUSB driver for the *Android Accessory Interface*.
2. **With USB debugging on the phone,** Gazeora switches the phone to accessory mode through
   its adb interface. File transfer (MTP) keeps working.
3. **Without USB debugging,** the button names the phone model. The driver is then installed
   for that phone model itself, so that Gazeora can switch it to accessory mode. **Windows can
   no longer transfer files with that phone on this PC** until you click
   **Restore file transfer**. After restoring, those phones need USB debugging to stream over
   USB.
4. Plug in the phone. It switches to accessory mode, which opens the Gazeora app, and stays in
   accessory mode until it is unplugged.

The driver package is signed with a certificate that Gazeora creates and adds to the PC's
**Trusted Root Certification Authorities** and **Trusted Publishers** stores, named
*Gazeora USB accessory driver*. The certificate's private key is deleted after signing, so it
cannot sign anything else. Uninstalling Gazeora with the MSI removes the driver package and the
certificate (see [Uninstalling](installer.md#uninstalling)). Each driver install replaces the
previous package and keeps the phone models added before.

adb keeps working while the phone streams, because Gazeora opens only the accessory interface.
However, while adb holds the phone in normal mode, Gazeora cannot switch it to accessory mode.
Settings says so when this is the cause; run `adb kill-server` briefly to release it.

## Choosing a codec

**Settings → Frame stream → Video codec** switches between H.264 and H.265. The choice is saved
as `[stream] codec`. Changing it disconnects everyone watching, because a player cannot switch
codecs mid-stream.

| | H.264 | H.265 |
|---|---|---|
| Profile | High | Main |
| Bitrate at full quality | The stream quality | 70% of the stream quality |
| Encoder | GPU encoder through Media Foundation, or Microsoft's software encoder | GPU encoder, or an installed software encoder. Shown as *not supported on this PC* if neither exists |
| Players | Widely supported | Needs an H.265 decoder on the player's device |

Gazeora gives H.265 70% of the H.264 bitrate on the assumption that it reaches similar picture
quality with fewer bits. This depends on the encoder and content and is not a measured
guarantee. Fewer bits also means fewer packets that can be lost on Wi-Fi.

### Stream quality

**Stream quality** sets the H.264 bitrate at full quality:

| Choice | Bitrate |
|---|---|
| Balanced | 10 Mbit/s |
| High (default) | 20 Mbit/s |
| Very high | 35 Mbit/s |

It is saved as `[stream] max_bitrate_mbps` and applies immediately. Other values from 2 to 100
can be set in `stream.ini`; the setting then shows as *Custom*.

### Frame rate

**Frame rate** picks 60 fps (default) or 30 fps. It is saved as `[stream] fps` and applies
immediately; the encoder restarts with a key frame. At the same stream quality, 60 fps gives
each frame half the bits of 30 fps. Other values from 10 to 120 can be set in `stream.ini`.

The stream runs at a steady frame rate. When a game draws fewer frames, its last frame is sent
again. Gazeora's own window is also sent at the full rate.

## Wi-Fi and packet loss

UDP does not resend lost packets. Gazeora limits the damage a lost packet causes:

- **Slices.** Each frame is cut into about 24 slices of whole macroblock rows. A lost packet
  damages one strip, and the rest of the frame decodes.
- **Key frames.** A key frame every second repairs any damage. Players can also request one
  (see [Key frame requests](stream-format.md#rtsp-and-rtp)).
- **Paced sending (UDP).** A frame's packets go out in bursts 1 ms apart instead of all at
  once, from a separate thread while the next frame encodes.
- **Adaptive bitrate.** Loss comes from RTCP receiver reports (RTSP), from the players' hello
  messages (UDP), and from frames skipped for a full send queue (RTSP over TCP, HTTP and USB;
  see below). While a player reports 2% loss or more, the bitrate steps down every
  1.5 seconds through 75%, 55%, 40% and 28% of the stream quality. After 8 seconds without
  loss, it climbs back one step every 3 seconds. One player with a bad link lowers the bitrate
  for everyone. With no viewers, it returns to full quality.

While the bitrate is reduced, the session card and **Settings → Frame stream** show a
*Weak network* warning with the current loss and bitrate. A 5 GHz network and a wired PC
usually help most. To simulate a bad link, use the [stream probe](tools.md#stream-probe) with
`--drop PERCENT`.

### RTSP over TCP, HTTP and USB

With these protocols nothing is lost, but a slow link fills the send queue. While more than
512 KB is queued for a player, frames are skipped up to the next key frame. Skipped frames
count as loss, so the bitrate steps down. TCP retransmissions can add delay, so a slightly
larger player buffer helps.

If a player smears or stalls with RTSP over UDP, switch to RTSP over TCP.

## Stream overlay

**Settings → Stream overlay** draws stream details into the frames, so every player shows
them. This setting is only in builds configured with `GAZEORA_STREAM_OVERLAY=ON` (the default
for local builds). CI builds, including the released MSIs, are configured with it `OFF`, so
released versions do not have it.

**Show stream details on the picture** turns it on. It is off by default. **Details to show**:

| Detail | Shows | Default |
|---|---|---|
| Protocol | The stream protocol | On |
| Viewers | Number of players | On |
| Codec | H.264 or H.265 | On |
| Resolution | Stream size | On |
| Frame rate | Frames per second | On |
| Bitrate | Current bitrate | On |
| Packet loss | Worst reported loss | On |
| Encoder | Encoder name | Off |
| PC time | This PC's clock, to the second | Off |
| Head tracking | Head yaw and pitch in degrees, poses per second, and whether a game is being turned (while head tracking is on) | On |
| Phone camera | The hand-tracking camera's state and brightness, once the app reports it | On |

You can also add a line of custom text and choose the corner, text size (50 to 300%) and
background opacity (0 to 100%). Changes show in the stream immediately. In VR mode the panel is
drawn into each eye. Settings are saved under `[overlay]` in `stream.ini`.

The overlay is drawn on the CPU. While it is on, frames are read back from the GPU and encoded
from system memory instead of directly from GPU textures, which costs extra CPU time per frame.

## Stream log

`%APPDATA%\Gazeora\stream.log` records what each player did. It is rewritten each time
Gazeora starts. It contains connects and disconnects with reasons, the transport each player
chose, receiver reports (loss and jitter), bitrate changes, skipped frames, and UDP players that
stopped sending reports for 12 seconds. When a stream stalls on one device, the log shows
whether the player went quiet, the connection closed, or the link got worse.
