# Plugins

A plugin is a DLL that adds to Gazeora without rebuilding it. Plugins can add:

- **Game stores**: a store's client that games start through, as Gazeora does with Steam. The
  store appears under **Launch through** on a game's page, and *Automatic* uses it for the
  games it knows.
- **Video encoders**: another way to encode the stream. Gazeora tries plugin encoders before
  its own Media Foundation encoder.
- **Codecs** (plugin API version 2): a codec the stream can carry, listed under **Video codec**
  in the settings. It needs an encoder for it too, from the same plugin or another one.
- **Protocols** (plugin API version 2): another way frames leave the PC, listed under the
  stream's protocol in the settings.

Capture and the UI can't be extended by plugins.

## Bundled plugins

Some of Gazeora's own protocols and codecs are plugins. They come with Gazeora but aren't
installed: the **Plugins** page lists them under **Available**, marked *Comes with Gazeora*, and
**Install** copies Gazeora's own copy into the plugins folder, with no download.

| File | Name | Adds |
|---|---|---|
| `gazeora-udp.dll` | UDP Direct | The UDP protocol |
| `gazeora-http.dll` | HTTP Live | The HTTP protocol |
| `gazeora-usb.dll` | USB Cable | The USB protocol |
| `gazeora-h265.dll` | H.265 (HEVC) | The H.265 codec, encoded by Gazeora's own Media Foundation encoder |

RTSP over TCP and over UDP and the H.264 codec are part of `Gazeora.exe`, because every player
can use them: they can't be removed.

