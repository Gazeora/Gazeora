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

| Button | Does |
|---|---|
| D-pad or left stick | Moves between controls: the sidebar, buttons, switches, game cards |
| Bottom face button (A on Xbox, Cross on PlayStation, B on Nintendo) | Presses the focused control |
| Right face button (B, Circle, A) | Backs out: closes a list or popup, or leaves a field |
| Left and right shoulder buttons (LB / RB, L1 / R1, L / R) | The previous and next page in the sidebar |
| Right stick | Scrolls the page, faster the further it's pushed |

Held, the d-pad and the left stick repeat. To change a slider, press the bottom button on it,
move it with left and right, and press the bottom button again.
Sticks have a dead zone, so one that rests slightly off center (most do) doesn't move anything.
Typing still needs the keyboard.

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
  left and top face buttons aren't used.
- **Steam Input.** Steam can stand in for a controller while Steam is open, and it may then
  show up as an Xbox controller. Either way, it works.
