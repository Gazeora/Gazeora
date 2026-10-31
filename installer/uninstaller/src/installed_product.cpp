// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "installed_product.h"

#include <windows.h>
#include <msi.h>

namespace gazeora::uninstall {

namespace {

constexpr DWORD kFirstProduct = 0;
constexpr size_t kGuidLength = 38;  // {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}

}  // namespace

std::optional<std::wstring> InstalledProduct::ProductCode() const {
    wchar_t code[kGuidLength + 1] = {};
    if (MsiEnumRelatedProductsW(m_upgradeCode.c_str(), 0, kFirstProduct, code) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::wstring(code);
}

}  // namespace gazeora::uninstall
