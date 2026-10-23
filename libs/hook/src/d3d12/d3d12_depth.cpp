#include "gazeora/hook/d3d12/d3d12_depth.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gazeora/hook/core/depth_picker.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/shared_channel.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook::d3d12 {

namespace {

using CreateDepthStencilViewFn = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*,
                                                          const D3D12_DEPTH_STENCIL_VIEW_DESC*,
                                                          D3D12_CPU_DESCRIPTOR_HANDLE);
using DrawInstancedFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT, UINT,
                                                 UINT);
using DrawIndexedInstancedFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT, UINT,
                                                        UINT, INT, UINT);
using ExecuteIndirectFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,
                                                   ID3D12CommandSignature*, UINT, ID3D12Resource*,
                                                   UINT64, ID3D12Resource*, UINT64);
using ResourceBarrierFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT,
                                                   const D3D12_RESOURCE_BARRIER*);
using OMSetRenderTargetsFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, UINT,
                                                      const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL,
                                                      const D3D12_CPU_DESCRIPTOR_HANDLE*);
using ClearDepthStencilViewFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,
                                                         D3D12_CPU_DESCRIPTOR_HANDLE,
                                                         D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT,
                                                         const D3D12_RECT*);

// vtable slots. ID3D12Device: IUnknown 0-2, ID3D12Object 3-6. ID3D12GraphicsCommandList:
// IUnknown 0-2, ID3D12Object 3-6, ID3D12DeviceChild 7, ID3D12CommandList 8.
constexpr size_t kCreateDepthStencilViewSlot = 21;
constexpr size_t kDrawInstancedSlot = 12;
constexpr size_t kDrawIndexedInstancedSlot = 13;
constexpr size_t kResourceBarrierSlot = 26;
constexpr size_t kOMSetRenderTargetsSlot = 46;
constexpr size_t kClearDepthStencilViewSlot = 47;
constexpr size_t kExecuteIndirectSlot = 59;

// An indirect execution draws an unknown number of times; it counts as this many.
constexpr uint32_t kIndirectDraws = 16;
// A game that copied at a barrier lately doesn't need the copy before clearing.
constexpr uint64_t kBarrierCopyFrames = 2;
// Replaced buffers are kept this many frames, for command lists recorded with them that
// haven't run yet.
constexpr uint64_t kRetireFrames = 8;

CreateDepthStencilViewFn g_origCreateDepthStencilView = nullptr;
DrawInstancedFn g_origDrawInstanced = nullptr;
DrawIndexedInstancedFn g_origDrawIndexedInstanced = nullptr;
ExecuteIndirectFn g_origExecuteIndirect = nullptr;
ResourceBarrierFn g_origResourceBarrier = nullptr;
OMSetRenderTargetsFn g_origOMSetRenderTargets = nullptr;
ClearDepthStencilViewFn g_origClearDepthStencilView = nullptr;

// The resource behind each depth descriptor, by its CPU handle (identity only).
SRWLOCK g_viewsLock = SRWLOCK_INIT;
std::unordered_map<SIZE_T, ID3D12Resource*> g_views;

ID3D12Resource* ResourceOf(const D3D12_CPU_DESCRIPTOR_HANDLE& view) {
    AcquireSRWLockShared(&g_viewsLock);
    auto it = g_views.find(view.ptr);
    ID3D12Resource* resource = it == g_views.end() ? nullptr : it->second;
    ReleaseSRWLockShared(&g_viewsLock);
    return resource;
}

// The depth buffer this thread's command list draws with, and the draws not yet counted.
struct Bound {
    ID3D12GraphicsCommandList* list = nullptr;  // identity only
    ID3D12Resource* depth = nullptr;            // identity only
    uint32_t draws = 0;
};
thread_local Bound t_bound;

// How the depth plane's values read as 32-bit words; kDepthNone for formats not handled.
DepthEncoding EncodingOf(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
            return kDepthUnorm24;
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return kDepthFloat;
        default:
            return kDepthNone;
    }
}

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = 0;  // the depth plane
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

