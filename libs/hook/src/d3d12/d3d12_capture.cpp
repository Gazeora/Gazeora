#include "gazeora/hook/d3d12/d3d12_capture.h"

#include <dxgi1_4.h>

#include <algorithm>

#include "gazeora/hook/core/log.h"
#include "gazeora/hook/d3d12/d3d12_depth.h"
#include "gazeora/hook/d3d12/d3d12_queue_tracker.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

namespace {

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

}  // namespace

void D3D12Capture::Reset() {
    ReleaseResources();
    for (auto& allocator : m_allocators) allocator.Reset();
    m_list.Reset();
    m_queue.Reset();
    m_device.Reset();
    m_reportedNoQueue = false;
}

// Only used when tearing resources down (resize / swapchain switch).
void D3D12Capture::WaitForGpu() {
    if (!m_frameFence || m_frameFence->GetCompletedValue() >= m_lastSubmitted) {
        return;
    }
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (event && SUCCEEDED(m_frameFence->SetEventOnCompletion(m_lastSubmitted, event))) {
        WaitForSingleObject(event, 1000);
    }
    if (event) CloseHandle(event);
}

void D3D12Capture::ReleaseResources() {
    WaitForGpu();
    m_handles.Close();
    for (auto& slot : m_slots) slot.Reset();
    for (auto& slot : m_depthSlots) slot.Reset();
    for (auto& source : m_depthSources) source.Reset();
    for (auto& value : m_allocatorFrame) value = 0;
    m_frameFence.Reset();
    m_hostFence.Reset();
    m_lastSubmitted = 0;
    m_width = 0;
    m_height = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
}

bool D3D12Capture::AttachQueue(IDXGISwapChain* swap) {
    ID3D12CommandQueue* queue = d3d12::FindSwapChainQueue(swap);
    if (!queue) {
        if (!m_reportedNoQueue) {
            m_reportedNoQueue = true;
            SetStatus("D3D12 game: looking for the swapchain's command queue...");
        }
        return false;  // the game submits work every frame, so we'll see it soon
    }
    m_queue = queue;
    if (!CreateCommandObjects()) {
        m_queue.Reset();
        return false;
    }
    return true;
}

bool D3D12Capture::CreateCommandObjects() {
    for (auto& allocator : m_allocators) {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&allocator)))) {
            SetStatus("D3D12 CreateCommandAllocator failed");
            return false;
        }
    }
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].Get(),
                                           nullptr, IID_PPV_ARGS(&m_list)))) {
        SetStatus("D3D12 CreateCommandList failed");
        return false;
    }
    m_list->Close();
    return true;
}

