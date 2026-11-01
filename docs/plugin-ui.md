# Plugin UI

A plugin can add cards to Gazeora's pages. It describes each card in XML: what's on it, and
which values its controls show and change. Gazeora reads the XML when the plugin loads and
draws the card itself, with its own controls, so a plugin card looks like the rest of Gazeora
and a game controller moves through it the same way. The plugin never draws anything.

This needs plugin API version 3. See [Plugins](plugins.md#writing-a-plugin) for writing a
plugin in general.

## Registering a card

A plugin registers a `GazeoraUiPanelV3` through `register_ui_panel` of the version 3 host
(`GazeoraPluginHostV3Of(host)`), during `GazeoraPluginRegister`:

| Field | Required | Does |
|---|---|---|
| `struct_size` | Yes | `sizeof(GazeoraUiPanelV3)` |
| `self` | No | Passed back to every function |
| `xml` | Yes | The card's layout, UTF-8. It must stay valid while the DLL is loaded |
| `get(self, key, value, capacity)` | Yes | Writes the value under `key` as text, truncated to `capacity` (which counts the NUL). `""` if it has none |
| `set(self, key, value)` | Yes | The player changed the value under `key` to `value` |
| `action(self, action)` | Yes | The player pressed a button with this `action` |

Gazeora checks the layout when the plugin registers the card. A card with a missing function or
a layout that's wrong is refused, and the **Plugins** page says why under the plugin, for
example `refused a panel whose layout is wrong (line 4: <toggle> needs bind)`. The plugin's
other parts still load. A plugin can register several cards.

`get`, `set` and `action` are called on Gazeora's UI thread, only while the card is on screen.
`get` is called every frame for each value the card shows, so it should be quick.

Values are text:

| Control | Value |
|---|---|
| `<toggle>` | `"1"` on, `"0"` off |
| `<slider>` | A number, with `.` for decimals, such as `"0.5"` or `"180"` |
| `<input>` | The text |
| `<choice>` | The chosen `<option>`'s `value` |
| Conditions (`visible-if`, `enabled-if`) | False if `""`, `"0"` or `"false"`; true otherwise |

## The layout

The root is one `<panel>`. Its elements are drawn one under another, in order.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<panel page="settings" title="Example" icon="plugin">
  <toggle bind="enabled" label="Turn the example on" help="What it does, in a sentence."/>
  <slider bind="speed" label="Speed" min="1" max="10" format="%.0f" enabled-if="enabled"/>
  <text tone="dim">Changes take effect at once.</text>
  <button action="reset" icon="refresh">Reset</button>
</panel>
```

The layout is checked strictly. An element or attribute Gazeora doesn't know, a missing
required attribute, or a value that isn't one of the allowed ones refuses the card, with the
line it's on. Text inside an element, and attribute values, have their spaces and line breaks
run together, so long text can be wrapped over several lines.

### `<panel>`

| Attribute | Required | Values |
|---|---|---|
| `page` | Yes | The page the card goes on: `home`, `library`, `controls`, `plugins` or `settings`. A game's own page can't have cards |
| `title` | Yes | The card's title |
| `icon` | No | An [icon](#icons) before the title |
| `position` | No | `start`: under the page's title, before Gazeora's own content. Without it, after Gazeora's own content |

Cards with the same page and position appear in plugin load order.

### Elements

Every element can also have `visible-if` and `enabled-if` ([conditions](#conditions)).

| Element | Attributes | Content | Draws |
|---|---|---|---|
| `<text>` | `bind`, `tone`, `tone-bind` | The text, unless `bind` is set | Wrapped text. With `bind`, the value under that key. `tone` is a [tone](#tones); `tone-bind` takes the tone's id from a value instead |
| `<toggle>` | `bind` (required), `label`, `help` | None | A switch, its label beside it, and a help icon with `help` |
| `<button>` | `action` (required), `icon` | The label | A button that calls `action` with its `action`. It needs a label or an icon |
| `<slider>` | `bind` (required), `label`, `help`, `min` (default 0), `max` (default 1), `format` (default `%.2f`) | None | A slider, its label above it. `min` must be below `max`. `format` is printf-style, such as `%.0f%%` |
| `<input>` | `bind` (required), `label`, `help`, `hint` | None | A one-line text box, its label above it. `hint` shows while it's empty. With a game controller, it opens the on-screen keyboard. `set` is called on every change |
| `<choice>` | `bind` (required), `label`, `help` | `<option>`s | A drop-down list, its label above it |
| `<option>` | `value` (required) | The label; the value if empty | One entry of a `<choice>` |
| `<group>` | `layout` | Elements | Its elements one under another, or side by side with `layout="row"`. Use it to show or enable several elements together |
| `<table>` | None | `<row>`s | A table, each column as wide as its widest cell |
| `<row>` | None | `<cell>`s | One row of a table |
| `<cell>` | `tone` | The text | One cell of a row |
| `<separator>` | None | None | A line across the card |

### Conditions

`visible-if` shows an element only while the value under its key is true. `enabled-if` shows it
greyed out and unusable while the value is false. A `!` in front turns it around:
`visible-if="!gazeora.game-running"`. Any key works, the plugin's own or
[Gazeora's](#gazeoras-values).

### Tones

What a text means, drawn in Gazeora's colors: `normal` (the default), `dim`, `faint`, `accent`,
`success`, `warning`, `danger`.

### Icons

`add`, `camera`, `check`, `chip`, `clock`, `delete`, `edit`, `folder`, `game`, `globe`, `hand`,
`info`, `play`, `plugin`, `refresh`, `search`, `settings`, `shop`, `warning`.

## Gazeora's values

Keys and actions that start with `gazeora.` are Gazeora's own. Gazeora answers them itself
and never asks the plugin. They are read-only: a control bound to one doesn't change it. A
`gazeora.` key Gazeora doesn't have reads as `""`, and such an action does nothing.

| Key | Value |
|---|---|
| `gazeora.game-running` | `"1"` while a game launched from Gazeora is starting or running |
| `gazeora.gamepad.status` | The game controller line: the connected controllers, *Off*, *Paused while a game runs*, or why controllers can't be read |
| `gazeora.gamepad.tone` | That line's [tone](#tones) id |
| `gazeora.gamepad.failed` | `"1"` when controllers can't be read |

| Action | Does |
|---|---|
| `gazeora.gamepad.retry` | Reads game controllers again, after an error |

## Settings

A plugin keeps its own settings. `settings_folder` in `GazeoraPluginHostV3` is the folder
Gazeora keeps its settings in (`%APPDATA%\Gazeora`, beside `library.json`), as UTF-8. Keep
settings in a file named after the plugin there, and copy the path during
`GazeoraPluginRegister`: the string is valid only during the call. It's `""` when Gazeora has
no settings folder, as in the tests; then keep nothing.

## Example: the Game Controller plugin

The bundled Game Controller plugin's card on the **Controls** page is
[`plugins/controller/controller_panel.xml`](../plugins/controller/controller_panel.xml). Its
`enabled` is the plugin's own switch, saved in `controls.ini`
([`[game_controller]`](configuration.md#game_controller)); the rest are Gazeora's values. The
build turns the XML file into a string in the DLL (`gazeora_embed_text` in
[`plugins/CMakeLists.txt`](../plugins/CMakeLists.txt)).

## Limitations

- Cards can't go on a game's own page, or in the sidebar.
- A card has no images and no custom drawing: only the elements above.
- Values are polled every frame. A plugin can't push a change, and nothing tells it when its
  card stops being shown.
