# Command line

`GazeoraCLI.exe` manages the same game library as `Gazeora.exe` and can capture a game from a
console. It does not stream; streaming is part of `Gazeora.exe`. The MSI's **Add GazeoraCLI to
PATH** feature lets you run it from any new command prompt.

```text
GazeoraCLI list
GazeoraCLI add <game.exe | shortcut.lnk> [--name N] [--args A] [--workdir D] [--capture]
GazeoraCLI remove <id | name>
GazeoraCLI launch <id | name> [--capture]
GazeoraCLI attach <pid | process.exe> [--depth] [--save-after SECONDS]
GazeoraCLI run <game.exe> [--args A] [--depth] [--save-after SECONDS]
GazeoraCLI help
```

## Commands

| Command | Does |
|---|---|
| `list` | Lists the library: ID, name, play time, CLI capture flag, launch method and path |
| `add` | Adds a game. `--name`, `--args` and `--workdir` set its name, launch arguments and working directory. `--capture` sets the game's CLI capture flag |
| `remove` | Removes a game by ID or name |
| `launch` | Starts a game from the library. With capture (the game's flag or `--capture`), the hook is injected and the console receives its frames. Without it, the game just starts |
| `attach` | Injects the hook into a running process, by process ID or exe name |
| `run` | Starts an exe with the hook injected from its first instruction, as `Gazeora.exe` does, without adding it to the library. `--args` passes launch arguments |
| `help` | Prints the usage |

A name must match a single game. If it does not, the command fails and suggests `list`.

The capture flag (`captureOnLaunch` in the library) is used only by `GazeoraCLI launch`.
`Gazeora.exe` always captures the games it launches.

## While capturing

| Key | Action |
|---|---|
| `S` | Saves the current frame as `frame_N.bmp` in the current folder |
| `Q` | Quits |

- `--depth` also asks for the game's depth buffer, reports how many frames came with it, and
  saves the frame's 3D pair (`frame_N_3d.bmp`) with each saved frame. See
  [3D from depth](vr-viewer.md#3d-from-depth).
- `--save-after SECONDS` saves one frame after that many seconds, then quits.

The CLI must be the same release (x64 or x86) as the game. See
[Getting started](getting-started.md#1-pick-a-release).
