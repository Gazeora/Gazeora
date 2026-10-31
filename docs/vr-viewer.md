# VR viewer mode

VR mode is on by default and can be switched off in **Settings → VR headset**. Choose the
viewer's lenses (Google Cardboard 2015 or 2014 presets, or no correction), then play the stream
full screen in landscape and put the phone in the viewer. Changes reach the stream immediately,
so the distortion, picture size, lens separation and color-fringe sliders can be tuned with the
headset on.

**Lens correction done by** picks where the eyes are drawn:

- **The phone (Gazeora app)**, the default. Frames go out flat, in the game's own shape, and
  the app draws them into both eyes for its own screen with OpenGL ES. Every pixel of the stream
  is the game's, so the picture is sharper than a pre-warped frame of the same size. The lens
  settings travel with every frame (see [Stream format](stream-format.md)), so slider changes show
  at once. Players other than the app show these frames flat.
- **This PC**, for VLC and other players. Frames go out split, warped and shaped to the phone's
  screen, which you pick here.

## Gazeora's window and your hand

While no game runs, the stream shows Gazeora's own window, and with the lens correction done by
the phone, it fills both eyes as a game does. The Android app then tracks your hand with the
phone's back camera (its picture never shows) and lets it point at the window, as VR headsets
let a hand point at a screen out of reach: the middle of your palm moves a cursor the phone
draws on the window, a pinch clicks, and a pinch that moves scrolls. The phone sends these to
the PC as pointer messages on the window's own spots, the way it sends touches on a flat
picture, so the PC takes pointer input in VR too. While a game is launching or running, they're
off, like touches. A denied camera permission, or a camera in use elsewhere, leaves the window
without a pointer.

While the picture is laid out for the viewer, the phone's screen stays in the landscape it was
in: tilting your head doesn't turn it over. A flat picture turns with the phone again.

Games still fill both eyes as before. With the lens correction done by this PC, or VR off,
Gazeora's window streams like a game.

## 3D from depth

**3D from the game's depth** gives each eye its own view. The hook finds the game's main depth
buffer (the one with the most draws in the back buffer's shape) and copies it beside each
frame; Gazeora then shifts every pixel sideways by its depth, in opposite directions for the two
eyes, like a pair of eyes converged on the subject in the middle of the view (the character, in
a third-person game): the subject sits at the screen and stays put, farther things go back,
nearer ones come out. It works out on its own whether the game stores depth reversed (1 near,
as most games now do) or not, and follows the subject's depth, eased over a few frames.

Where one eye sees behind something, the game drew nothing; the background is stretched over
the gap. Near objects' depth is widened by a couple of pixels first, so their antialiased edges
move with them instead of tearing off into the background.

- **3D depth** is how much the two views differ for things far away, as a share of an eye's
  width (0.012, about a degree in a Cardboard-style viewer, by default). More strains the eyes
  and widens the stretched strips beside near objects.
- **Pop-out** is how far things nearer than the subject may come out of the screen, as a share
  of the 3D depth.

The two views go out side by side in one frame, each squeezed to half the width (the same
frame size as 2D), and the app stretches each back for its eye. The game's details page shows
whether 3D is working. Notes:

- D3D11 and D3D12 games only. Other APIs, the idle picture, and games without a depth buffer in
  the back buffer's shape stay 2D.
- Things drawn over the scene without depth, such as the HUD, take the depth of what's behind
  them and bend with it.
- D3D12 games are followed through their depth views, so start them from Gazeora. A D3D12 game
  attached to while already running works only if it moves its depth buffer out of the depth
  write state each frame, as deferred renderers do.

## Head tracking

While a game is in both eyes, turning your head turns the game's camera. The Android app reads
the phone's rotation sensor 200 times a second (gyroscope and accelerometer, without the compass,
which metal nearby pulls around) and sends where your head points to the PC, on the USB cable or
over UDP port 8557 like touches. Gazeora turns each change into mouse movement for the game in
front, the way mouse-look games (first and third person) turn their camera. Only the change
counts, so the camera starts from wherever it looks; turn with the mouse or a stick as well, and
both add up.

Set it in **Controls → Head tracking**:

- **The switch** turns it on and off (on until switched off, remembered).
- **Mouse sensitivity** is how far the mouse moves for each degree your head turns. Games
  differ, and so do their own mouse sensitivity settings: turn your head a quarter turn and raise
  it if the game turned less, lower it if more. 20 by default.
- **Controller: the game's turn at full stick** and **the game's stick dead zone**, for playing
  with a controller (below). 180° a second and 15% by default.

### With a controller

Games switch their button prompts and controls to whichever you used last, so mouse movement
from your head would keep pulling a game you play with a controller over to keyboard and mouse.
Instead, while you're on the controller, your head turns go through it: Gazeora's hook in the
game moves the controller's right stick as the game reads it, on top of your own thumb, and the
game never sees a mouse. Touch the keyboard or mouse and head turns go back to the mouse; touch
the controller and they come back to it.

It works with PlayStation controllers on USB (DualSense, DualSense Edge, DualShock 4) that the
game reads itself, and with Xbox-style controllers (XInput), including PlayStation controllers
Steam Input passes on as one. The game has to be captured (**Capture on launch**), so the hook is
inside it; the card says how the game reads your controller once it has.

A stick sets how fast the camera turns, not where it points, so the stick is pushed just enough
to turn the camera as far as your head by the game's next read of it. That depends on how fast
the game turns at full stick and how much of the stick's middle it ignores, which differ by game
and by its own sensitivity settings: if the game turns less than your head, lower the turn;
more, raise it; if small head turns go missing, raise the dead zone; if the camera twitches,
lower it. Games that speed the camera up the longer the stick is held follow less closely.

The card says whether head turns are coming from the phone and reaching a game, and so does the
stream overlay's **Head tracking** detail, from inside the headset: the head's yaw and pitch as
the phone sends them, how many poses arrive a second (about 200), and whether they're turning
the game. The [test app](tools.md#test-app) looks around with the mouse, for trying it without
a game. Notes:

- Only a game launched from Gazeora, and only while it's the window in front: switch to another
  window and the desktop's mouse is left alone. The game needn't be captured: with capture off,
  the headset shows Gazeora's window while your head still turns the game.
- The phone sends head poses while it does the lens correction (**Lens correction done by →
  The phone**, the default); with it done by this PC, the phone can't tell it's in a viewer.
- Roll (tilting your head to the side) isn't passed on: a mouse can't tilt a camera.
- The camera follows the head after the stream's delay (a game frame, encoding, the network and
  decoding), so fast turns lag a little.
- Windows doesn't let a program move the mouse in a game running as administrator unless it runs
  as administrator too.
