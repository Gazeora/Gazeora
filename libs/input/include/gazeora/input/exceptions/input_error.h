#pragma once

#include <stdexcept>
#include <string>

#include "gazeora/common/text/utf8.h"

namespace gazeora::input {

// An input device couldn't be used; the message says what, for people.
class InputError : public std::runtime_error {
public:
    explicit InputError(std::wstring message)
        : std::runtime_error(Utf8::FromWide(message)), m_message(std::move(message)) {}

    const std::wstring& Message() const { return m_message; }

private:
    std::wstring m_message;
};

}  // namespace gazeora::input
