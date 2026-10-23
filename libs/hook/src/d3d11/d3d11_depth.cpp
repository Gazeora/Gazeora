#include "gazeora/hook/d3d11/d3d11_depth.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "gazeora/hook/core/depth_picker.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/log.h"
#include "gazeora/hook/core/shared_channel.h"

using Microsoft::WRL::ComPtr;

namespace gazeora::hook {

namespace {

using DrawIndexedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
using DrawIndexedInstancedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT,
                                                        INT, UINT);
using DrawInstancedFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, UINT);
using DrawIndirectFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Buffer*, UINT);
using OMSetRenderTargetsFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT,
                                                      ID3D11RenderTargetView* const*,
                                                      ID3D11DepthStencilView*);
using OMSetRenderTargetsAndUavsFn = void(STDMETHODCALLTYPE*)(
    ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*, UINT,
    UINT, ID3D11UnorderedAccessView* const*, const UINT*);
using ClearDepthStencilViewFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,
                                                         ID3D11DepthStencilView*, UINT, FLOAT,
                                                         UINT8);
using D3DCompileFn = decltype(&D3DCompile);

// ID3D11DeviceContext vtable slots (IUnknown 0-2, ID3D11DeviceChild 3-6).
constexpr size_t kDrawIndexedSlot = 12;
constexpr size_t kDrawSlot = 13;
constexpr size_t kDrawIndexedInstancedSlot = 20;
constexpr size_t kDrawInstancedSlot = 21;
constexpr size_t kOMSetRenderTargetsSlot = 33;
constexpr size_t kOMSetRenderTargetsAndUavsSlot = 34;
constexpr size_t kDrawIndexedInstancedIndirectSlot = 39;
constexpr size_t kDrawInstancedIndirectSlot = 40;
constexpr size_t kClearDepthStencilViewSlot = 53;

DrawIndexedFn g_origDrawIndexed = nullptr;
DrawFn g_origDraw = nullptr;
DrawIndexedInstancedFn g_origDrawIndexedInstanced = nullptr;
DrawInstancedFn g_origDrawInstanced = nullptr;
DrawIndirectFn g_origDrawIndexedInstancedIndirect = nullptr;
DrawIndirectFn g_origDrawInstancedIndirect = nullptr;
OMSetRenderTargetsFn g_origOMSetRenderTargets = nullptr;
OMSetRenderTargetsAndUavsFn g_origOMSetRenderTargetsAndUavs = nullptr;
ClearDepthStencilViewFn g_origClearDepthStencilView = nullptr;

// The depth buffer this thread's context draws with, and the draws not yet counted. Contexts
// are used from one thread at a time, so counting needs no lock until it's handed over.
struct Bound {
    ID3D11DeviceContext* context = nullptr;  // identity only
    ID3D11Resource* depth = nullptr;         // identity only
    uint32_t draws = 0;
};
thread_local Bound t_bound;

// The typeless format of a depth buffer, to copy it into one that can be read, and the format
// to read it with.
bool DepthFormats(DXGI_FORMAT format, DXGI_FORMAT& typeless, DXGI_FORMAT& view) {
    switch (format) {
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
            typeless = DXGI_FORMAT_R24G8_TYPELESS;
            view = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            return true;
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:
            typeless = DXGI_FORMAT_R32_TYPELESS;
            view = DXGI_FORMAT_R32_FLOAT;
            return true;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            typeless = DXGI_FORMAT_R32G8X24_TYPELESS;
            view = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            return true;
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_TYPELESS:
            typeless = DXGI_FORMAT_R16_TYPELESS;
            view = DXGI_FORMAT_R16_UNORM;
            return true;
        default:
            return false;
    }
}

// Which depth buffer is the game's, and its copy. The hooks run on the game's threads; the
// copies happen on the immediate context, inside the game's own calls on it.
struct Tracker {
    std::mutex mutex;
    bool active = false;  // holding anything, so it's cleared once depth isn't wanted
    DepthPicker picker;
    // References to this frame's candidates, so the picked one is still there at Present.
    std::unordered_map<ID3D11Resource*, ComPtr<ID3D11Resource>> seen;
    ComPtr<ID3D11Resource> selected;
    ID3D11DeviceContext* immediate = nullptr;  // identity only
    // The copy, readable, and how many draws on the selected buffer it holds. The draws
    // since the selected buffer's last clear decide whether a clear (or Present) copies it.
    ComPtr<ID3D11Texture2D> copy;
    ComPtr<ID3D11ShaderResourceView> copyView;
    UINT copyWidth = 0;
    UINT copyHeight = 0;
    uint32_t drawsSinceClear = 0;
    uint32_t copiedDraws = 0;

