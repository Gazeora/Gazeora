// The Windows driver the USB protocol needs: WinUSB, so libusb and Gazeora can reach the phone.
// On the accessory interface of a phone in USB accessory mode, which Windows has no driver for
// by itself; and, for phones without USB debugging, on the phone in its normal mode, so
// Gazeora can ask it to switch (its file transfer driver can't), at the cost of Windows'
// file transfer with it until that's restored.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace gazeora::app {

// Gazeora.exe run with one of these (a pipe in the process that started it, for what went
// wrong; then any hardware IDs) installs the driver or restores file transfer, and exits. The
// *Elevated functions run it that way, as an administrator.
constexpr wchar_t kInstallUsbDriverArgument[] = L"--install-usb-driver";
constexpr wchar_t kRestoreFileTransferArgument[] = L"--restore-file-transfer";
// Run by the uninstaller, already as an administrator.
constexpr wchar_t kUninstallUsbDriverArgument[] = L"--uninstall-usb-driver";

// Installs the driver for the accessory interface of any phone (the accessory IDs are
// Google's, the same for all), and for the phones with `phoneIds` (UsbPhone::hardwareId) on
// top of those added before. Needs an administrator. A driver package is only trusted if
// signed, so this signs its catalog with a certificate made for the purpose, trusts that
// certificate on this PC, and deletes its key once the catalog is signed, so it can't sign
// anything else. False with `error` set if it can't.
bool InstallUsbDriver(const std::vector<std::wstring>& phoneIds, std::wstring& error);

// Takes the driver off the phones it was installed on, so Windows' file transfer works with
// them again; the accessory keeps it. Needs an administrator.
bool RestoreFileTransfer(std::wstring& error);

// Removes every driver package Gazeora installed and its certificate, and gives the phones
// their own driver back. Nothing to do, and true, if none is installed. Needs an administrator.
bool UninstallUsbDriver(std::wstring& error);

// The phones the installed driver covers (hardware IDs); no administrator needed.
std::vector<std::wstring> InstalledPhoneIds();

// Run Gazeora as an administrator to do the above (Windows asks the user first), and wait for
// it. False with `error` set if it failed or the user said no.
bool InstallUsbDriverElevated(HWND owner, const std::vector<std::wstring>& phoneIds,
                              std::wstring& error);
bool RestoreFileTransferElevated(HWND owner, std::wstring& error);

// For main: when the command line asks for one of the above, does it, writes what went wrong
// to that pipe and returns the exit code. -1 when the command line doesn't ask.
int RunUsbDriverCommand();

}  // namespace gazeora::app
