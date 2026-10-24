# Game controller

**Controls → Game controller** lets a game controller move around Gazeora, alongside the mouse
and keyboard, which keep working as always. It's on unless you switch it off, and the switch
is remembered.

Any controller works: Xbox (One, Series, 360), PlayStation (DualSense, DualShock 4), Nintendo
Switch Pro and most other USB or Bluetooth gamepads. Gazeora reads them through
[SDL](https://libsdl.org), which knows the layouts of nearly every make, and gives every
controller the same standard layout. So a button does the same thing wherever it sits on the
controller, not by what it's called. Several controllers can be connected at once; any of
them drives.

It works the way Steam's Big Picture does: the control the controller is on looks lifted a
little, lighter and with a soft glow beneath it, and the sidebar and the page are two regions
side by side.

| Button | Does |
|---|---|
| D-pad or left stick | Moves the focus. Right from the sidebar goes into the page (where you were, or its top once the page has changed); left from the page's edge goes back to the sidebar, onto the open page |
| Bottom face button (A on Xbox, Cross on PlayStation, B on Nintendo) | Presses the focused control. In the sidebar, opens the page and stays in the sidebar; on a game, opens its page; on a text box, starts typing |
| Right face button (B, Circle, A) | Backs out, a step at a time: first it closes what's open (a list, a menu, the keyboard, a slider being changed); then from a game's page back to the library, onto that game; from any other page to its entry in the sidebar. In the sidebar it stays put |
| Menu / Options / + button | On a game: its menu (Play, Details, Open file location, Remove), as a right-click gives |
| Left and right shoulder buttons (LB / RB, L1 / R1, L / R) | The previous and next page. In the sidebar, the focus follows to the new page's entry; in a page, it goes to the new page's first control |
| Right stick | Scrolls the page, faster the further it's pushed |

Held, the d-pad and the left stick repeat. To change a slider, press the bottom button on it,
move it with left and right, and press the bottom button again. Focusing an ⓘ shows its
explanation. Sticks have a dead zone, so one that rests slightly off center (most do) doesn't
move anything.

## Typing

Selecting a text box with the controller (the library's search, a game's name, launch
arguments, folders, the stream overlay's text) brings up a keyboard on screen, in the window
itself, so it's in the stream and the headset too. It types into the box as a keyboard would,
and the box saves as it always does.

| Button | Does |
|---|---|
| D-pad or left stick | Picks a key (held, it keeps going) |
| Bottom face button | Types the key |
| Left face button (X, Square, Y) | Deletes the character before the caret (held, it keeps deleting) |
| Top face button (Y, Triangle, X) | Types a space |
| Left shoulder | Shift: the next letter in capitals, or the digits' symbols |
| Right shoulder | Switches between letters and symbols (`\ / : . _ -` and the rest for paths and arguments) |
| Right face button, or Menu / Options | Done: keeps the text and puts the keyboard away |

The keys **Shift**, **&123** / **ABC**, **Space**, **Delete** and **Done** do the same.

The card lists the controllers it sees, by name.

## When it drives Gazeora

- **Gazeora in front, or on the stream.** The controller drives Gazeora while its window is
  the one in front, or while the stream shows the window (**Settings → Frame stream**, with the
  window as the picture and someone watching). That includes the phone in the headset, with
  Gazeora minimized. With another program in front and nobody watching, Gazeora leaves the
  controller alone.
- **Never while a game runs.** While a game launched from Gazeora is starting or running,
  Gazeora stops reading controllers and lets go of them entirely, so the game has them to
  itself. They're picked up again when it closes.
- **Controllers are left as games expect them.** Gazeora doesn't switch a DualSense or
  DualShock 4 into its extended report mode, and doesn't change player lights. Over Bluetooth
  that mode would otherwise last until the controller reconnects, and some games don't
  read it.

## Troubleshooting

- **"No controller connected".** Plug the controller in, or pair it in Windows' Bluetooth
  settings first. It shows up within a moment, with no need to restart Gazeora.
- **A button does nothing.** The guide button (Xbox, PS) is left to Windows and Steam. The
  left and top face buttons are used only by the on-screen keyboard.
- **Adding games and choosing pictures.** **Add game**, **Browse** and **Choose image...**
  open Windows' own file dialogs, which need the mouse and keyboard.
- **Steam Input.** Steam can stand in for a controller while Steam is open, and it may then
  show up as an Xbox controller. Either way, it works.
