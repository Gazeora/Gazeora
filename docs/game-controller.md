# Game controller

A game controller can drive Gazeora's window, alongside the mouse and keyboard. It is set in
**Controls → Game controller** with **Move around Gazeora with a game controller**. It is on by
default and the setting is saved as `[game_controller] enabled` in
[`controls.ini`](configuration.md#game_controller).

Gazeora reads controllers through [SDL](https://libsdl.org), which maps most controllers
(Xbox, PlayStation, Nintendo Switch Pro and others, wired or Bluetooth) to one standard layout.
Buttons are identified by position, not by label: the "bottom face button" is A on Xbox, Cross
on PlayStation, and B on Nintendo. Several controllers can be connected at once, and any of
them can drive. The card lists the connected controllers by name.

## Navigation

The focused control has a colored edge. The sidebar and the page are two regions side by side.

| Button | Action |
|---|---|
| D-pad or left stick | Moves the focus. Right from the sidebar enters the page; left from the page's edge returns to the sidebar |
| Bottom face button | Presses the focused control. In the sidebar, opens the page and stays in the sidebar. On a game, opens its page. On a text box, opens the [on-screen keyboard](#typing) |
| Right face button | Goes back one step: closes an open list, menu, keyboard or slider edit; then from a game's page to the library; from any other page to its sidebar entry |
| Menu / Options / + | On a game: its menu (Play, Details, Open file location, Remove), as a right-click would |
| Shoulder buttons | Previous and next page |
| Right stick | Scrolls the page, faster the further it is pushed |

Holding the d-pad or left stick repeats. To change a slider, press the bottom face button on
it, move it with left and right, and press the bottom face button again. Sticks have a dead
zone, so a stick resting slightly off center does not move the focus.

## Typing

Selecting a text box with the controller opens a keyboard inside the window, so it also shows
in the stream. It types into the box as a physical keyboard would.

| Button | Action |
|---|---|
| D-pad or left stick | Picks a key (repeats when held) |
| Bottom face button | Types the key |
| Left face button | Deletes the character before the caret (repeats when held) |
| Top face button | Types a space |
| Left shoulder | Shift: the next letter in capitals, or the symbols on digit keys |
| Right shoulder | Switches between letters and symbols |
| Right face button, or Menu / Options | Done: keeps the text and closes the keyboard |

The on-screen keys **Shift**, **&123** / **ABC**, **Space**, **Delete** and **Done** do the
same.

## When the controller drives Gazeora

- **While Gazeora is in front, or on the stream.** The controller drives Gazeora while its
  window is the foreground window, or while the stream is showing the window to at least one
  viewer, including with Gazeora minimized. Otherwise Gazeora ignores the controller.
- **Never while a game runs.** While a game launched from Gazeora is starting or running,
  Gazeora stops reading controllers and releases them, so the game has them to itself. The card
  shows *Paused while a game runs*. They are picked up again when the game closes.
- **Controller state is left unchanged.** Gazeora does not switch PlayStation controllers to
  their enhanced report mode and does not change player LEDs.

## Limitations

- The guide button (Xbox, PS) is not used.
- The left and top face buttons are used only by the on-screen keyboard.
- **Add game**, **Browse** and **Choose image...** open Windows file dialogs, which need a mouse
  and keyboard.
- With Steam running, Steam Input may present a controller as an Xbox controller. Behavior with
  Steam Input has not been separately verified.

## Troubleshooting

- **"No controller connected: plug one in or pair it over Bluetooth".** Plug the controller in,
  or pair it in Windows' Bluetooth settings. It is detected without restarting Gazeora.
- **An error instead of the controller list.** The controller system failed to start. Click
  **Try again** on the card.