void D3D12Capture::Capture(IDXGISwapChain* swap, ID3D12Device* device, LONGLONG presentQpc) {
    if (m_device.Get() != device) {
        Reset();
        m_device = device;
    }
    if (!m_queue && !AttachQueue(swap)) {
        return;
    }

    ComPtr<IDXGISwapChain3> swap3;
    ComPtr<ID3D12Resource> backBuffer;
    if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&swap3))) ||
        FAILED(swap3->GetBuffer(swap3->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backBuffer)))) {
        return;
    }
    D3D12_RESOURCE_DESC bb = backBuffer->GetDesc();
    bool depth = CaptureDepth();
    if (!m_frameFence || bb.Width != m_width || bb.Height != m_height || bb.Format != m_format ||
        (m_depthSlots[0] != nullptr) != depth || !IsPublisher(this)) {
        if (!CreateResources(bb, depth)) {
            return;
        }
    }

    LONG64 frame = NextFrameNumber();
    UINT index = UINT(frame % kFrameSlots);
    // Our own previous copy into this slot must be done before its allocator is reused.
    if (m_frameFence->GetCompletedValue() < m_allocatorFrame[index]) {
        CountSkippedFrame();
        return;
    }
    if (!ClaimSlot(frame, m_hostFence->GetCompletedValue())) {
        return;
    }

    ID3D12Resource* slot = m_slots[index].Get();
    m_allocators[index]->Reset();
    m_list->Reset(m_allocators[index].Get(), nullptr);
    D3D12_RESOURCE_BARRIER before[2] = {
        Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT,
                   D3D12_RESOURCE_STATE_COPY_SOURCE),
        Transition(slot, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
    };
    m_list->ResourceBarrier(2, before);
    m_list->CopyResource(slot, backBuffer.Get());
    D3D12_RESOURCE_BARRIER after[2] = {
        Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_PRESENT),
        Transition(slot, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON),
    };
    m_list->ResourceBarrier(2, after);

    // The game's depth, from the buffer its command lists copied it into, as 32-bit words
    // whatever the depth format: the slot's RGBA8 takes them as they are.
    d3d12::DepthCopy depthCopy;
    bool withDepth =
        depth && d3d12::TakeDepthCopy(m_device.Get(), UINT(bb.Width), bb.Height, depthCopy);
    m_depthSources[index] = depthCopy.buffer;
    if (withDepth) {
        ID3D12Resource* depthSlot = m_depthSlots[index].Get();
        D3D12_RESOURCE_BARRIER toCopy =
            Transition(depthSlot, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        m_list->ResourceBarrier(1, &toCopy);
        D3D12_TEXTURE_COPY_LOCATION destination = {};
        destination.pResource = depthSlot;
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION source = {};
        source.pResource = depthCopy.buffer.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = depthCopy.footprint;
        source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        D3D12_BOX box = {0,
                         0,
                         0,
                         std::min(depthCopy.footprint.Footprint.Width, UINT(bb.Width)),
                         std::min(depthCopy.footprint.Footprint.Height, bb.Height),
                         1};
        m_list->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
        D3D12_RESOURCE_BARRIER back =
            Transition(depthSlot, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        m_list->ResourceBarrier(1, &back);
    }
    m_list->Close();

    // Submitted on the game's present queue, so it runs after the frame is
    // rendered and before the Present that follows.
    ID3D12CommandList* lists[] = {m_list.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    m_queue->Signal(m_frameFence.Get(), UINT64(frame));
    m_allocatorFrame[index] = UINT64(frame);
    m_lastSubmitted = UINT64(frame);
    if (withDepth) {
        PublishDepth(frame, depthCopy.footprint.Footprint.Width,
                     depthCopy.footprint.Footprint.Height, depthCopy.encoding);
    }
    FramePublished(frame, presentQpc);
}

bool D3D12Capture::CreateResources(const D3D12_RESOURCE_DESC& bb, bool depth) {
    ReleaseResources();

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = bb.Width;
    desc.Height = bb.Height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = bb.Format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    auto createShared = [&](const D3D12_RESOURCE_DESC& desc, ComPtr<ID3D12Resource>& slot,
                            HANDLE& handle) {
        HRESULT hr = m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc,
                                                       D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                       IID_PPV_ARGS(&slot));
        if (SUCCEEDED(hr)) {
            hr = m_device->CreateSharedHandle(slot.Get(), nullptr, GENERIC_ALL, nullptr, &handle);
        }
        if (FAILED(hr)) {
            SetStatus("D3D12 shared texture %llux%u fmt %d failed: 0x%08lX",
                      (unsigned long long)desc.Width, desc.Height, desc.Format, hr);
        }
        return SUCCEEDED(hr);
    };
    D3D12_RESOURCE_DESC depthDesc = desc;
    depthDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        if (!createShared(desc, m_slots[i], m_handles.slots[i]) ||
            (depth && !createShared(depthDesc, m_depthSlots[i], m_handles.depthSlots[i]))) {
            ReleaseResources();
            return false;
        }
    }

    UINT64 initial = InitialFenceValue();
    HRESULT hr =
        m_device->CreateFence(initial, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_frameFence));
    if (SUCCEEDED(hr)) {
        hr = m_device->CreateFence(initial, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_hostFence));
    }
    if (SUCCEEDED(hr)) {
        hr = m_device->CreateSharedHandle(m_frameFence.Get(), nullptr, GENERIC_ALL, nullptr,
                                          &m_handles.frameFence);
    }
    if (SUCCEEDED(hr)) {
        hr = m_device->CreateSharedHandle(m_hostFence.Get(), nullptr, GENERIC_ALL, nullptr,
                                          &m_handles.hostFence);
    }
    if (FAILED(hr)) {
        SetStatus("D3D12 shared fence creation failed: 0x%08lX", hr);
        ReleaseResources();
        return false;
    }
    m_lastSubmitted = initial;

    m_width = bb.Width;
    m_height = bb.Height;
    m_format = bb.Format;
    PublishResources(m_handles, UINT(m_width), m_height, m_format, m_device->GetAdapterLuid(),
                     kApiD3D12, this);
    return true;
}

}  // namespace gazeora::hook
