// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora::app {

// What a key of the on-screen keyboard can do.
class IKeyboardActions {
public:
    virtual ~IKeyboardActions() = default;

    virtual void Type(char32_t character) = 0;
    virtual void Erase() = 0;
    virtual void ToggleShift() = 0;
    virtual void ToggleSymbols() = 0;
    virtual void Finish() = 0;
    virtual bool Shifted() const = 0;
};

}  // namespace gazeora::app