    void Clear() {
        picker.Clear();
        seen.clear();
        selected.Reset();
        copy.Reset();
        copyView.Reset();
        drawsSinceClear = copiedDraws = 0;
        active = false;
    }
};
Tracker g_tracker;

// Hands this thread's draw count to the tracker.
void Flush() {
    Bound& bound = t_bound;
    if (!bound.depth || bound.draws == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_tracker.mutex);
    g_tracker.picker.Count(bound.depth, bound.draws);
    if (bound.depth == g_tracker.selected.Get()) {
        g_tracker.drawsSinceClear += bound.draws;
    }
    bound.draws = 0;
}

void Bind(ID3D11DeviceContext* context, ID3D11DepthStencilView* dsv) {
    if (!CaptureDepth()) {
        t_bound = {};
        std::lock_guard<std::mutex> lock(g_tracker.mutex);
        if (g_tracker.active) {
            g_tracker.Clear();  // let go of the game's buffers
        }
        return;
    }
    Flush();
    t_bound.context = context;
    t_bound.depth = nullptr;
    if (!dsv) {
        return;
    }
    ComPtr<ID3D11Resource> resource;
    dsv->GetResource(&resource);
    t_bound.depth = resource.Get();
    std::lock_guard<std::mutex> lock(g_tracker.mutex);
    g_tracker.active = true;
    if (g_tracker.picker.Seen(resource.Get())) {
        return;
    }
    DepthPicker::Candidate candidate;
    ComPtr<ID3D11Texture2D> texture;
    if (SUCCEEDED(resource.As(&texture))) {
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        DXGI_FORMAT typeless, view;
        candidate.width = desc.Width;
        candidate.height = desc.Height;
        candidate.usable = desc.SampleDesc.Count == 1 && desc.ArraySize == 1 &&
                           DepthFormats(desc.Format, typeless, view);
    }
    g_tracker.picker.See(resource.Get(), candidate);
    g_tracker.seen.emplace(resource.Get(), resource);
}

// Copies the selected buffer if it has more of the frame than the copy so far. With the
// tracker locked, on the immediate context.
void CopyIfFuller(ID3D11DeviceContext* context) {
    Tracker& t = g_tracker;
    if (t.selected && t.copy && t.drawsSinceClear > t.copiedDraws) {
        context->CopyResource(t.copy.Get(), t.selected.Get());
        t.copiedDraws = t.drawsSinceClear;
    }
}

void STDMETHODCALLTYPE HookDrawIndexed(ID3D11DeviceContext* context, UINT a, UINT b, INT c) {
    if (t_bound.context == context) ++t_bound.draws;
    g_origDrawIndexed(context, a, b, c);
}

void STDMETHODCALLTYPE HookDraw(ID3D11DeviceContext* context, UINT a, UINT b) {
    if (t_bound.context == context) ++t_bound.draws;
    g_origDraw(context, a, b);
}

void STDMETHODCALLTYPE HookDrawIndexedInstanced(ID3D11DeviceContext* context, UINT a, UINT b,
                                                UINT c, INT d, UINT e) {
    if (t_bound.context == context) ++t_bound.draws;
    g_origDrawIndexedInstanced(context, a, b, c, d, e);
}

void STDMETHODCALLTYPE HookDrawInstanced(ID3D11DeviceContext* context, UINT a, UINT b, UINT c,
                                         UINT d) {
    if (t_bound.context == context) ++t_bound.draws;
    g_origDrawInstanced(context, a, b, c, d);
}

void STDMETHODCALLTYPE HookDrawIndexedInstancedIndirect(ID3D11DeviceContext* context,
                                                        ID3D11Buffer* args, UINT offset) {
    if (t_bound.context == context) ++t_bound.draws;
    g_origDrawIndexedInstancedIndirect(context, args, offset);
}

void STDMETHODCALLTYPE HookDrawInstancedIndirect(ID3D11DeviceContext* context, ID3D11Buffer* args,
                                                 UINT offset) {
    if (t_bound.context == context) ++t_bound.draws;
    g_origDrawInstancedIndirect(context, args, offset);
}

void STDMETHODCALLTYPE HookOMSetRenderTargets(ID3D11DeviceContext* context, UINT count,
                                              ID3D11RenderTargetView* const* targets,
                                              ID3D11DepthStencilView* dsv) {
    Bind(context, dsv);
    g_origOMSetRenderTargets(context, count, targets, dsv);
}

