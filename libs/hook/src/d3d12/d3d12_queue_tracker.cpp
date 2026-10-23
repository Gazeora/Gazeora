#include "gazeora/hook/d3d12/d3d12_queue_tracker.h"

#include <wrl/client.h>

#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/d3d12/d3d12_depth.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook::d3d12 {

namespace {

using ExecuteCommandListsFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT,
                                                       ID3D12CommandList* const*);
using D3D12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);

// IUnknown 0-2, ID3D12Object 3-6, ID3D12DeviceChild 7, ID3D12CommandQueue 8-18.
constexpr size_t kExecuteCommandListsSlot = 10;

// How far into the swapchain object to look for the queue pointer (seen at +320).
constexpr size_t kSwapChainScanPointers = 512;

ExecuteCommandListsFn g_origExecuteCommandLists = nullptr;
D3D12CreateDeviceFn g_origD3D12CreateDevice = nullptr;
volatile LONG g_trackerInstalled = 0;

// Every queue the game has submitted work on (identity only, not AddRef'd).
constexpr int kMaxQueues = 32;
ID3D12CommandQueue* volatile g_queues[kMaxQueues] = {};

void TrackQueue(ID3D12CommandQueue* queue) {
    for (auto& entry : g_queues) {
        ID3D12CommandQueue* existing = entry;
        if (existing == queue) {
            return;
        }
        if (!existing &&
            InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(&entry), queue,
                                              nullptr) == nullptr) {
            return;
        }
    }
}

void STDMETHODCALLTYPE HookExecuteCommandLists(ID3D12CommandQueue* queue, UINT count,
                                               ID3D12CommandList* const* lists) {
    TrackQueue(queue);
    g_origExecuteCommandLists(queue, count, lists);
}

// Hooks ExecuteCommandLists, found through a throwaway queue on `device`. `create`
// is CreateHook at startup and CreateEnabledHook later on.
bool HookQueueVTable(ID3D12Device* device,
                     bool (*create)(const char*, void*, ExecuteCommandListsFn,
                                    ExecuteCommandListsFn*)) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)))) {
        return false;
    }
    return create("ID3D12CommandQueue::ExecuteCommandLists",
                  VTableEntry(queue.Get(), kExecuteCommandListsSlot), &HookExecuteCommandLists,
                  &g_origExecuteCommandLists);
}

// Launched games: install the tracker on the game's own first device.
HRESULT WINAPI HookD3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL level, REFIID riid,
                                     void** out) {
    HRESULT hr = g_origD3D12CreateDevice(adapter, level, riid, out);
    ComPtr<ID3D12Device> device;
    if (SUCCEEDED(hr) && out && *out &&
        SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(IID_PPV_ARGS(&device))) &&
        InterlockedCompareExchange(&g_trackerInstalled, 1, 0) == 0) {
        HookQueueVTable(device.Get(), CreateEnabledHook<ExecuteCommandListsFn>);
        InstallDepthHooks(device.Get(), true);
    }
    return hr;
}

}  // namespace

bool InstallQueueTracker() {
    HMODULE module = GetModuleHandleW(L"d3d12.dll");
    if (module) {
        auto createDevice =
            reinterpret_cast<D3D12CreateDeviceFn>(ModuleExport(module, "D3D12CreateDevice"));
        ComPtr<ID3D12Device> device;
        if (!createDevice ||
            FAILED(createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))) ||
            !HookQueueVTable(device.Get(), CreateHook<ExecuteCommandListsFn>)) {
            return false;
        }
        InstallDepthHooks(device.Get(), false);  // without them, 2D only
        return true;
    }
    module = GraphicsRuntime(L"d3d12.dll");
    void* createDevice = ModuleExport(module, "D3D12CreateDevice");
    return createDevice && CreateHook("D3D12CreateDevice", createDevice, &HookD3D12CreateDevice,
                                      &g_origD3D12CreateDevice);
}

ID3D12CommandQueue* FindSwapChainQueue(IDXGISwapChain* swap) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(swap, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
        return nullptr;
    }
    auto begin = reinterpret_cast<uintptr_t*>(swap);
    auto regionEnd =
        reinterpret_cast<uintptr_t*>(static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize);
    auto end =
        begin + kSwapChainScanPointers < regionEnd ? begin + kSwapChainScanPointers : regionEnd;

    for (auto p = begin; p < end; ++p) {
        for (ID3D12CommandQueue* queue : g_queues) {
            if (queue && *p == reinterpret_cast<uintptr_t>(queue)) {
                return queue;
            }
        }
    }
    return nullptr;
}

}  // namespace gazeora::hook::d3d12
