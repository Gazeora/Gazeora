// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "gazeora/hook/input/head_turn_feed.h"

#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/shared_channel.h"

namespace gazeora::hook::input {

namespace {

constexpr double kMicro = 1e-6;
constexpr double kPermille = 1e-3;

// One turn for every controller the game reads: whichever reads first takes what's new.
SRWLOCK g_lock = SRWLOCK_INIT;
StickTurn g_turn;

}  // namespace

StickPush HeadTurnForRead(bool active, LONG path) {
    SharedHeader* shared = Shared();
    if (!shared) {
        return {};
    }
    const LONGLONG now = QpcNow();
    InterlockedOr(&shared->controllerPaths, path);
    InterlockedExchange64(&shared->controllerReadQpc, now);
    if (active) {
        InterlockedExchange64(&shared->controllerActiveQpc, now);
    }
    StickResponse response;
    response.degreesPerSecond = shared->stickDegreesPerSecond;
    response.deadZone = shared->stickDeadZonePermille * kPermille;
    const double yaw = shared->headYawMicro * kMicro;
    const double pitch = shared->headPitchMicro * kMicro;
    const double seconds = double(now) / double(QpcFrequency());
    AcquireSRWLockExclusive(&g_lock);
    StickPush push = g_turn.Next(yaw, pitch, seconds, response);
    ReleaseSRWLockExclusive(&g_lock);
    return push;
}

}  // namespace gazeora::hook::input
