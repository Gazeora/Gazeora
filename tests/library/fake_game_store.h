// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#pragma once

#include <string>

#include "gazeora/host/launch/game_store.h"

namespace gazeora {

// A store that owns the one exe it's given, under a fixed app id, and launches nothing.
class FakeGameStore final : public GameStore {
public:
    FakeGameStore(std::string id, std::wstring ownedExe, std::wstring appId)
        : m_id(std::move(id)),
          m_name(m_id.begin(), m_id.end()),
          m_ownedExe(std::move(ownedExe)),
          m_appId(std::move(appId)) {}

    const std::string& Id() const override { return m_id; }
    const std::wstring& Name() const override { return m_name; }
    std::wstring AppIdFor(const std::wstring& exePath) const override {
        ++m_lookups;
        return exePath == m_ownedExe ? m_appId : std::wstring();
    }
    bool Launch(const std::wstring&, std::wstring&) const override { return true; }

    int Lookups() const { return m_lookups; }

private:
    const std::string m_id;
    const std::wstring m_name;  // the id; ids are ASCII
    const std::wstring m_ownedExe;
    const std::wstring m_appId;
    mutable int m_lookups = 0;
};

}  // namespace gazeora
