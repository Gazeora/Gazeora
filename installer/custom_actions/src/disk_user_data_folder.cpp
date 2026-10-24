#include "disk_user_data_folder.h"

#include <system_error>

namespace gazeora::setup {

bool DiskUserDataFolder::Exists() const {
    std::error_code error;
    return std::filesystem::is_directory(m_path, error);
}

bool DiskUserDataFolder::Remove() const {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
    return !error;
}

}  // namespace gazeora::setup
