# Command line

```
GazeoraCLI list
GazeoraCLI add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] [--capture]
GazeoraCLI remove <id | name>
GazeoraCLI launch <id | name> [--capture]
GazeoraCLI attach <pid | process.exe> [--depth] [--save-after SECONDS]
GazeoraCLI run <game.exe> [--args A] [--depth] [--save-after SECONDS]
```

While attached, press **S** to save the current frame as a BMP and **Q** to quit. `run` starts
an exe hooked from its first instruction, like a game launched from the library, without
adding it there. `--depth` also asks for the game's depth buffer, reports how many frames came
with it, and saves the frame's 3D pair (`frame_N_3d.bmp`) with each frame. `--save-after`
saves once, that many seconds in, and quits. The CLI does not stream; streaming is part of
`Gazeora.exe`.
