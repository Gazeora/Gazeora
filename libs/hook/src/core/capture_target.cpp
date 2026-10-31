// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/core/capture_target.h"

#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::hook {

namespace {

// Guards every backend's capture state and the target below.
SRWLOCK g_lock = SRWLOCK_INIT;
const void* g_target = nullptr;  // identity only
LONGLONG g_targetLastPresent = 0;

}  // namespace

CaptureScope::CaptureScope() {
    if (!CaptureEnabled()) {
        return;
    }
    m_locked = TryAcquireSRWLockExclusive(&g_lock);
    m_active = m_locked;
    m_now = QpcNow();
}

CaptureScope::~CaptureScope() {
    if (m_locked) {
        ReleaseSRWLockExclusive(&g_lock);
    }
}

bool CaptureScope::SelectTarget(const void* presenter) {
    if (g_target != presenter) {
        bool stale = g_target == nullptr || m_now - g_targetLastPresent > QpcFrequency() / 2;
        if (!stale) {
            return false;
        }
        g_target = presenter;
    }
    g_targetLastPresent = m_now;
    return true;
}

TeardownLock::TeardownLock() { AcquireSRWLockExclusive(&g_lock); }

TeardownLock::~TeardownLock() { ReleaseSRWLockExclusive(&g_lock); }

}  // namespace gazeora::hook