void STDMETHODCALLTYPE HookOMSetRenderTargetsAndUavs(ID3D11DeviceContext* context, UINT count,
                                                     ID3D11RenderTargetView* const* targets,
                                                     ID3D11DepthStencilView* dsv, UINT uavStart,
                                                     UINT uavCount,
                                                     ID3D11UnorderedAccessView* const* uavs,
                                                     const UINT* initialCounts) {
    if (count != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
        Bind(context, dsv);
    }
    g_origOMSetRenderTargetsAndUavs(context, count, targets, dsv, uavStart, uavCount, uavs,
                                    initialCounts);
}

void STDMETHODCALLTYPE HookClearDepthStencilView(ID3D11DeviceContext* context,
                                                 ID3D11DepthStencilView* dsv, UINT flags,
                                                 FLOAT depth, UINT8 stencil) {
    if (dsv && (flags & D3D11_CLEAR_DEPTH) && CaptureDepth()) {
        Flush();
        ComPtr<ID3D11Resource> resource;
        dsv->GetResource(&resource);
        std::lock_guard<std::mutex> lock(g_tracker.mutex);
        if (resource == g_tracker.selected && context == g_tracker.immediate) {
            CopyIfFuller(context);
            g_tracker.drawsSinceClear = 0;
        }
    }
    g_origClearDepthStencilView(context, dsv, flags, depth, stencil);
}

// Draws the copy's depth values into the slot as 32-bit words, a byte a channel.
const char kConvertShader[] = R"(
Texture2D<float> depth : register(t0);
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 ps(float4 position : SV_Position) : SV_Target {
    uint bits = asuint(depth.Load(int3(position.xy, 0)));
    return float4(bits & 255, (bits >> 8) & 255, (bits >> 16) & 255, bits >> 24) / 255;
}
)";

// The pipeline state Convert changes, to put back afterwards: the game may rely on state it
// set long ago.
struct SavedState {
    static constexpr UINT kMaxInstances = 256;

    D3D11_PRIMITIVE_TOPOLOGY topology;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11VertexShader> vs;
    ID3D11ClassInstance* vsInstances[kMaxInstances];
    UINT vsInstanceCount = kMaxInstances;
    ComPtr<ID3D11HullShader> hs;
    ID3D11ClassInstance* hsInstances[kMaxInstances];
    UINT hsInstanceCount = kMaxInstances;
    ComPtr<ID3D11DomainShader> ds;
    ID3D11ClassInstance* dsInstances[kMaxInstances];
    UINT dsInstanceCount = kMaxInstances;
    ComPtr<ID3D11GeometryShader> gs;
    ID3D11ClassInstance* gsInstances[kMaxInstances];
    UINT gsInstanceCount = kMaxInstances;
    ComPtr<ID3D11PixelShader> ps;
    ID3D11ClassInstance* psInstances[kMaxInstances];
    UINT psInstanceCount = kMaxInstances;
    ComPtr<ID3D11ShaderResourceView> psView;
    ComPtr<ID3D11RasterizerState> rasterizer;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ComPtr<ID3D11BlendState> blend;
    FLOAT blendFactor[4];
    UINT sampleMask;
    ComPtr<ID3D11DepthStencilState> depthStencil;
    UINT stencilRef;
    ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView* dsv;

