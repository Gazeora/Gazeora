#pragma once

#include <stdexcept>
#include <string>

#include "gazeora/common/text/utf8.h"

namespace gazeora {

// The library couldn't be read or written; the message says what and where.
class LibraryStorageError : public std::runtime_error {
public:
    explicit LibraryStorageError(std::wstring message)
        : std::runtime_error(Utf8::FromWide(message)), m_message(std::move(message)) {}

    const std::wstring& Message() const { return m_message; }

private:
    std::wstring m_message;
};

}  // namespace gazeora
