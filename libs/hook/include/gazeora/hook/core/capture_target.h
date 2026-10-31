// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Serializes frame captures across every hooked API and picks the one presenter
// (swapchain, D3D9 device, GL window, ...) whose frames go to the host.
#pragma once

#include <windows.h>

namespace gazeora::hook {

// Held for the duration of one capture inside a present hook. Only try-locks, so a
// game's present never waits on another thread's capture.
class CaptureScope {
public:
    CaptureScope();
    ~CaptureScope();
    CaptureScope(const CaptureScope&) = delete;
    CaptureScope& operator=(const CaptureScope&) = delete;

    // True if the lock was taken and the host wants frames.
    bool Active() const { return m_active; }

    // Call once `presenter` is known to be capturable. Games can present from several
    // presenters (launcher, tool windows, even different APIs); one stays the target
    // until it stops presenting for half a second.
    bool SelectTarget(const void* presenter);

    // QueryPerformanceCounter value taken when the scope was entered.
    LONGLONG PresentQpc() const { return m_now; }

private:
    bool m_locked = false;
    bool m_active = false;
    LONGLONG m_now = 0;
};

// Blocks until no capture is running. For tearing down capture resources outside a
// present (device reset/destruction), which must not race a capture in progress.
class TeardownLock {
public:
    TeardownLock();
    ~TeardownLock();
    TeardownLock(const TeardownLock&) = delete;
    TeardownLock& operator=(const TeardownLock&) = delete;
};

}  // namespace gazeora::hook
