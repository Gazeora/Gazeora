# VR viewer mode

VR viewer mode lays the stream out for a phone in a Cardboard-style VR viewer: one picture per
eye, shaped for the viewer's lenses. It is on by default and is set in **Settings → VR
headset**. Changes reach the stream immediately, so you can adjust them while wearing the
viewer.

## Setting up

1. Turn on **Split the picture for a phone VR viewer**.
2. Choose **Lens correction done by**:
   - **The phone (Gazeora app): sharper**, the default. Frames go out flat, in the game's own
     shape. The Gazeora app splits and warps them for its own screen. The lens settings travel
     with every frame (see [View info](stream-format.md#view-info)). Other players show these
     frames flat.
   - **This PC: for other players, such as VLC.** Frames are split and warped before they are
     sent, shaped to **Phone screen shape (landscape)**, so any player can show them.
3. Choose **Viewer lenses**: **Google Cardboard (2015)**, **Google Cardboard (2014)**, or
   **No lens correction**. Changing a slider afterwards shows the choice as **Custom**.
4. Play the stream full screen in landscape and put the phone in the viewer.

**Reset VR settings** restores the defaults. The page shows a preview of how the settings shape
the picture.

## Lens settings

| Setting | Default | Effect |
|---|---|---|
| Lens distortion (k1) | 0.340 | Barrel pre-distortion that cancels the lenses' pincushion distortion: r′ = r(1 + k1·r² + k2·r⁴) |
| Edge distortion (k2) | 0.550 | The same, for the edge of the view |
| Picture size | 0.70 | Picture height in each eye, relative to the eye's view |
| Distance between lenses (share of screen width) | 0.500 | 0.5 puts each lens center in the middle of its half |
| Color fringe correction | 0.000 | Draws blue smaller and red larger to cancel the lenses' color fringes |
| Phone screen shape (landscape) | 2.00:1 | Only when this PC does the lens correction. Presets 16:9 to 21:9 |

The settings are saved under `[vr]` in [`stream.ini`](configuration.md#vr).

## Gazeora's window and the hand pointer

While no game runs, the stream shows Gazeora's own window. With the lens correction done by the
phone, the window fills both eyes as a game does, and the Android app can track your hand with
the phone's back camera to point at it: the app draws a cursor on the window, a pinch clicks,
and a moving pinch scrolls. The app sends these to the PC as
[Pointer messages](stream-format.md#pointer-type-5), the same way it sends touches on a flat
picture. They control only Gazeora's window, not the PC's mouse. While a game is launching or
running, pointer input is ignored.

The hand tracking itself runs in the Android app; see its
[hand pointer guide](https://github.com/Gazeora/Gazeora-Android/blob/master/docs/user-guide.md#hand-pointer) for the camera permission and lighting
requirements. With **Settings → Stream overlay → Phone camera** you can see the
camera's state in the stream (overlay builds only).

## 3D from depth

**3D from the game's depth** gives each eye its own view, built from the game's depth buffer.
It is off by default.

The hook finds the game's main depth buffer (the one with the most draws at the back buffer's
size) and copies it beside each frame. Gazeora shifts each pixel sideways by its depth, in
opposite directions for the two eyes. Whatever is in the middle of the view sits at the screen
plane; farther things recede and nearer ones come forward. Gazeora detects whether the game
stores depth reversed and follows the subject's depth, eased over a few frames.

Where one eye sees behind an object, the background is stretched over the gap.

| Setting | Default | Effect |
|---|---|---|
| 3D depth (how far back distant things go) | 0.012 | How much the two views differ for distant things, as a share of an eye's width. Higher values strain the eyes and widen stretched areas |
| Pop-out (how far near things come forward) | 0.50 | How far things nearer than the subject come out of the screen, as a share of the 3D depth |

The two views go out side by side in one frame, each squeezed to half the width, so the frame
size is the same as in 2D. The game's page shows whether 3D is working.

Limitations:

- Direct3D 11 and Direct3D 12 games only. Other graphics APIs, Gazeora's window, menus, videos,
  and scenes without a depth buffer at the back buffer's size stay 2D.
- Things drawn over the scene without depth, such as the HUD, take the depth of what is behind
  them and bend with it.
- Direct3D 12 games are followed from their first frame when launched from Gazeora. A Direct3D
  12 game attached to while already running (for example with `GazeoraCLI attach`) works only
  if it moves its depth buffer out of the depth-write state each frame.

## Head tracking

While a game is shown in the viewer, turning your head turns the game's camera. The Android app
sends the phone's orientation to the PC as [HeadPose messages](stream-format.md#headpose-type-7),
over the USB cable or to UDP port 8557. Gazeora turns each change in orientation into movement
for the game in front. Only changes count, so the camera starts from wherever it already looks,
and you can still turn with the mouse or a stick at the same time.

The app sends head poses only while frames are laid out for a VR viewer, which means VR mode on
with the lens correction done by the phone. See the Android app's
[head tracking notes](https://github.com/Gazeora/Gazeora-Android/blob/master/docs/user-guide.md#head-tracking).

Head tracking is set in **Controls → Head tracking**:

| Setting | Default | Effect |
|---|---|---|
| Turn the game's camera with your head | On | Head tracking |
| Mouse sensitivity | 20 mouse counts per degree (1 to 100) | Mouse movement per degree of head turn |
| Controller: the game's turn at full stick | 180° a second (30 to 720) | The game's camera speed at full right-stick tilt |
| Controller: the game's stick dead zone | 15% (0 to 50%) | How much of the stick's center the game ignores |

To tune the mouse sensitivity, turn your head a quarter turn. If the game turned less, raise it;
if more, lower it. Games differ, and so do their own sensitivity settings.

The card's status line shows whether head poses are arriving and whether they are reaching a
game, and through which device. Once the game has read a controller, the card also says whether
it reads a PlayStation controller on USB or an Xbox-style controller (XInput).

### Mouse or controller

Games often switch their button prompts and controls to whichever device you used last. To
avoid pulling a controller game over to mouse and keyboard, Gazeora sends head turns through the
device you are using:

- **Mouse and keyboard:** head turns become mouse movement (Windows `SendInput`).
- **Controller:** the capture hook adds head turns to the controller's right stick as the game
  reads it, on top of your own input. The game never sees a mouse.

Touch the keyboard or mouse and head turns go to the mouse; touch the controller and they go to
the controller.

Controller head turns work when the game reads:

- An Xbox-style controller through XInput (`xinput1_4.dll`, `xinput1_3.dll` or
  `xinput9_1_0.dll`), including controllers that Steam Input presents as one.
- A DualSense, DualSense Edge or DualShock 4 on USB, read directly by the game.

A stick sets how fast the camera turns, not where it points, so Gazeora pushes the stick just
enough to match your head by the game's next read. If the game turns less than your head, lower
**the game's turn at full stick**; if more, raise it. If small head turns go missing, raise the dead zone;
if the camera twitches, lower it. Games that accelerate the camera while the stick is held
follow less closely.

### Head tracking limitations

- Only a game launched from Gazeora, and only while it is the foreground window. Switching to
  another window stops head tracking immediately, so the desktop pointer is not moved.
- Roll (tilting your head sideways) is not passed on.
- The camera follows your head after the stream's delay, so fast turns lag.
- Windows does not let a program move the mouse in a game running as administrator unless that
  program also runs as administrator.
- Head turn speed through the controller depends on the game's own stick response; the settings
  above can only approximate it.

The [test app](tools.md#test-app) turns its camera with the mouse and can be used to try head
tracking without a game.
