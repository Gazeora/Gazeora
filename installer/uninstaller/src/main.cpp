// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Uninstall.exe, in Gazeora's install folder: starts the MSI's uninstall, as Settings > Apps does.

#include <windows.h>

#include "installed_product.h"
#include "release_info.h"  // generated: GAZEORA_PRODUCT_NAME, GAZEORA_UPGRADE_CODE
#include "uninstall_launcher.h"

namespace {

constexpr wchar_t kTitle[] = L"Uninstall " GAZEORA_PRODUCT_NAME;
constexpr wchar_t kNotInstalled[] = GAZEORA_PRODUCT_NAME L" isn't installed.";
constexpr wchar_t kNotStarted[] =
    L"Windows Installer didn't start. "
    L"Uninstall " GAZEORA_PRODUCT_NAME L" from Settings > Apps instead.";

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace gazeora::uninstall;
    const auto productCode = InstalledProduct(GAZEORA_UPGRADE_CODE).ProductCode();
    if (!productCode) {
        MessageBoxW(nullptr, kNotInstalled, kTitle, MB_ICONINFORMATION);
        return 1;
    }
    if (!UninstallLauncher().Start(*productCode)) {
        MessageBoxW(nullptr, kNotStarted, kTitle, MB_ICONERROR);
        return 1;
    }
    return 0;
}
