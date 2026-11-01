// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

namespace gazeora {

// RAII: an object a plugin made for Gazeora (an encoder, a controller driver), destroyed with
// its own destroy unless Release() says an adapter took it over. So it isn't leaked when it's
// refused, or when making its adapter throws.
template <typename Struct>
class PluginObjectGuard {
public:
    explicit PluginObjectGuard(const Struct& object) : m_object(&object) {}
    PluginObjectGuard(const PluginObjectGuard&) = delete;
    PluginObjectGuard& operator=(const PluginObjectGuard&) = delete;
    ~PluginObjectGuard() {
        if (m_object && m_object->destroy) {
            m_object->destroy(m_object->self);
        }
    }

    void Release() { m_object = nullptr; }

private:
    const Struct* m_object;
};

}  // namespace gazeora