    explicit SavedState(ID3D11DeviceContext* c) {
        c->IAGetPrimitiveTopology(&topology);
        c->IAGetInputLayout(&inputLayout);
        c->VSGetShader(&vs, vsInstances, &vsInstanceCount);
        c->HSGetShader(&hs, hsInstances, &hsInstanceCount);
        c->DSGetShader(&ds, dsInstances, &dsInstanceCount);
        c->GSGetShader(&gs, gsInstances, &gsInstanceCount);
        c->PSGetShader(&ps, psInstances, &psInstanceCount);
        c->PSGetShaderResources(0, 1, &psView);
        c->RSGetState(&rasterizer);
        c->RSGetViewports(&viewportCount, viewports);
        c->OMGetBlendState(&blend, blendFactor, &sampleMask);
        c->OMGetDepthStencilState(&depthStencil, &stencilRef);
        // Straight to the original: these aren't the game's binds.
        c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &dsv);
    }

    void Restore(ID3D11DeviceContext* c) {
        c->IASetPrimitiveTopology(topology);
        c->IASetInputLayout(inputLayout.Get());
        c->VSSetShader(vs.Get(), vsInstances, vsInstanceCount);
        c->HSSetShader(hs.Get(), hsInstances, hsInstanceCount);
        c->DSSetShader(ds.Get(), dsInstances, dsInstanceCount);
        c->GSSetShader(gs.Get(), gsInstances, gsInstanceCount);
        c->PSSetShader(ps.Get(), psInstances, psInstanceCount);
        ID3D11ShaderResourceView* view = psView.Get();
        c->PSSetShaderResources(0, 1, &view);
        c->RSSetState(rasterizer.Get());
        c->RSSetViewports(viewportCount, viewports);
        c->OMSetBlendState(blend.Get(), blendFactor, sampleMask);
        c->OMSetDepthStencilState(depthStencil.Get(), stencilRef);
        g_origOMSetRenderTargets(c, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv);
        Release(vsInstances, vsInstanceCount);
        Release(hsInstances, hsInstanceCount);
        Release(dsInstances, dsInstanceCount);
        Release(gsInstances, gsInstanceCount);
        Release(psInstances, psInstanceCount);
        for (ID3D11RenderTargetView* target : targets) {
            if (target) target->Release();
        }
        if (dsv) dsv->Release();
    }

    static void Release(ID3D11ClassInstance** instances, UINT count) {
        for (UINT i = 0; i < count; ++i) {
            if (instances[i]) instances[i]->Release();
        }
    }
};

template <typename Fn>
bool HookContext(ID3D11DeviceContext* context, const char* name, size_t slot, Fn detour,
                 Fn* original) {
    return CreateHook(name, VTableEntry(context, slot), detour, original);
}

}  // namespace

namespace d3d11 {

bool InstallDepthHooks(ID3D11Device* device) {
    ComPtr<ID3D11DeviceContext> c;
    device->GetImmediateContext(&c);
    ID3D11DeviceContext* context = c.Get();
    return HookContext(context, "ID3D11DeviceContext::DrawIndexed", kDrawIndexedSlot,
                       &HookDrawIndexed, &g_origDrawIndexed) &&
           HookContext(context, "ID3D11DeviceContext::Draw", kDrawSlot, &HookDraw, &g_origDraw) &&
           HookContext(context, "ID3D11DeviceContext::DrawIndexedInstanced",
                       kDrawIndexedInstancedSlot, &HookDrawIndexedInstanced,
                       &g_origDrawIndexedInstanced) &&
           HookContext(context, "ID3D11DeviceContext::DrawInstanced", kDrawInstancedSlot,
                       &HookDrawInstanced, &g_origDrawInstanced) &&
           HookContext(context, "ID3D11DeviceContext::DrawIndexedInstancedIndirect",
                       kDrawIndexedInstancedIndirectSlot, &HookDrawIndexedInstancedIndirect,
                       &g_origDrawIndexedInstancedIndirect) &&
           HookContext(context, "ID3D11DeviceContext::DrawInstancedIndirect",
                       kDrawInstancedIndirectSlot, &HookDrawInstancedIndirect,
                       &g_origDrawInstancedIndirect) &&
           HookContext(context, "ID3D11DeviceContext::OMSetRenderTargets", kOMSetRenderTargetsSlot,
                       &HookOMSetRenderTargets, &g_origOMSetRenderTargets) &&
           HookContext(context, "ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews",
                       kOMSetRenderTargetsAndUavsSlot, &HookOMSetRenderTargetsAndUavs,
                       &g_origOMSetRenderTargetsAndUavs) &&
           HookContext(context, "ID3D11DeviceContext::ClearDepthStencilView",
                       kClearDepthStencilViewSlot, &HookClearDepthStencilView,
                       &g_origClearDepthStencilView);
}

}  // namespace d3d11

void D3D11DepthCapture::Reset() {
    std::lock_guard<std::mutex> lock(g_tracker.mutex);
    g_tracker.Clear();
    m_device.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_shaderFailed = false;
    for (UINT i = 0; i < kFrameSlots; ++i) {
        m_targetTextures[i] = nullptr;
        m_targets[i].Reset();
    }
    m_nextTarget = 0;
}

