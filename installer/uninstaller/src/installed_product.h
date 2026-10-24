#pragma once

#include <optional>
#include <string>

namespace gazeora::uninstall {

// The installed Gazeora MSI. Its product code changes with every build; the upgrade code doesn't.
class InstalledProduct {
public:
    explicit InstalledProduct(std::wstring upgradeCode) : m_upgradeCode(std::move(upgradeCode)) {}

    // The installed product's code, as {GUID}; none if this release isn't installed.
    std::optional<std::wstring> ProductCode() const;

private:
    std::wstring m_upgradeCode;
};

}  // namespace gazeora::uninstall
