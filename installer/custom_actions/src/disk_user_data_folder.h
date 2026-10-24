#pragma once

#include <filesystem>
#include <utility>

#include "user_data_folder.h"

namespace gazeora::setup {

class DiskUserDataFolder : public UserDataFolder {
public:
    explicit DiskUserDataFolder(std::filesystem::path path) : m_path(std::move(path)) {}

    std::wstring Path() const override { return m_path.wstring(); }
    bool Exists() const override;
    bool Remove() const override;

private:
    std::filesystem::path m_path;
};

}  // namespace gazeora::setup