bool D3D11DepthCapture::Capture(ID3D11Device* device, ID3D11DeviceContext* context,
                                ID3D11Texture2D* slot, UINT width, UINT height, UINT& depthWidth,
                                UINT& depthHeight) {
    if (!g_origOMSetRenderTargets || !slot) {
        return false;  // no depth hooks
    }
    if (m_device.Get() != device) {
        Reset();
        m_device = device;
    }
    Flush();
    std::lock_guard<std::mutex> lock(g_tracker.mutex);
    Tracker& t = g_tracker;
    t.immediate = context;
    t.active = true;

    // This frame's depth: the copy a clear made, or the buffer as it stands now.
    CopyIfFuller(context);
    bool have = t.copiedDraws > 0 && Convert(device, context, slot, t.copyView.Get(),
                                             std::min(t.copyWidth, width),
                                             std::min(t.copyHeight, height));
    depthWidth = t.copyWidth;
    depthHeight = t.copyHeight;
    t.copiedDraws = t.drawsSinceClear = 0;

    // The buffer to copy next frame.
    auto* picked = static_cast<ID3D11Resource*>(const_cast<void*>(t.picker.Pick(width, height)));
    if (picked != t.selected.Get()) {
        t.selected.Reset();
        t.copy.Reset();
        t.copyView.Reset();
        auto it = t.seen.find(picked);
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11Device> owner;
        D3D11_TEXTURE2D_DESC desc = {};
        if (it != t.seen.end() && SUCCEEDED(it->second.As(&texture))) {
            texture->GetDevice(&owner);
            texture->GetDesc(&desc);
        }
        DXGI_FORMAT typeless, view;
        if (texture && owner.Get() == device && DepthFormats(desc.Format, typeless, view)) {
            desc.Format = typeless;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            desc.MipLevels = 1;
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
            viewDesc.Format = view;
            viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipLevels = 1;
            if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &t.copy)) &&
                SUCCEEDED(device->CreateShaderResourceView(t.copy.Get(), &viewDesc,
                                                           &t.copyView))) {
                t.selected = it->second;
                t.copyWidth = desc.Width;
                t.copyHeight = desc.Height;
            } else {
                t.copy.Reset();
                t.copyView.Reset();
            }
        }
    }
    t.seen.clear();
    return have;
}

bool D3D11DepthCapture::CreateShaders(ID3D11Device* device) {
    if (m_pixelShader || m_shaderFailed) {
        return !m_shaderFailed;
    }
    m_shaderFailed = true;
    // The game may not ship the compiler, but Windows does.
    HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
    auto compile = reinterpret_cast<D3DCompileFn>(ModuleExport(compiler, "D3DCompile"));
    ComPtr<ID3DBlob> vs, ps;
    if (!compile ||
        FAILED(compile(kConvertShader, sizeof(kConvertShader) - 1, nullptr, nullptr, nullptr,
                       "vs", "vs_4_0", 0, 0, &vs, nullptr)) ||
        FAILED(compile(kConvertShader, sizeof(kConvertShader) - 1, nullptr, nullptr, nullptr,
                       "ps", "ps_4_0", 0, 0, &ps, nullptr)) ||
        FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr,
                                          &m_vertexShader)) ||
        FAILED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr,
                                         &m_pixelShader))) {
        SetStatus("D3D11 depth copy: the shaders didn't compile, so no 3D");
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        return false;
    }
    m_shaderFailed = false;
    return true;
}

bool D3D11DepthCapture::Convert(ID3D11Device* device, ID3D11DeviceContext* context,
                                ID3D11Texture2D* slot, ID3D11ShaderResourceView* depth, UINT width,
                                UINT height) {
    if (!depth || !CreateShaders(device)) {
        return false;
    }
    ID3D11RenderTargetView* target = nullptr;
    for (UINT i = 0; i < kFrameSlots && !target; ++i) {
        if (m_targetTextures[i] == slot) target = m_targets[i].Get();
    }
    if (!target) {
        // A slot of a new ring. The views hold their textures, so an address seen before is
        // always the same texture.
        UINT i = m_nextTarget;
        m_nextTarget = (i + 1) % kFrameSlots;
        m_targetTextures[i] = nullptr;
        m_targets[i].Reset();
        if (FAILED(device->CreateRenderTargetView(slot, nullptr, &m_targets[i]))) {
            return false;
        }
        m_targetTextures[i] = slot;
        target = m_targets[i].Get();
    }

    SavedState saved(context);
    D3D11_VIEWPORT viewport = {0, 0, float(width), float(height), 0, 1};
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(nullptr);
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, &depth);
    context->RSSetState(nullptr);
    context->RSSetViewports(1, &viewport);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    g_origOMSetRenderTargets(context, 1, &target, nullptr);
    g_origDraw(context, 3, 0);
    saved.Restore(context);
    return true;
}

}  // namespace gazeora::hook
