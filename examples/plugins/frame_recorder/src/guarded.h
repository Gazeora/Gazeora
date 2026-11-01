// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace frame_recorder {

// Runs the body of a function Gazeora calls so no exception leaves it: one that crossed into
// Gazeora would end the program.
class Guarded {
public:
    template <typename Result, typename Body>
    static Result Call(Result fallback, Body&& body) noexcept {
        try {
            return body();
        } catch (...) {
            return fallback;
        }
    }

    template <typename Body>
    static void Run(Body&& body) noexcept {
        try {
            body();
        } catch (...) {
        }
    }
};

}  // namespace frame_recorder