struct Tracker {
    std::mutex mutex;
    DepthPicker picker;
    uint64_t frame = 0;
    uint64_t barrierCopyFrame = 0;  // when a barrier last had the depth copied
    // Where the depth is copied to, for which buffer (identity only) of which size.
    ComPtr<ID3D12Resource> buffer;
    ID3D12Resource* bufferFor = nullptr;
    ID3D12Device* bufferDevice = nullptr;  // identity only
    D3D12_RESOURCE_DESC bufferForDesc = {};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    DepthEncoding encoding = kDepthNone;
    bool copied = false;  // a copy into `buffer` has been recorded
    std::vector<std::pair<uint64_t, ComPtr<ID3D12Resource>>> retired;
};
Tracker g_tracker;
// The buffer being copied, read without the lock by the hooks; null while depth isn't wanted.
std::atomic<ID3D12Resource*> g_selected{nullptr};

void Flush() {
    Bound& bound = t_bound;
    if (!bound.depth || bound.draws == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_tracker.mutex);
    g_tracker.picker.Count(bound.depth, bound.draws);
    bound.draws = 0;
}

// Makes `depth` a candidate for this frame. With the lock held.
void See(ID3D12Resource* depth) {
    if (g_tracker.picker.Seen(depth)) {
        return;
    }
    D3D12_RESOURCE_DESC desc = depth->GetDesc();
    DepthPicker::Candidate candidate;
    candidate.width = UINT(desc.Width);
    candidate.height = desc.Height;
    candidate.usable = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
                       desc.DepthOrArraySize == 1 && desc.SampleDesc.Count == 1 &&
                       EncodingOf(desc.Format) != kDepthNone;
    g_tracker.picker.See(depth, candidate);
}

void Bind(ID3D12GraphicsCommandList* list, ID3D12Resource* depth) {
    Flush();
    t_bound = {list, depth, 0};
    if (depth) {
        std::lock_guard<std::mutex> lock(g_tracker.mutex);
        See(depth);
    }
}

