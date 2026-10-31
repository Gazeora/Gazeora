# Configuration files

Gazeora stores its data per user in `%APPDATA%\Gazeora`. The INI files are plain text and can be
edited by hand while Gazeora is closed; Gazeora reads them at startup and rewrites values when
you change them in the app.

| File | Contents | Written by |
|---|---|---|
| `library.json` | The game library | `Gazeora.exe`, `GazeoraCLI` |
| `stream.ini` | Stream, VR viewer and stream overlay settings | `Gazeora.exe` |
| `controls.ini` | Game controller and head tracking settings | `Gazeora.exe` |
| `stream.log` | What stream players did, rewritten at each start. See [Stream log](streaming.md#stream-log) | `Gazeora.exe` |

## Library location

Set the environment variable `GAZEORA_LIBRARY` to the full path of a library file to use it
instead of `%APPDATA%\Gazeora\library.json`. `stream.ini`, `controls.ini` and `stream.log` are
always kept in the same folder as the library file.

```powershell
$env:GAZEORA_LIBRARY = "D:\Games\gazeora-library.json"
GazeoraCLI list
```

The library format is internal to Gazeora and may change between versions. Edit it through
`Gazeora.exe` or `GazeoraCLI` rather than by hand.

## stream.ini

### `[stream]`

| Key | Values | Default | Effect |
|---|---|---|---|
| `protocol` | `rtsp-tcp`, `rtsp-udp`, `udp`, `http`, `usb` | `rtsp-tcp` | The stream protocol. Unknown values use the default. See [Choosing a protocol](streaming.md#choosing-a-protocol) |
| `codec` | `h264`, `h265` | `h264` | The video codec |
| `max_bitrate_mbps` | 2 to 100 (clamped) | 20 | H.264 bitrate at full quality, in Mbit/s. H.265 uses 70% of it |
| `fps` | 10 to 120 (clamped) | 60 | Stream frame rate. The app offers 30 and 60 |

### `[vr]`

Numbers are decimal. Switches are `1` (on) or `0` (off).

| Key | Default | Effect |
|---|---|---|
| `enabled` | `1` | VR viewer mode |
| `lens_on_phone` | `1` | `1`: the Gazeora app does the lens correction. `0`: this PC does it |
| `screen_aspect` | `2` | The phone screen's width divided by its height, in landscape. Used when the PC does the lens correction |
| `k1` | `0.34` | Lens distortion, second-order coefficient |
| `k2` | `0.55` | Lens distortion, fourth-order coefficient |
| `image_scale` | `0.7` | Picture height in each eye, relative to the eye's view |
| `lens_separation` | `0.5` | Distance between lens centers, as a share of the screen width |
| `chroma_correction` | `0` | Color-fringe correction |
| `stereo` | `0` | 3D from the game's depth |
| `stereo_amount` | `0.012` | 3D depth: how far views differ for distant things, as a share of an eye's width |
| `stereo_front` | `0.5` | Pop-out: how far near things may come forward, as a share of `stereo_amount` |

The defaults of `k1` and `k2` match the **Google Cardboard (2015)** preset. See
[VR viewer mode](vr-viewer.md) for what each setting does.

### `[overlay]`

Only used by builds with the stream overlay (see [Stream overlay](streaming.md#stream-overlay)).

| Key | Values | Default |
|---|---|---|
| `enabled` | `0`, `1` | `0` |
| `protocol`, `viewers`, `codec`, `resolution`, `frame_rate`, `bitrate`, `loss`, `head`, `camera` | `0`, `1` | `1` |
| `encoder`, `clock` | `0`, `1` | `0` |
| `corner` | `top_left`, `top_right`, `bottom_left`, `bottom_right` | `top_left` |
| `text_size` | 50 to 300 (percent, clamped) | `100` |
| `opacity` | 0 to 100 (percent, clamped) | `60` |
| `text` | Any single line, up to 511 characters | empty |

## controls.ini

### `[game_controller]`

| Key | Values | Default | Effect |
|---|---|---|---|
| `enabled` | `0`, `1` | `1` | Game controllers drive the launcher. See [Game controller](game-controller.md) |

### `[head_tracking]`

Values that are missing, negative or not a number use the default.

| Key | Range | Default | Effect |
|---|---|---|---|
| `enabled` | `0`, `1` | `1` | Head tracking. See [Head tracking](vr-viewer.md#head-tracking) |
| `counts_per_degree` | 1 to 100 | 20 | Mouse counts per degree of head turn |
| `stick_degrees_per_second` | 30 to 720 | 180 | The game's camera turn speed at full stick, in degrees per second |
| `stick_dead_zone` | 0 to 0.5 | 0.15 | The game's stick dead zone, as a share of full tilt |
