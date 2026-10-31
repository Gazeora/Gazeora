# Installer

Gazeora ships as one MSI per release: `Gazeora-v<version>-x64.msi` for 64-bit games and
`Gazeora-v<version>-x86.msi` for 32-bit ones. The two install side by side, as "Gazeora" and
"Gazeora (32-bit)".

## Building the MSI

The MSI is built with [WiX](https://wixtoolset.org/) 5, which comes as a .NET tool pinned in
`dotnet-tools.json`. Building needs the .NET SDK; without it, configuring turns the `gazeora_msi` target off. The
build fetches WiX and its extensions on first use.

```powershell
cmake --build build --target gazeora_msi    # writes build\Gazeora-v1.0.0-x64.msi
```

The target builds what the MSI holds, stages it with `cmake --install` (the `Runtime` and
`Tools` components) under `build\installer\stage`, and packs that. So the MSI holds exactly
what the install rules list (see [Installing a build](development.md#installing-a-build)). The version comes
from the `VERSION` file (see [Releasing](development.md#releasing)). With a signing certificate
set (see `cmake/CodeSigning.cmake`), the programs, the custom-action DLL and the MSI are all
signed.

To check the result against Windows Installer's rules:

```powershell
dotnet wix msi validate build\Gazeora-v1.0.0-x64.msi
```

It warns about ICE61 only, because a rebuild of the same version may replace the installed one.

## What it installs

Everything goes into `Program Files\Gazeora` (`Program Files (x86)\Gazeora` for x86), as
administrator. The wizard's feature tree has:

| Feature | Default | What it does |
|---|---|---|
| Gazeora | Always | `Uninstall.exe`, the programs and the hook in `bin\`, the guides and licenses in `share\doc\Gazeora`, and a Start menu shortcut |
| Desktop shortcut | On | A shortcut on the desktop |
| Add GazeoraCLI to PATH | On | Adds `bin\` to the system PATH, so `GazeoraCLI` runs from any new command prompt |
| Open with Gazeora in Explorer | On | Right-click a game's `.exe` or shortcut and pick **Open with Gazeora** (**Open with Gazeora (32-bit)** for the x86 release) to add it to the library. On Windows 11 it's under **Show more options**. If Gazeora is already open, the game goes to that window |
| Allow the stream through the firewall | On | A Windows Firewall rule for `Gazeora.exe` on **private** networks, from the local subnet only. On a network Windows calls public, the stream stays blocked: make the network private in Windows' network settings |
| Developer tools | Off | `gazeora_testapp.exe` and `gazeora_stream_probe.exe` ([Tools](tools.md)) |

## Uninstalling

Uninstall from **Settings → Apps**, or run `Uninstall.exe` in the install folder, which starts
the same uninstall. Uninstalling removes everything the features added, and also:

- **The USB driver.** If Gazeora installed its USB driver ([USB setup](streaming.md#usb-setup)),
  the driver package and the certificate Gazeora trusted for it are removed. Phones get their
  own driver (file transfer) back. A phone that is streaming at that moment gets it back when
  it's plugged in again. On 64-bit Windows, the x86 MSI leaves the driver to the x64 one.
- **Your data, if you want.** Windows asks whether to delete your game library and settings
  (`%APPDATA%\Gazeora`, see [Configuration files](configuration.md)). Only the data of the
  user who uninstalls is removed.

Upgrading to a newer MSI does neither: the library, the settings and the driver stay.

## Updating

**Settings → Updates** in `Gazeora.exe` finds and installs newer releases.

- **Checking.** At each start, Gazeora reads the list of Windows releases that
  [gazeora.github.io](https://gazeora.github.io) publishes, over HTTPS. Turn off **Check for
  updates when Gazeora starts** to stop that (saved in [`updates.ini`](configuration.md#updatesini))
  and use **Check now** instead. Nothing about you or the PC is sent beyond what any web
  request carries: the PC's IP address, and Gazeora's version in the user agent.
- **What's offered.** A release (`v1.2.0`) is offered newer releases only. A release candidate
  (`v1.2.0-rc.1`) is also offered newer release candidates. A release is offered only if it has an
  MSI for the running build (x64 or x86).
- **Installing.** **Download and install** downloads that MSI to `%TEMP%\Gazeora\updates` and
  checks its size and SHA-256 against what the release published. If either doesn't match, the
  file is deleted and nothing runs. Otherwise Gazeora opens the MSI's wizard and closes itself so
  its files can be replaced. The wizard asks for administrator permission, as a first install
  does. The button is disabled while a game launched from Gazeora runs.
- **If it fails.** **Try again** repeats the step. **Download it yourself** opens the release's
  page on GitHub.

The wizard starts from the default install folder. If Gazeora was installed somewhere else, pick
that folder again in the wizard.

## Silent installs

Feature IDs and properties for `msiexec`:

```powershell
msiexec /i Gazeora-v1.0.0-x64.msi /qn                                   # everything that's on by default
msiexec /i Gazeora-v1.0.0-x64.msi /qn ADDLOCAL=Main,CommandLinePath     # only these features
msiexec /i Gazeora-v1.0.0-x64.msi /qn INSTALLFOLDER="D:\Apps\Gazeora"   # another folder
msiexec /x Gazeora-v1.0.0-x64.msi /qn REMOVEUSERDATA=1                  # uninstall, deleting the user's data
```

The feature IDs are `Main`, `DesktopShortcut`, `CommandLinePath`, `OpenWith`, `Firewall` and
`Tools`. `REMOVEUSERDATA=1` deletes `%APPDATA%\Gazeora` without asking and `REMOVEUSERDATA=0`
keeps it. Without the property, a silent uninstall keeps the data.

## Layout

```text
installer/
  CMakeLists.txt        The gazeora_msi target
  cmake/TextToRtf.cmake NOTICE and LICENSE as the license page's RTF
  custom_actions/       gazeora_setup_actions.dll: asks about %APPDATA%\Gazeora on uninstall
  uninstaller/          Uninstall.exe: finds the installed MSI by its upgrade code, uninstalls it
  branding/             The wizard's pictures in Gazeora's colors, and make_branding.py, which
                        draws them from the app icon (needs Pillow)
  wix/
    Release.wxi           The Explorer verb, which differs between x64 and x86
    Package.wxs           The package, its features and the wizard
    Files.wxs             The install folders, and the staged files gathered into them
    Shortcuts.wxs         Start menu and desktop
    System.wxs            PATH, the Explorer verb and the firewall rule
    CustomActions.wxs     USB driver and user data clean-up, and starting Gazeora at the end
```

Never change the upgrade codes in `installer/CMakeLists.txt`: a new MSI replaces an installed
one only when the codes match.
