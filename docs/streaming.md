# Streaming

How to watch the stream, pick a protocol and codec, and keep it smooth on Wi-Fi.

## Watching the stream

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

## Choosing a protocol

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

### USB setup

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

## Stream overlay

**Settings → Stream overlay** draws streaming details into the frames themselves, so every
player shows them, Gazeora's app or VLC alike: the protocol and viewers; codec, resolution and
frame rate; bitrate and packet loss; the encoder; this PC's clock (handy for judging delay:
photograph the PC's clock and the phone together); and a line of your own text. Pick the
details, the corner, the text size and how dark the panel behind it is; changes show in the
stream at once. In VR mode the panel is drawn into each eye, away from the lens edges. The
choices are saved in `stream.ini` under `[overlay]`. It's off until switched on. The overlay
is drawn on the CPU, so while it's on, frames are read back from the GPU instead of going
straight to its encoder: a few milliseconds more per frame and some CPU time.

## Choosing a codec

**Settings → Frame stream → Video codec** switches between H.264 and H.265. The choice is saved
in `stream.ini` (`[stream] codec=h264` or `h265`). Changing it disconnects anyone watching, as a
player can't switch codecs mid-stream; open the stream again.

**Stream quality** next to it sets the bitrate at full quality: more bits keep fine detail and
motion sharp. Balanced (10 Mbit/s) suits weak or busy Wi-Fi, High (20) good Wi-Fi, Very high
(35) 5 GHz Wi-Fi close to the router, or USB. It's saved as `[stream] max_bitrate_mbps` and
applies at once.

**Frame rate** next to it picks 60 fps (the default) or 30. At 60 the next frame is never more
than 17 ms away and motion is smoother; from the same stream quality each frame gets half the
bits, so pick a higher quality with it if the Wi-Fi allows. 30 puts the most detail in each
frame. It's saved as `[stream] fps` and applies at once: the encoder starts over with a key
frame. Games drawing fewer frames have their last one sent again.

| | H.264 | H.265 |
|---|---|---|
| Bitrate at full quality | The stream quality picked: 10, 20 (default) or 35 Mbit/s | 70% of it: 7, 14 or 24.5 Mbit/s |
| Picture | Reference | About the same at 70% of the bits (measured: 47.6 dB PSNR at 5.6 Mbit/s against 47.1 dB for H.264 at 8, on panning detailed content with NVENC) |
| Players | Every device | VLC on PC, iOS and Android; iPhones from the 6s and most recent Android phones decode it in hardware |
| Encoder | Always available | Needs a GPU encoder (NVIDIA GTX 950 or newer, AMD RX 400 series or newer, Intel 6th gen or newer) or an installed software one |

Both are cut into about 24 slices per frame. Which one copes better with Wi-Fi loss depends on
the link: H.265 sends fewer packets, so fewer frames get hit, but its loss concealment in VLC may
be weaker than H.264's. Try both with the phone you use.

## Wi-Fi and packet loss

UDP doesn't resend lost packets. Gazeora is built so that a lost packet costs as little as
possible:

- **Small slices.** Each frame is cut into about 24 slices of whole macroblock rows, mostly a
  packet or a few each. A lost packet damages one strip, which the player fills in from the
  previous frame; the rest of the frame decodes normally. (FFmpeg, which VLC decodes with,
  ignores slices beyond the 32nd, so there are never more.)
- **Key frames every second.** A key frame every second repairs whatever loss has smeared.
- **Paced sending.** Each frame's packets go out in bursts a millisecond apart instead of all at
  once, so Wi-Fi access points and phones don't overflow: at four times the bitrate, and within
  half a frame interval, so a frame is never still going out when the next is ready. A thread
  of its own sends them while the next frame encodes.
- **Adaptive bitrate.** Players report their packet loss in RTCP receiver reports. While a player
  reports 2% loss or more, the bitrate steps down from 8 to 6, 4.5 and 3 Mbit/s. Once the reports
  have been clean for 8 seconds it climbs back a step every 3 seconds. One player with a bad link
  lowers the bitrate for everyone. (VLC's first report claims about 98% loss with a negative
  total; reports with a negative total count as no loss.)

The session card and **Settings → Frame stream** show a *Weak network* warning with the current
loss and bitrate while this is happening. A 5 GHz Wi-Fi network, and the PC on a cable rather than
Wi-Fi (otherwise every packet crosses the air twice), make the biggest difference. To try a bad
link on a good one, run the stream probe with `--drop PERCENT`.

### RTSP over TCP

If a phone's picture keeps smearing or stopping, set the player to RTSP over TCP (VLC for iOS:
the **RTSP over TCP** switch in Settings; desktop VLC: `--rtsp-tcp`; other players: their own
RTSP transport setting). RTP then travels inside the RTSP connection and nothing is lost. A link too slow
for the bitrate makes the stream skip frames rather than smear: while more than 512 KB is still
queued for the player, frames are skipped up to the next key frame, and the skipped share
counts as loss, so the bitrate steps down. TCP can add delay when Wi-Fi retransmits, so a
little more network caching helps.

### Stream log

`%APPDATA%\Gazeora\stream.log` records what every player did, rewritten at each start: connects
and disconnects (with the reason), the transport each chose, every receiver report (loss,
jitter), bitrate changes, skipped frames, and UDP players that stopped sending reports, which
usually means they stopped receiving. When a stream stalls on one device, this shows whether
the player went quiet, the connection closed, or the link got worse.
