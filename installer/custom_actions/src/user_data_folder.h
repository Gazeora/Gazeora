#pragma once

#include <string>

namespace gazeora::setup {

// The folder Gazeora keeps a user's library and settings in.
class UserDataFolder {
public:
    virtual ~UserDataFolder() = default;

    virtual std::wstring Path() const = 0;
    virtual bool Exists() const = 0;
    // Deletes it with everything in it; false if something stayed.
    virtual bool Remove() const = 0;
};

}  // namespace gazeora::setup
