# Development

Building, testing, benchmarks, formatting and releasing. For how the code is organized, see
[Architecture](architecture.md). For the contribution workflow, see
[CONTRIBUTING.md](../CONTRIBUTING.md).

## Building

### Prerequisites

- Windows. Configuring on another OS fails with *Gazeora is built for Windows only*.
- CMake 3.20 or newer.
- A C++17 compiler for the release you want (see [x64 and x86](#x64-and-x86)):
  - MSVC from Visual Studio 2022 or newer, with its x64 or x86 tools, or
  - MinGW-w64: a 64-bit toolchain for x64, a 32-bit one (such as MSYS2's
    `mingw-w64-i686-gcc`) for x86.
- Ninja, or a Visual Studio generator.
- An internet connection for the first configure. CMake downloads:

  | Dependency | Version | Used for |
  |---|---|---|
  | [MinHook](https://github.com/TsudaKageyu/minhook) | 1.3.3 | API hooking in the hook DLL |
  | [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.5 | The launcher UI |
  | [libusb](https://github.com/libusb/libusb) | 1.0.29 | The USB protocol |
  | [SDL](https://github.com/libsdl-org/SDL) | 3.4.18 | Game controllers |
  | [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | The library file |
  | [GoogleTest](https://github.com/google/googletest) | 1.15.2 | Tests and benchmarks |

- Optional: the .NET SDK (for the MSI), clang-format (for the `format` targets), and Python
  with [pre-commit](https://pre-commit.com).

### x64 and x86

The compiler decides the release: a 64-bit compiler builds the x64 release, a 32-bit compiler
the x86 release. Use a separate build folder for each. Configuring prints which release it is,
for example `Building the x64 release of Gazeora`. See
[Architecture](architecture.md#x64-and-x86-releases) for why.

### Configure and build

With MSVC, from a **Developer PowerShell for VS** (or after running `vcvarsall.bat x64` or
`vcvarsall.bat x86`), in the repository root. These are the commands CI runs, without the
overlay option:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build
```

With MinGW-w64, with the toolchain's `bin` folder on `PATH`:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Visual Studio generators also work (`-G "Visual Studio 17 2022" -A x64`, or `-A Win32` for
x86). They are multi-config, so outputs go to `build\bin\<config>\` and the build type is
chosen with `cmake --build build --config Release`.

### Build options

| Option | Default | Effect |
|---|---|---|
| `GAZEORA_STREAM_OVERLAY` | `ON` | Includes the [stream overlay](streaming.md#stream-overlay) in `Gazeora.exe`. CI and the released MSIs use `OFF` |
| `GAZEORA_BUILD_TESTS` | `ON` | Builds the unit tests |
| `GAZEORA_BUILD_BENCH` | `ON` | Builds `gazeora_bench.exe` |
| `GAZEORA_SIGN_CERT_FILE` | empty | A `.pfx` to Authenticode-sign the programs and the MSI. Its password is read from the `GAZEORA_SIGN_CERT_PASSWORD` environment variable |
| `GAZEORA_SIGN_CERT_THUMBPRINT` | empty | Instead of a file, the SHA-1 thumbprint of a certificate in the user's store |

Signing is off unless a certificate is given. See `cmake/CodeSigning.cmake`.

### Outputs

Everything goes into `build\bin\`. Keep the executables and `gazeora_hook.dll` in the same
folder, because they load the hook from their own folder. With MSVC, the build also copies
`vcomp140.dll` (the OpenMP runtime) from Visual Studio's redistributable files, so the programs
run without the Visual C++ Redistributable. If it is not found, configuring warns.

| Output | Description |
|---|---|
| `Gazeora.exe` | The launcher and stream server |
| `GazeoraCLI.exe` | [Command-line](command-line.md) front end |
| `gazeora_hook.dll` | The capture hook, injected into games |
| `gazeora_testapp.exe` | [Test app](tools.md#test-app) |
| `gazeora_stream_probe.exe` | [Stream probe](tools.md#stream-probe) |
| `gazeora_*_tests.exe` | Unit tests |
| `gazeora_bench.exe` | Benchmarks |

### Installing a build

`cmake --install` copies what Gazeora needs to run into `bin\` under the install prefix:
`Gazeora.exe`, `GazeoraCLI.exe`, `gazeora_hook.dll` and, with MSVC, `vcomp140.dll`. The README and
the license files go into `share\doc\Gazeora`, and the guides into `share\doc\Gazeora\docs`,
so the README's links to them work in the installed copy. The test app and the
stream probe are in the `Tools` component, installed only when asked for. Tests and benchmarks
are never installed.

```powershell
cmake --install build                            # Program Files\Gazeora (x86: Program Files (x86)\Gazeora), as administrator
cmake --install build --prefix D:\Apps\Gazeora   # another folder
cmake --install build --component Tools          # also the test app and the stream probe
```

`cmake --install` does not add shortcuts, PATH entries or a firewall rule. For those, build
the MSI: see [Installer](installer.md).

```powershell
cmake --build build --target gazeora_msi
```

## Testing

From the repository root, after building:

```powershell
ctest --test-dir build --output-on-failure
```

The tests use GoogleTest and run Direct3D on WARP, so no GPU is needed. CI runs them with
`--repeat until-pass:3`. They are split into several executables because the hook and the host
each attach to a per-process shared memory block:

| Suite | Covers |
|---|---|
| `gazeora_hook_tests` | Hook core against a fake host; head turns as a controller's right stick; controller reports |
| `gazeora_host_tests` | Capture client, injector, launcher, the test app captured end to end with the real hook, frame read-back, GPU scaling and the VR pass, NV12 conversion, H.264 and H.265 encoding, RTP round trips, the RTSP server, pointer and head pose input |
| `gazeora_transport_tests` | The real hook and host exchanging frames in one process |
| `gazeora_library_tests` | The game library |
| `gazeora_installer_tests` | The MSI's custom actions, against a fake installer session |
| `gazeora_input_tests` | Game controllers: stick filtering, the polling session, SDL without a window |
| `gazeora_update_tests` | Release versions and channels, the release feed, and the MSI download's size and SHA-256 checks, against a fake web server |
| `gazeora_app_tests` | The launcher's controller navigation and on-screen keyboard on a headless ImGui, `controls.ini`, head tracking, and the update steps |

Some tests depend on what the machine offers (for example a hardware encoder) and may skip
themselves when it is missing.

To test both releases on one PC, build both. The tests and the test app of each run as that
release.

## Benchmarks

`gazeora_bench.exe` times each stage on this PC's GPU and encoders, and prints the mean,
median, 99th percentile and worst case. It is not part of `ctest`. Run it from a Release build:

```powershell
cd build\bin
.\gazeora_bench.exe                                   # everything
.\gazeora_bench.exe --gtest_filter=Encode.*:Stream.*  # some areas
```

| Area | Measures |
|---|---|
| `Convert` | The hook's pixel conversion, and BGRA8 to NV12 at 720p, 1080p and 1440p |
| `Capture` | The frame transport, `TextureReader` at several sizes and modes, BMP screenshots, and the real hook in the test app for each graphics API (opens a window per API) |
| `Encode` | Each codec on the hardware and software encoder, the whole `VideoPipeline`, and a 1080p frame to packets |
| `Rtp`, `Rtcp`, `Rtsp`, `Bitrate` | Packetizing, RTCP reports, RTSP messages, the bitrate controller |
| `Stream` | `FrameMailbox`, `PacketSendQueue` pacing, and the whole server at 30 and 60 fps to a loopback UDP player |
| `Library` | Loading, saving and searching a 500-game library |

Results depend on the hardware and are not recorded in the repository.

## Formatting and checks

Code is formatted with clang-format using `.clang-format` at the root:

```powershell
cmake --build build --target format         # format everything
cmake --build build --target format-check   # fail if anything is not formatted
```

The targets are off if clang-format is not found. The [pre-commit](https://pre-commit.com)
configuration runs clang-format, whitespace and line-ending checks (LF), YAML checks, and
`tools/license_headers.py`, which checks that every source file has the copyright and SPDX
license header:

```powershell
pip install pre-commit
pre-commit install
```

## Releasing

The version is in the `VERSION` file at the root, as `MAJOR.MINOR.PATCH` (`1.1.0`) or
`MAJOR.MINOR.PATCH-rc.N` for a release candidate (`1.1.0-rc.1`). It is always shown with a `v`
prefix: `v1.1.0`, `v1.1.0-rc.1`. It reaches the version info of `Gazeora.exe`, the About card
on the Settings page, and the MSI's file name. Windows Installer compares only the three
numbers, so `v1.1.0-rc.1` and `v1.1.0` install as the same version and either replaces the
other.

To release:

1. Set `VERSION`, for example to `1.1.0-rc.1`, and commit it (`chore: release v1.1.0-rc.1`).
2. Publish a GitHub release titled `Gazeora v1.1.0-rc.1` with a new tag `v1.1.0-rc.1`. Publish
   a release candidate as a **pre-release** and a stable version as a normal release.
3. CI (`.github/workflows/ci.yml`) builds and tests both releases and attaches
   `Gazeora-v1.1.0-rc.1-x64.msi` and `Gazeora-v1.1.0-rc.1-x86.msi` to the release. It fails if
   the tag does not match `VERSION`, or if the pre-release flag does not match the version.
4. CI then sends the release to [gazeora.github.io](https://gazeora.github.io), which lists it
   on its download page. Editing or deleting the release later updates the site too
   (`.github/workflows/website.yml`). Both need the `GAZEORA_SITE_TOKEN` repository secret: a
   fine-grained token with **Contents: Read and write** access to `Gazeora/gazeora.github.io`
   only.

When a candidate is good, set `VERSION` to the stable version and release it the same way.

### Weekly pre-release

Every Monday at 00:00 UTC, or by hand from **Actions → Weekly pre-release → Run workflow**,
`.github/workflows/weekly-release.yml` compares `master` with the highest release so far. If
anything other than `VERSION` has changed, it releases the next patch as a candidate: after
`v1.0.1-rc.1` comes `v1.0.2-rc.1`, and after a stable `v1.0.3` comes `v1.0.4-rc.1`. If nothing
has changed, there is no release that week.

`master` only accepts pull requests, so the workflow makes its own commit on top of `master`
that sets `VERSION`, tags it, and runs CI on the tag. That commit stays off `master`, so
`VERSION` on `master` keeps the last version set by hand. If the build fails, nothing is
published (only the tag remains), and the next week tags the same version again from the newer
`master`.

### CI artifacts

CI also runs on every pull request and by hand (`workflow_dispatch`). The MSIs of each run are
kept as workflow artifacts for 7 days, or 90 days for a release run. MSIs attached to a release
do not expire.
