#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstring>
#include <utility>

#include "testapp/app.h"

using Microsoft::WRL::ComPtr;

namespace testapp {

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

// The 3D scene's pipeline: root constants at b0, depth 32-bit float, 1 near and 0 far.
bool CreateScenePipeline(ID3D12Device* device, ComPtr<ID3D12RootSignature>& rootSignature,
                         ComPtr<ID3D12PipelineState>& pipeline) {
    D3D12_ROOT_PARAMETER constants = {};
    constants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants.Constants.Num32BitValues = sizeof(SceneConstants) / 4;
    constants.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &constants;
    ComPtr<ID3DBlob> rootBlob, vs, ps;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob,
                                           nullptr)) ||
        FAILED(device->CreateRootSignature(0, rootBlob->GetBufferPointer(),
                                           rootBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&rootSignature))) ||
        FAILED(D3DCompile(kSceneShader, strlen(kSceneShader), nullptr, nullptr, nullptr, "vs",
                          "vs_5_0", 0, 0, &vs, nullptr)) ||
        FAILED(D3DCompile(kSceneShader, strlen(kSceneShader), nullptr, nullptr, nullptr, "ps",
                          "ps_5_0", 0, 0, &ps, nullptr))) {
        return false;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rootSignature.Get();
    pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = UINT_MAX;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    pd.DepthStencilState.DepthEnable = TRUE;
    pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pd.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pd.SampleDesc.Count = 1;
    return SUCCEEDED(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pipeline)));
}

}  // namespace

int RunD3D12(HWND hwnd) { return RunD3D12Scene(hwnd, true); }

int RunD3D12DepthWrite(HWND hwnd) { return RunD3D12Scene(hwnd, false); }

int RunD3D12Scene(HWND hwnd, bool depthBarriers) {
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

    // The depth buffer the way recent games have it: 32-bit float, 1 near and 0 far.
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC dd = {};
    dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dd.Width = kWidth;
    dd.Height = kHeight;
    dd.DepthOrArraySize = 1;
    dd.MipLevels = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 0.0f;
    ComPtr<ID3D12Resource> depth;
    const D3D12_RESOURCE_STATES kDepthRead =
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hd.NumDescriptors = 1;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipeline;
    if (FAILED(device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &dd,
            depthBarriers ? kDepthRead : D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
            IID_PPV_ARGS(&depth))) ||
        FAILED(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dsvHeap))) ||
        !CreateScenePipeline(device.Get(), rootSignature, pipeline)) {
        fprintf(stderr, "creating the 3D scene failed\n");
        return 1;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depth.Get(), nullptr, dsv);

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

    D3D12_VIEWPORT viewport = {0, 0, float(kWidth), float(kHeight), 0, 1};
    D3D12_RECT scissor = {0, 0, LONG(kWidth), LONG(kHeight)};
    Clock clock;
    while (PumpMessages()) {
        UINT i = swap->GetCurrentBackBufferIndex();
        waitFor(bufferFence[i]);
        allocators[i]->Reset();
        list->Reset(allocators[i].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier = Transition(
            buffers[i].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->ResourceBarrier(1, &barrier);
        if (depthBarriers) {
            D3D12_RESOURCE_BARRIER write =
                Transition(depth.Get(), kDepthRead, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            list->ResourceBarrier(1, &write);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += SIZE_T(i) * rtvSize;
        float t = clock.Seconds();
        Scene scene = SceneAt(t);
        list->ClearRenderTargetView(rtv, scene.background, 0, nullptr);
        list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
        SceneConstants values = {t, 1.0f, float(kWidth) / kHeight, 0.0f};
        list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        list->SetGraphicsRootSignature(rootSignature.Get());
        list->SetGraphicsRoot32BitConstants(0, sizeof(values) / 4, &values, 0);
        list->SetPipelineState(pipeline.Get());
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        list->DrawInstanced(4, kSceneInstances, 0, 0);
        if (depthBarriers) {
            // Done with depth: lighting and effects would read it now.
            D3D12_RESOURCE_BARRIER read =
                Transition(depth.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, kDepthRead);
            list->ResourceBarrier(1, &read);
        }
        // A HUD-like square on top, drawn without depth.
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
