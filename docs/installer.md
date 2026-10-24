# Installer

Gazeora ships as one MSI per release: `Gazeora-<version>-x64.msi` for 64-bit games and
`Gazeora-<version>-x86.msi` for 32-bit ones. The two install side by side, as "Gazeora" and
"Gazeora (32-bit)".

## Building the MSI

The MSI is built with [WiX](https://wixtoolset.org/) 5, which comes as a .NET tool pinned in
`dotnet-tools.json`. Building needs the .NET SDK (6 or newer); the build fetches WiX and its
extensions on first use.

```powershell
cmake --build build --target gazeora_msi    # writes build\Gazeora-1.0.0-x64.msi
```

The target builds what the MSI holds, stages it with `cmake --install` (the `Runtime` and
`Tools` components) under `build\installer\stage`, and packs that. So the MSI holds exactly
what the install rules list (see [Installing](../README.md#installing)). The version is the
project version in `CMakeLists.txt`. With a signing certificate set (see `cmake/CodeSigning.cmake`),
the programs, the custom-action DLL and the MSI are all signed.

To check the result against Windows Installer's rules:

```powershell
dotnet wix msi validate build\Gazeora-1.0.0-x64.msi
```

It warns about ICE61 only, because a rebuild of the same version may replace the installed one.

## What it installs

Everything goes into `Program Files\Gazeora` (`Program Files (x86)\Gazeora` for x86), as
administrator. The wizard's feature tree has:

| Feature | Default | What it does |
|---|---|---|
| Gazeora | Always | `Uninstall.exe`, the programs, the hook, ONNX Runtime and the models in `bin\`, the guides and licenses in `share\doc\Gazeora`, and a Start menu shortcut |
| Desktop shortcut | On | A shortcut on the desktop |
| Add GazeoraCLI to PATH | On | Adds `bin\` to the system PATH, so `GazeoraCLI` runs from any new command prompt |
| Open with Gazeora in Explorer | On | Right-click a game's `.exe` or shortcut and pick **Open with Gazeora** to add it to the library. On Windows 11 it's under **Show more options**. If Gazeora is already open, the game goes to that window |
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

## Silent installs

Feature IDs and properties for `msiexec`:

```powershell
msiexec /i Gazeora-1.0.0-x64.msi /qn                                   # everything that's on by default
msiexec /i Gazeora-1.0.0-x64.msi /qn ADDLOCAL=Main,CommandLinePath     # only these features
msiexec /i Gazeora-1.0.0-x64.msi /qn INSTALLFOLDER="D:\Apps\Gazeora"   # another folder
msiexec /x Gazeora-1.0.0-x64.msi /qn REMOVEUSERDATA=1                  # uninstall, deleting the user's data
```

The feature IDs are `Main`, `DesktopShortcut`, `CommandLinePath`, `OpenWith`, `Firewall` and
`Tools`. `REMOVEUSERDATA=1` deletes `%APPDATA%\Gazeora` without asking and `REMOVEUSERDATA=0`
keeps it. Without the property, a silent uninstall keeps the data.

## Layout

```
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