They wait in `bundled-plugins` in the install folder (`C:\Program Files\Gazeora\bundled-plugins`),
beside `plugins`, with `bundled.json`, which says what they are in the
[plugin list's format](#publishing-to-the-plugin-list). Gazeora never loads
plugins from there. Once installed, a bundled plugin uninstalls like any other, and **Update**
appears for it when a newer Gazeora brings a newer version.

If the stream's protocol came from a plugin that isn't installed, the stream uses RTSP over TCP;
if its codec did, H.264.

## Managing plugins

The **Plugins** page in the sidebar shows the installed plugins and the plugin list:

- **Installed**: every DLL in the plugins folder, whether it loaded, what it added, and why it
  didn't load if it didn't. **Uninstall** removes one. **Update** appears when the plugin list
  has a newer version, and **Reinstall** when an installed plugin didn't load but the list has
  a build this Gazeora can use.
- **Available**: the plugins that aren't installed, with **Install**: the bundled plugins, then
  the plugin list's. Plugins can be installed and uninstalled while the list loads.
- **Search** filters both by name, description, author and id.
- The category chips (**Protocols**, **Codecs**, **Encoders**, **Game stores**, **Other**) show
  only the plugins of one kind, with how many there are. Each tile shows its category too.
- **Refresh list** loads the plugin list again. Gazeora loads it from
  `https://gazeora.github.io/data/plugins/windows/index.json` the first time the page is opened
  in a session, not before.
- **Install from file** installs a DLL from your disk. The plugin is named after the file, in
  lowercase, with other characters than letters, digits, `-` and `_` turned into `-`. Installing
  a file with the name of an installed plugin replaces it.

Each install, update and uninstall asks for administrator permission, because the plugins folder
is under Program Files. Changes take effect when Gazeora next starts: a plugin that's loaded
stays loaded until then.

A download from the list is kept only if its SHA-256 matches the list's. The copy in the plugins
folder is checked again, as an administrator, before it replaces anything, also for a file from
disk. Plugins downloaded from the list wait in `%TEMP%\Gazeora\plugins` and are deleted once
installed.

`GazeoraCLI plugins` lists the installed plugins and how they loaded.

### By hand

1. Get the plugin build for your Gazeora release: an x64 plugin for the x64 release, x86 for
   x86. A plugin for the other architecture is refused.
2. Copy the DLL into the `plugins` folder beside the `bin` folder `Gazeora.exe` is in. For an
   MSI install that's `C:\Program Files\Gazeora\plugins` (`Program Files (x86)` for x86).
   Create the folder if it doesn't exist. Writing there needs administrator rights.
3. Restart Gazeora. Plugins load only at start.

To remove a plugin by hand, close Gazeora, delete its DLL and start Gazeora again. The MSI
doesn't install plugins, and uninstalling Gazeora leaves the plugins folder as it is.

When the Plugins page replaces or removes a plugin that's loaded, it renames the old DLL to
`<name>.dll.<number>.removed`, because Windows doesn't let a loaded DLL be deleted. Gazeora
never loads those files, and the next install or uninstall deletes them.

A plugin runs inside Gazeora with the same rights, and Gazeora sometimes runs as
administrator. Only install plugins you trust. Gazeora loads plugins from the `plugins` folder
beside its `bin` folder only, never from your user folders. A plugin's own DLLs are looked for in its
folder and the system folders, not in the current directory.

## Writing a plugin

A plugin is built against one C header,
[`gazeora_plugin.h`](../libs/plugin_api/include/gazeora/plugin_api/gazeora_plugin.h). It's plain
C, so a plugin can be written in C, C++ or another language that can export C functions, with
any compiler. The header is under the MIT license
([libs/plugin_api/LICENSE](../libs/plugin_api/LICENSE)), and a plugin that uses only the header
can have any license, closed-source included. See the additional permission for plugins in
[NOTICE](../NOTICE).

A plugin exports two functions, with the `__cdecl` calling convention (`GAZEORA_CALL`):

| Export | Does |
|---|---|
| `GazeoraPluginInfo` | Returns a `GazeoraPluginInfoV1`: the plugin's name, version and the plugin API version it was built with |
| `GazeoraPluginRegister` | Registers what the plugin offers through the `GazeoraPluginHostV1` it's given. For codecs and protocols, `GazeoraPluginHostV2Of(host)` gives the version 2 host, or `NULL` from a Gazeora that has only version 1. Returning `GAZEORA_FALSE` drops everything it registered |

A minimal plugin with one game store:

```c
#include <stdio.h>
#include <string.h>

#include "gazeora/plugin_api/gazeora_plugin.h"

static void GAZEORA_CALL AppIdFor(void* self, const char* exe_path, char* app_id,
                                  size_t capacity) {
    /* Look the exe up in the store's install records; "" if it isn't the store's. */
    snprintf(app_id, capacity, "%s", strstr(exe_path, "\\ExampleStore\\") ? "42" : "");
}

static int32_t GAZEORA_CALL Launch(void* self, const char* app_id, char* error,
                                   size_t capacity) {
    /* Ask the store's client to start the game, for example through its URL scheme. */
    snprintf(error, capacity, "not implemented");
    return GAZEORA_FALSE;
}

static const GazeoraPluginInfoV1 kInfo = {sizeof(GazeoraPluginInfoV1),
                                          GAZEORA_PLUGIN_API_VERSION, "Example store", "1.0.0"};
static const GazeoraGameStoreV1 kStore = {sizeof(GazeoraGameStoreV1), NULL, "example",
                                          "Example store", AppIdFor, Launch};

GAZEORA_PLUGIN_EXPORT const GazeoraPluginInfoV1* GAZEORA_CALL GazeoraPluginInfo(void) {
    return &kInfo;
}

GAZEORA_PLUGIN_EXPORT int32_t GAZEORA_CALL GazeoraPluginRegister(const GazeoraPluginHostV1* host) {
    return host->register_game_store(host->context, &kStore);
}
```

[`tests/plugins/sample_plugin/sample_plugin.cpp`](../tests/plugins/sample_plugin/sample_plugin.cpp)
is a complete plugin with a game store and an encoder, which the tests load.

### Rules

- Strings are UTF-8 and NUL-terminated. Text a plugin returns goes into the buffer Gazeora
  passes, truncated to its capacity (which counts the NUL).
- No exception or `longjmp` may leave a plugin function. A plugin that crashes takes Gazeora
  down with it.
- Pointers Gazeora passes are valid only during the call. Gazeora copies the structs a plugin
  registers, but the strings and functions they point to must stay valid while the DLL is
  loaded.
- Each struct starts with `struct_size`; set it to `sizeof` the struct.

### Game stores

- `id` is stored in `library.json` as the game's `launchVia`, so keep it the same across
  versions. It's lowercase `a`–`z`, `0`–`9` and `-`, at most 64 characters, and not `auto` or
  `direct`. A store with an invalid id is refused.
- Plugin stores come after Gazeora's own ones. A plugin store with the id of a built-in store,
  such as `steam`, replaces it.
- `app_id_for` and `launch` can be called from any thread, also at the same time.
- After `launch`, Gazeora waits for the game's process by its exe name, as it does for Steam.

### Encoders

- Each time the stream opens an encoder, Gazeora tries the encoder plugins in load order, then
  Media Foundation, and keeps the first that opens for the codec.
- An encoder is used from one thread, which has COM (multithreaded) and `MFStartup`.
- Frames are NV12. With `d3d11_device` set in the settings, they come as `ID3D11Texture2D`
  textures on that device through `encode_texture`, and an encoder without `encode_texture`
  refuses to open with a device. Gazeora then reads frames back to memory instead.
- `AddRef` the device or a texture to keep it past the call.
- Output is Annex B (NAL units with start codes), without B-frames.

### Codecs

A codec plugin describes a codec whose bitstream is NAL units in Annex B, as H.264, H.265 and
H.266 are, in a `GazeoraCodecV2`. Encoding it is an encoder's job: register an encoder provider
as a `GazeoraEncoderProviderV2`, whose `supports_id` takes codec ids, and read the codec's id from
the `GazeoraEncoderSettingsV2` that `open` gets.

- `id` follows the same rules as a game store's, and stream.ini and the discovery reply carry it.
  A plugin codec with a built-in codec's id replaces it.
- Gazeora builds the RTP packets itself: a NAL unit that fits goes in a packet of its own, and a
  larger one is cut into fragmentation units, each with the payload header
  `fragment_payload_header` gives, then a byte with the start and end bits and the NAL type.
- `fmtp` gives the SDP `fmtp` line from the parameter sets seen so far, and
  `required_parameter_sets` the ones a key frame needs in front of it. Gazeora puts them there.
- `ts_stream_type` and `access_unit_delimiter` are for the HTTP protocol's MPEG transport
  stream.
- `sei_header` is the header of an SEI NAL unit, which carries the VR viewer settings to the
  Gazeora app. With a size of 0, frames of this codec go without them.
- The UDP and USB protocols mark only H.265 in their frame header, so the Gazeora app treats any
  other codec on them as H.264. Players also need a decoder for the codec.

### Protocols

A protocol plugin registers a `GazeoraProtocolV2`, whose `create_output` makes the
`GazeoraOutputV2` that runs while the protocol is chosen.

- `id` follows the same rules as a game store's, but can't be `rtsp-tcp` or `rtsp-udp`, which
  the stream server serves itself. A plugin protocol with another built-in id (`udp`, `http`,
  `usb`) replaces it.
- `start`, `stop` and `send` are called one at a time. The other functions can be called from
  any thread at any time.
- `send` gets each encoded frame, and a `GazeoraLossReporterV2` to report the share of packets
  a player lost, so the stream lowers its bitrate. Both are valid during the call only. The
  frame's `codec` describes its codec as a codec plugin does, also for H.264, and stays valid
  while the output exists.
- `take_key_frame_request` returning `GAZEORA_TRUE` makes the next frame a key frame: return
  it once when a player joins or loses a frame.
- The `GazeoraOutputHostV2` it's made with stays valid while the output exists. Its `log`
  writes a line to the [stream log](streaming.md#stream-log).
- Touches, head poses and camera status the Gazeora app sends over the network go to Gazeora
  itself, which takes them only from a player that `watching` says is watching. Without
  `watching`, it takes none.
- An output whose players send them over its own link, such as a cable, hands each message to
  the host's `input`: the type (`GAZEORA_INPUT_*`) and the payload of that
  [GZF1 message](stream-format.md).
- `app_viewers` counts the viewers that are the Gazeora app; without it, every viewer counts.
- `needs_usb_driver` and `usb_devices_needing_driver` let the settings offer the USB driver
  Gazeora installs, as the USB protocol does.
- `status`, `app_viewers`, `watching`, `needs_usb_driver` and `usb_devices_needing_driver` may
  be `NULL`.

### Versions

This is plugin API version 2 (`GAZEORA_PLUGIN_API_VERSION`). Gazeora loads plugins built for
version 1 or 2 (`GAZEORA_PLUGIN_API_OLDEST_VERSION` to `GAZEORA_PLUGIN_API_VERSION`), and says so
on the **Plugins** page for any other. A struct version 2 extends is a new struct whose first
member is the version 1 one, so a version 1 plugin works unchanged. Gazeora reads a plugin's
version 2 fields only when its `struct_size` covers them.

## Publishing to the plugin list

The plugin list is `data/plugins/windows/index.json` in the
[gazeora.github.io](https://github.com/Gazeora/gazeora.github.io) repository. It's edited by
hand. Each entry:

```json
{
 "platform": "windows",
 "api_version": 1,
 "plugins": [
  {
   "id": "example-store",
   "name": "Example store",
   "summary": "Starts games through the Example store's client",
   "author": "Example Author",
   "license": "MIT",
   "homepage": "https://example.com/gazeora-plugin",
   "version": "1.0.0",
   "api_version": 1,
   "assets": {
    "x64": {
     "name": "example-store-x64.dll",
     "size": 45056,
     "browser_download_url": "https://example.com/example-store-x64.dll",
     "digest": "sha256:<64 hex digits>"
    },
    "x86": {"name": "...", "size": 0, "browser_download_url": "...", "digest": "sha256:..."}
   }
  }
 ]
}
```

| Field | Rules |
|---|---|
| `id` | Required. `a`–`z`, `0`–`9`, `-` and `_`, at most 64 characters. The plugin installs as `<id>.dll`, whatever its download is called |
| `name`, `version` | Required. `version` is `MAJOR.MINOR.PATCH`, the same as `GazeoraPluginInfo` reports, so **Update** can compare them |
| `summary`, `author`, `license` | Shown on the page and searched |
| `category` | What kind of plugin it is, for the page's label and filter: `protocol`, `codec`, `encoder`, `game-store` or `other`. Without it, or with another word, an installed plugin's category comes from what it registered most of, and one that isn't installed shows as *Other* |
| `homepage` | Shown as **Website**. Only `https://` addresses are kept |
| `api_version` | The plugin API version the build is for. Builds for a version this Gazeora doesn't load are listed but can't be installed |
| `assets` | One build per architecture, `x64` and `x86`. A download must be `https://` and have a `sha256:` digest, or it's left out. A `size` of 0 skips the size check |

Entries that break these rules are left out of the page.

## Why a plugin doesn't load

| Shown | Cause |
|---|---|
| `not built for this release's architecture` | An x86 DLL in the x64 release, or the other way round |
| `could not load the DLL (error N)` | Windows couldn't load it, often because a DLL it needs is missing. Error 126 is a missing DLL |
| `not a Gazeora plugin` | The DLL doesn't export `GazeoraPluginInfo` and `GazeoraPluginRegister` |
| `built for plugin API version N` | It was built for a plugin API version this Gazeora doesn't load |
| `the plugin stopped while registering` | Its `GazeoraPluginRegister` returned `GAZEORA_FALSE` |
| `refused a game store with an invalid id` | Its store id breaks the rules above. The plugin's other parts still load |
| `refused a codec ...` or `refused a protocol ...` | The codec or protocol is missing something, or its id breaks the rules above. The plugin's other parts still load |
