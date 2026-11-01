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
- **Game controller drivers** (plugin API version 3): what reads game controllers, so they can
  drive Gazeora's window. Gazeora has none of its own.
- **Cards** (plugin API version 3): cards on Gazeora's pages, described in XML, which Gazeora
  draws with its own controls. See [Plugin UI](plugin-ui.md).

Capture can't be extended by plugins, and the UI only through cards. The controller navigation
is Gazeora's own: a driver only reports the buttons and sticks.

## Bundled plugins

Some of Gazeora's own protocols and codecs, and its game controller support, are plugins. They
come with Gazeora but aren't installed: the **Plugins** page lists them under **Available**, marked *Comes with Gazeora*, and
**Install** copies Gazeora's own copy into the plugins folder, with no download.

| File | Name | Adds |
|---|---|---|
| `gazeora-udp.dll` | UDP Direct | The UDP protocol |
| `gazeora-http.dll` | HTTP Live | The HTTP protocol |
| `gazeora-usb.dll` | USB Cable | The USB protocol |
| `gazeora-h265.dll` | H.265 (HEVC) | The H.265 codec, encoded by Gazeora's own Media Foundation encoder |
| `gazeora-controller.dll` | Game Controller | A game controller driver, through SDL, and the **Game controller** card on the **Controls** page. See [Game controller](game-controller.md) |

RTSP over TCP and over UDP and the H.264 codec are part of `Gazeora.exe`, because every player
can use them: they can't be removed.

They wait in `bundled-plugins` in the install folder (`C:\Program Files\Gazeora\bundled-plugins`),
beside `plugins`, with `bundled.json`, which says what they are in the
[plugin list's format](plugin-development.md#publishing-to-the-plugin-list). Gazeora never loads
plugins from there. Once installed, a bundled plugin uninstalls like any other, and **Update**
appears for it when a newer Gazeora brings a newer version.

If the stream's protocol came from a plugin that isn't installed, the stream uses RTSP over TCP;
if its codec did, H.264. Without the Game Controller plugin, Gazeora reads no game controllers
and the **Controls** page has no **Game controller** card.

## Managing plugins

The **Plugins** page in the sidebar shows the installed plugins and the plugin list:

- **Installed**: every DLL in the plugins folder, whether it loaded, what it added, and why it
  didn't load if it didn't. **Uninstall** removes one. **Update** appears when the plugin list
  has a newer version, and **Reinstall** when an installed plugin didn't load but the list has
  a build this Gazeora can use.
- **Available**: the plugins that aren't installed, with **Install**: the bundled plugins, then
  the plugin list's. Plugins can be installed and uninstalled while the list loads.
- **Search** filters both by name, description, author and id.
- The category chips (**Protocols**, **Codecs**, **Encoders**, **Game stores**, **Controllers**,
  **Other**) show
  only the plugins of one kind, with how many there are. Each tile shows its category too.
- **Refresh list** loads the plugin list again. Gazeora loads it from
  `https://gazeora.github.io/data/plugins/windows/index.json` the first time the page is opened
  in a session, not before.
- **Install from file** installs a DLL from your disk. The plugin is named after the file, in
  lowercase, with other characters than letters, digits, `-` and `_` turned into `-`. Installing
  a file with the name of an installed plugin replaces it.

Each install, update and uninstall asks for administrator permission, because the plugins folder
is under Program Files.

Installing and uninstalling take effect at once, without restarting Gazeora:

- An installed plugin loads, and what it adds is there straight away: its protocol or codec in
  the settings, its game store, its cards, its game controller driver.
- An uninstalled plugin's parts go. If the stream used its protocol, it switches to RTSP over
  TCP; if it used its codec, to H.264.
- If the plugin adds encoders or codecs, or the stream's protocol changes, the stream restarts,
  and anyone watching has to reconnect, as when the protocol is changed by hand.

Updating or reinstalling a plugin that has been loaded since Gazeora started takes effect when
Gazeora next starts: Windows doesn't let a running program load a new copy of a DLL it has
loaded. The plugin's card says so, and the old version keeps working until then.

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
3. Restart Gazeora. Gazeora loads plugins copied there by hand only at start; the **Plugins**
   page loads the ones it installs straight away.

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
[`gazeora_plugin.h`](../libs/plugin_api/include/gazeora/plugin_api/gazeora_plugin.h), under the
MIT license, so it can have any license, closed-source included. [Plugin
development](plugin-development.md) covers writing, building, testing and publishing one,
starting from the example plugin in
[`examples/plugins/frame_recorder`](../examples/plugins/frame_recorder), and adding a bundled
plugin to this repository. [Plugin UI](plugin-ui.md) describes the cards a plugin adds.

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
| `refused a panel with missing fields` or `refused a panel whose layout is wrong (...)` | A card is missing a function, or its XML isn't a valid layout: the text says what and on which line. See [Plugin UI](plugin-ui.md). The plugin's other parts still load |
| `refused a game controller driver with missing fields` | Its `GazeoraGamepadDriverProviderV3` has no `name` or `create`, or its `struct_size` is too small. The plugin's other parts still load |
