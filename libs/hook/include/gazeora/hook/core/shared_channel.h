// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Hook side of the frame transport described in common/shared.h. API backends
// (D3D11, D3D12, or the D3D11 bridge used by the others) create their own shared
// textures and fences, then use these functions to publish them and to hand frames
// to the host.
#pragma once

#include <windows.h>
#include <dxgiformat.h>

#include "gazeora/common/shared.h"

namespace gazeora::hook {

// Opens the shared block the host created for this process. False if the DLL
// wasn't loaded by Gazeora.exe.
bool OpenSharedChannel();

// nullptr until OpenSharedChannel succeeds.
SharedHeader* Shared();

bool CaptureEnabled();
// Whether the host wants the depth buffer too (3D from depth). Only D3D11 and D3D12 have it.
bool CaptureDepth();
void SetHookState(HookState state);

// True if the host started the game and injected before any game code ran.
bool LaunchedByHost();

// NT handles for one set of capture resources: kFrameSlots textures, frame fence, host fence,
// and the depth slots when there are any.
struct ExportedHandles {
    HANDLE slots[kFrameSlots] = {};
    HANDLE depthSlots[kFrameSlots] = {};
    HANDLE frameFence = nullptr;
    HANDLE hostFence = nullptr;

    void Close();
};

// Describes a newly created set of resources to the host. `owner` identifies the
// backend object that created them.
void PublishResources(const ExportedHandles& handles, UINT width, UINT height, DXGI_FORMAT format,
                      LUID adapterLuid, GraphicsApi api, const void* owner);

// True if `owner` published the resources the host is currently reading. Backends
// recreate (and so republish) theirs when another backend took over in between.
bool IsPublisher(const void* owner);

// Initial value for newly created fences: the current frame number, so a host read
// from an older set of resources never looks "in flight" against the new fences.
UINT64 InitialFenceValue();

// Number the next captured frame will get.
LONG64 NextFrameNumber();

// Announces that `frame` is about to be written, then checks the host isn't still
// reading the slot it reuses. hostCompleted is the host fence's completed value.
// False means skip this frame.
bool ClaimSlot(LONG64 frame, UINT64 hostCompleted);

// Records a skipped frame (e.g. the backend's own previous copy is still running).
void CountSkippedFrame();

// Says what the depth slot of `frame` holds; call before FramePublished. Frames without it
// have no depth (DepthSlotInfo::frame stays behind).
void PublishDepth(LONG64 frame, UINT width, UINT height, DepthEncoding encoding);

// Call after the GPU commands that copy `frame` and signal the frame fence are queued.
void FramePublished(LONG64 frame, LONGLONG presentQpc);

}  // namespace gazeora::hook
