// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "hook/fake_host.h"

#include <cstdlib>

#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::test {

FakeHost& FakeHost::Get() {
    static FakeHost host;
    return host;
}

FakeHost::FakeHost() {
    DWORD pid = GetCurrentProcessId();
    m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                   sizeof(SharedHeader), SharedMemoryName(pid).c_str());
    if (m_mapping) {
        m_shared = static_cast<SharedHeader*>(
            MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHeader)));
    }
    m_frameEvent = CreateEventW(nullptr, FALSE, FALSE, FrameEventName(pid).c_str());
    if (!m_shared || !m_frameEvent) {
        abort();  // nothing in the hook tests can work without the block
    }
    Reset();
    if (!hook::OpenSharedChannel()) {
        abort();
    }
}

void FakeHost::Reset() {
    ZeroMemory(m_shared, sizeof(SharedHeader));
    m_shared->magic = kSharedMagic;
    m_shared->version = kSharedVersion;
    ResetEvent(m_frameEvent);
}

}  // namespace gazeora::test