// Records a copy of `depth`'s depth plane, in `state`, into the buffer. With the lock held.
void RecordCopy(ID3D12GraphicsCommandList* list, ID3D12Resource* depth,
                D3D12_RESOURCE_STATES state) {
    Tracker& t = g_tracker;
    D3D12_RESOURCE_DESC desc = depth->GetDesc();
    if (!t.buffer || t.bufferFor != depth || desc.Width != t.bufferForDesc.Width ||
        desc.Height != t.bufferForDesc.Height || desc.Format != t.bufferForDesc.Format) {
        ComPtr<ID3D12Device> device;
        if (FAILED(depth->GetDevice(IID_PPV_ARGS(&device)))) {
            return;
        }
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT64 total = 0;
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total);
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = total;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> buffer;
        if (total == 0 || FAILED(device->CreateCommittedResource(
                              &heap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                              D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)))) {
            return;
        }
        if (t.buffer) {
            t.retired.emplace_back(t.frame, std::move(t.buffer));
        }
        t.buffer = std::move(buffer);
        t.bufferFor = depth;
        t.bufferDevice = device.Get();
        t.bufferForDesc = desc;
        t.footprint = footprint;
        t.encoding = EncodingOf(desc.Format);
        t.copied = false;
    }

    // The buffer needs no barrier: buffers go to COPY_DEST on their own, and back to COMMON
    // when the command list is done.
    bool readable = (state & D3D12_RESOURCE_STATE_COPY_SOURCE) != 0;
    if (!readable) {
        D3D12_RESOURCE_BARRIER to = Transition(depth, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        g_origResourceBarrier(list, 1, &to);
    }
    D3D12_TEXTURE_COPY_LOCATION destination = {};
    destination.pResource = t.buffer.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = t.footprint;
    D3D12_TEXTURE_COPY_LOCATION source = {};
    source.pResource = depth;
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    if (!readable) {
        D3D12_RESOURCE_BARRIER back = Transition(depth, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
        g_origResourceBarrier(list, 1, &back);
    }
    t.copied = true;
}

void Stop() {
    t_bound = {};
    // Every bind comes here while depth isn't wanted: only write when there's something to stop.
    if (g_selected.load(std::memory_order_relaxed) && g_selected.exchange(nullptr)) {
        std::lock_guard<std::mutex> lock(g_tracker.mutex);
        g_tracker.picker.Clear();
    }
}

void STDMETHODCALLTYPE HookCreateDepthStencilView(ID3D12Device* device, ID3D12Resource* resource,
                                                  const D3D12_DEPTH_STENCIL_VIEW_DESC* desc,
                                                  D3D12_CPU_DESCRIPTOR_HANDLE view) {
    AcquireSRWLockExclusive(&g_viewsLock);
    if (resource) {
        g_views[view.ptr] = resource;
    } else {
        g_views.erase(view.ptr);
    }
    ReleaseSRWLockExclusive(&g_viewsLock);
    g_origCreateDepthStencilView(device, resource, desc, view);
}

void STDMETHODCALLTYPE HookDrawInstanced(ID3D12GraphicsCommandList* list, UINT a, UINT b, UINT c,
                                         UINT d) {
    if (t_bound.list == list) ++t_bound.draws;
    g_origDrawInstanced(list, a, b, c, d);
}

void STDMETHODCALLTYPE HookDrawIndexedInstanced(ID3D12GraphicsCommandList* list, UINT a, UINT b,
                                                UINT c, INT d, UINT e) {
    if (t_bound.list == list) ++t_bound.draws;
    g_origDrawIndexedInstanced(list, a, b, c, d, e);
}

void STDMETHODCALLTYPE HookExecuteIndirect(ID3D12GraphicsCommandList* list,
                                           ID3D12CommandSignature* signature, UINT maxCount,
                                           ID3D12Resource* arguments, UINT64 argumentOffset,
                                           ID3D12Resource* count, UINT64 countOffset) {
    if (t_bound.list == list) t_bound.draws += kIndirectDraws;
    g_origExecuteIndirect(list, signature, maxCount, arguments, argumentOffset, count,
                          countOffset);
}

void STDMETHODCALLTYPE HookOMSetRenderTargets(ID3D12GraphicsCommandList* list, UINT count,
                                              const D3D12_CPU_DESCRIPTOR_HANDLE* targets,
                                              BOOL single, const D3D12_CPU_DESCRIPTOR_HANDLE* dsv) {
    if (CaptureDepth()) {
        Bind(list, dsv ? ResourceOf(*dsv) : nullptr);
    } else {
        Stop();
    }
    g_origOMSetRenderTargets(list, count, targets, single, dsv);
}

void STDMETHODCALLTYPE HookResourceBarrier(ID3D12GraphicsCommandList* list, UINT count,
                                           const D3D12_RESOURCE_BARRIER* barriers) {
    g_origResourceBarrier(list, count, barriers);
    if (!barriers || !CaptureDepth()) {
        return;
    }
    ID3D12Resource* selected = g_selected.load(std::memory_order_relaxed);
    for (UINT i = 0; i < count; ++i) {
        const D3D12_RESOURCE_BARRIER& b = barriers[i];
        // Done drawing depth: the scene's in it.
        if (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
            (b.Flags & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY) ||
            !(b.Transition.StateBefore & D3D12_RESOURCE_STATE_DEPTH_WRITE) ||
            (b.Transition.StateAfter & D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
            continue;
        }
        ID3D12Resource* depth = b.Transition.pResource;
        std::lock_guard<std::mutex> lock(g_tracker.mutex);
        // A depth buffer, whatever its views: a candidate even in a game that made them
        // before the hook came in, when its binds name nothing the hook knows.
        See(depth);
        g_tracker.picker.Count(depth, 1);
        if (depth == selected &&
            (b.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ||
             b.Transition.Subresource == 0) &&
            list->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            RecordCopy(list, selected, b.Transition.StateAfter);
            g_tracker.barrierCopyFrame = g_tracker.frame;
        }
    }
}

void STDMETHODCALLTYPE HookClearDepthStencilView(ID3D12GraphicsCommandList* list,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE view,
                                                 D3D12_CLEAR_FLAGS flags, FLOAT depth,
                                                 UINT8 stencil, UINT rectCount,
                                                 const D3D12_RECT* rects) {
    ID3D12Resource* selected = g_selected.load(std::memory_order_relaxed);
    if (selected && (flags & D3D12_CLEAR_FLAG_DEPTH) && rectCount == 0 && CaptureDepth() &&
        ResourceOf(view) == selected && list->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        std::lock_guard<std::mutex> lock(g_tracker.mutex);
        // Clearing takes DEPTH_WRITE, so that's the state it's in.
        if (g_tracker.frame - g_tracker.barrierCopyFrame > kBarrierCopyFrames) {
            RecordCopy(list, selected, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }
    }
    g_origClearDepthStencilView(list, view, flags, depth, stencil, rectCount, rects);
}

template <typename Fn>
bool Hook(bool enable, const char* name, void* object, size_t slot, Fn detour, Fn* original) {
    void* target = VTableEntry(object, slot);
    return enable ? CreateEnabledHook(name, target, detour, original)
                  : CreateHook(name, target, detour, original);
}

}  // namespace

bool InstallDepthHooks(ID3D12Device* device, bool enable) {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&allocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(),
                                         nullptr, IID_PPV_ARGS(&list)))) {
        return false;
    }
    list->Close();
    ID3D12GraphicsCommandList* l = list.Get();
    // The copies are recorded with the originals, so the ones they need go first: a failure
    // further on leaves those working on their own.
    return Hook(enable, "ID3D12GraphicsCommandList::ResourceBarrier", l, kResourceBarrierSlot,
                &HookResourceBarrier, &g_origResourceBarrier) &&
           Hook(enable, "ID3D12GraphicsCommandList::ClearDepthStencilView", l,
                kClearDepthStencilViewSlot, &HookClearDepthStencilView,
                &g_origClearDepthStencilView) &&
           Hook(enable, "ID3D12GraphicsCommandList::DrawInstanced", l, kDrawInstancedSlot,
                &HookDrawInstanced, &g_origDrawInstanced) &&
           Hook(enable, "ID3D12GraphicsCommandList::DrawIndexedInstanced", l,
                kDrawIndexedInstancedSlot, &HookDrawIndexedInstanced,
                &g_origDrawIndexedInstanced) &&
           Hook(enable, "ID3D12GraphicsCommandList::ExecuteIndirect", l, kExecuteIndirectSlot,
                &HookExecuteIndirect, &g_origExecuteIndirect) &&
           Hook(enable, "ID3D12Device::CreateDepthStencilView", device,
                kCreateDepthStencilViewSlot, &HookCreateDepthStencilView,
                &g_origCreateDepthStencilView) &&
           Hook(enable, "ID3D12GraphicsCommandList::OMSetRenderTargets", l,
                kOMSetRenderTargetsSlot, &HookOMSetRenderTargets, &g_origOMSetRenderTargets);
}

bool TakeDepthCopy(ID3D12Device* device, UINT backBufferWidth, UINT backBufferHeight,
                   DepthCopy& copy) {
    if (!g_origOMSetRenderTargets) {
        return false;  // not all hooked: nothing gets picked
    }
    Flush();
    std::lock_guard<std::mutex> lock(g_tracker.mutex);
    Tracker& t = g_tracker;
    ++t.frame;
    // Once copied, the buffer holds the latest depth the GPU wrote, even in a frame that
    // didn't record a copy.
    bool have = t.buffer && t.copied && t.bufferDevice == device;
    if (have) {
        copy.buffer = t.buffer;
        copy.footprint = t.footprint;
        copy.encoding = t.encoding;
    }
    g_selected = static_cast<ID3D12Resource*>(
        const_cast<void*>(t.picker.Pick(backBufferWidth, backBufferHeight)));
    while (!t.retired.empty() && t.frame - t.retired.front().first > kRetireFrames) {
        t.retired.erase(t.retired.begin());
    }
    return have;
}

}  // namespace gazeora::hook::d3d12
