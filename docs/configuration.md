# Configuration files

| File | Contents |
|---|---|
| `%APPDATA%\Gazeora\library.json` | The game library, shared by `Gazeora.exe` and `GazeoraCLI`. Set `GAZEORA_LIBRARY` to use another path. |
| `%APPDATA%\Gazeora\stream.ini` | The stream codec and protocol (`[stream]`) and VR viewer settings (`[vr]`: lenses, `lens_on_phone`, and `stereo`, `stereo_amount`, `stereo_front` for 3D), stored next to the library file. Plain INI, safe to edit by hand. |
| `%APPDATA%\Gazeora\controls.ini` | Game controllers (`[game_controller]`: `enabled`, `0` or `1`; on when missing). |
| `%APPDATA%\Gazeora\stream.log` | What stream players did, rewritten at each start. See [Stream log](streaming.md#stream-log). |
