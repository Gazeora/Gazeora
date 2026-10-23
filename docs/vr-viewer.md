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

There is no head tracking yet.
