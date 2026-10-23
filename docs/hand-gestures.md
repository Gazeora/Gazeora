# Hand gestures

**Controls → Hand gestures** lets a webcam on the PC drive the mouse pointer, alongside the
mouse and keyboard, which keep working as always. It's for playing in the headset: it turns
on by itself when the Gazeora app on a phone watches the stream with VR on (Settings), and
you can turn it off again for that connection. While no phone watches in VR (or only other
players, such as VLC, do), it's off and can't be turned on. The app is told apart by the
User-Agent it sends over RTSP and HTTP (`Gazeora Android`); over UDP and USB only the app
speaks Gazeora's format. Pick the camera; the card shows the hand it sees, live.

Your hands work like a touch screen you don't touch. The right hand points; the left hand
only signals:

- **Move (right hand):** palm toward the screen, index finger up. The pointer goes where your
  fingertip is in an air box in front of you that stands for the screen, so there's nothing
  to lift and put back, and every corner is a reach away. The box is placed when your hand
  is first seen, so the pointer doesn't jump, and slides along if your fingertip goes past
  an edge, so moving back moves away from it at once. The camera sees the hand best with
  the palm toward it; a finger pointing straight at the camera is hard for it to place.
- **Fine-tuning:** small, slow moves nudge the pointer with pointer precision (0.3 of the
  fingertip's move, rising to all of it at 2 palm lengths a second), up to 3% of the screen
  from where the fingertip is; quick moves bring it back under the fingertip. Moves are
  measured in palm lengths, so how near the camera you are doesn't matter, and the
  fingertip has to move a millimetre before the pointer does, so a still hand can't make it
  drift.
- **Hold and click (left index up):** raise your left index finger, the others folded, and
  the pointer holds still. Then fold your right index finger (halfway is enough) and open
  it again to click, as often as you like. The left index has to be up for 0.2 s first, so
  a few misread pictures don't stop the pointer; a fold held longer than a second isn't a
  click; and the right index has to have been up while holding before a fold counts, so
  raising the left index with the right one curled doesn't click.
- **Scroll (left pinch):** pinch your left thumb and index fingertips together, and your
  right index finger scrolls instead of moving the pointer: the page follows it, as on a
  touch screen (moving up scrolls down, moving right scrolls left). Each stroke scrolls the
  way it first went, vertically or sideways; pause a moment and the next stroke picks
  again. A palm's length of fingertip move turns the wheel five notches. The pinch has to
  last a tenth of a second, and is read from the fingertips' distance in the picture. A
  fist brings them close too, but its thumb lies across the index knuckle and its index tip
  curls into the palm; with either, it's no pinch.
- **Back to moving:** open the pinch, or lower the left hand (gone for over 0.3 s), and the
  right index moves the pointer again, from where it held.
- **The left hand never moves the pointer,** and means nothing else: two fingers up, an open
  hand or a fist are ignored. The hands are told apart by the landmark model's guess at
  which hand it sees, averaged over the pictures each is followed (your right hand reads
  as right in 98 to 100% of pictures on recordings). Alone, a left hand leaves the pointer
  where it is; two hands both taken for right, the one further to your left is the left.
  Gazeora follows up to two hands: while it follows one, it looks for the other every
  third picture.
- **Back under the finger:** hold all your right fingers open for half a second, and the
  pointer goes right where your fingertip is in the box, undoing fine-tuning and wherever
  the mouse put it. Seen from the knuckles the model draws folded fingers straight for up
  to about a third of a second at a time, so a shorter glimpse of an open hand does nothing.
- **Colors:** the hand preview and the status line show each gesture in its own color:
  violet moving (right index), amber holding (right index and left index), red folding to
  click, cyan scrolling (right index and the left pinch), green an open hand (every finger).
- A finger counts as folded by its joints, not its direction: a straight finger pointing
  down is still straight. Folding halfway counts. It counts as up only if it also reaches
  out from its knuckle in the model's 3D guess at the hand, which catches a folded finger
  hidden behind the others (seen from the side) that the model draws straight.
- When a hand is lost, the palm detector's three likeliest palms get a look, not just the
  first: a palm-like print on a shirt can outscore a hand seen from an unusual side, and
  the landmark model tells them apart.
- **Pointer speed** sets the box's size: the higher, the shorter the move that crosses the
  screen. At 5, the default, about 20 cm (two and a half palm lengths).
- The mouse keeps working alongside: the pointer stays where the mouse puts it until your
  fingertip moves, then glides back under it. Losing sight of a hand for a moment (under
  0.3 s) doesn't interrupt a gesture.

The pointer is moved as a mouse would move it, in the main screen's coordinates, so it works
in menus and games that show a cursor. Games that read raw mouse movement for the camera
don't respond to it yet. Windows won't let it reach a game running as administrator unless
Gazeora runs as administrator too. Tracking runs on the CPU with two threads, at the camera's
frame rate (Gazeora picks its fastest mode, up to 30 fps), and stops when it's switched off.
