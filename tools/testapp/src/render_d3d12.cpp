#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <cstdio>
#include <utility>

#include "testapp/app.h"

using Microsoft::WRL::ComPtr;

namespace testapp {

int RunD3D12(HWND hwnd) {
    constexpr UINT kBuffers = 2;

    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        fprintf(stderr, "D3D12CreateDevice failed\n");
        return 1;
    }
    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));

    ComPtr<IDXGIFactory2> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = kWidth;
    sd.Height = kHeight;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = kBuffers;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap1;
    ComPtr<IDXGISwapChain3> swap;
    if (FAILED(factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &sd, nullptr, nullptr, &swap1)) ||
        FAILED(swap1.As(&swap))) {
        fprintf(stderr, "CreateSwapChainForHwnd failed\n");
        return 1;
    }

    D3D12_DESCRIPTOR_HEAP_DESC hd = {};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = kBuffers;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap));
    UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    ComPtr<ID3D12Resource> buffers[kBuffers];
    ComPtr<ID3D12CommandAllocator> allocators[kBuffers];
    UINT64 bufferFence[kBuffers] = {};
    for (UINT i = 0; i < kBuffers; ++i) {
        swap->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += SIZE_T(i) * rtvSize;
        device->CreateRenderTargetView(buffers[i].Get(), nullptr, rtv);
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       IID_PPV_ARGS(&allocators[i]));
    }
    ComPtr<ID3D12GraphicsCommandList> list;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators[0].Get(), nullptr,
                              IID_PPV_ARGS(&list));
    list->Close();

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 fenceValue = 0;

    auto waitFor = [&](UINT64 value) {
        if (fence->GetCompletedValue() < value) {
            fence->SetEventOnCompletion(value, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    };

    Clock clock;
    while (PumpMessages()) {
        UINT i = swap->GetCurrentBackBufferIndex();
        waitFor(bufferFence[i]);
        allocators[i]->Reset();
        list->Reset(allocators[i].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = buffers[i].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        list->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += SIZE_T(i) * rtvSize;
        Scene scene = SceneAt(clock.Seconds());
        list->ClearRenderTargetView(rtv, scene.background, 0, nullptr);
        list->ClearRenderTargetView(rtv, kWhite, 1, &scene.square);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        list->ResourceBarrier(1, &barrier);
        list->Close();

        ID3D12CommandList* lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        swap->Present(1, 0);
        queue->Signal(fence.Get(), ++fenceValue);
        bufferFence[i] = fenceValue;
    }

    waitFor(fenceValue);
    CloseHandle(fenceEvent);
    return 0;
}

}  // namespace testapp
