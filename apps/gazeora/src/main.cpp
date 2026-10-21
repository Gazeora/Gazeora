// Gazeora: game launcher UI. Keeps a library of games, launches them, and can inject
// the capture hook as they start. GazeoraCLI.exe is the command-line equivalent.

#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "app.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kWindowClass[] = L"GazeoraLauncher";

struct Renderer {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> target;

    bool Create(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC desc = {};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = hwnd;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels),
            D3D11_SDK_VERSION, &desc, &swapChain, &device, nullptr, &context);
        if (FAILED(hr)) {
            // No usable GPU (e.g. a remote session): fall back to the software rasterizer.
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels,
                                               ARRAYSIZE(levels), D3D11_SDK_VERSION, &desc,
                                               &swapChain, &device, nullptr, &context);
        }
        if (FAILED(hr)) {
            return false;
        }
        CreateTarget();
        return true;
    }

    void CreateTarget() {
        ComPtr<ID3D11Texture2D> backBuffer;
        swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        device->CreateRenderTargetView(backBuffer.Get(), nullptr, &target);
    }

    void Resize(UINT width, UINT height) {
        target.Reset();
        swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        CreateTarget();
    }
};

Renderer g_renderer;
gazeora::app::App* g_app = nullptr;
float g_dpiScale = 1.0f;

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return TRUE;
    }
    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && g_renderer.swapChain) {
                g_renderer.Resize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = LONG(980 * g_dpiScale);
            info->ptMinTrackSize.y = LONG(640 * g_dpiScale);
            return 0;
        }
        case WM_DPICHANGED: {
            // Fonts are built for the starting DPI; just follow the suggested window size.
            const RECT* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_ACTIVATEAPP:
            if (wParam && g_app) {
                g_app->OnActivated();
            }
            break;
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            std::vector<std::wstring> paths;
            UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i) {
                wchar_t path[MAX_PATH * 2];
                if (DragQueryFileW(drop, i, path, ARRAYSIZE(path))) {
                    paths.push_back(path);
                }
            }
            DragFinish(drop);
            if (g_app) {
                g_app->OnFilesDropped(paths);
            }
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;  // no ALT application menu
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Dark title bar matching the window, on Windows 10 20H1+ / Windows 11.
void StyleTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    COLORREF caption = RGB(13, 14, 22);  // the sidebar color
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    // One launcher at a time: a second start just brings the first one forward.
    HANDLE singleInstance = CreateMutexW(nullptr, TRUE, L"Local\\Gazeora_launcher");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(kWindowClass, nullptr)) {
            ShowWindow(existing, IsIconic(existing) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(existing);
        }
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();
    POINT origin = {0, 0};
    g_dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, L"IDI_GAZEORA");
    if (!wc.hIcon) {
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wc.hbrBackground = CreateSolidBrush(RGB(18, 20, 30));
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    int width = int(1360 * g_dpiScale), height = int(860 * g_dpiScale);
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    width = std::min(width, int(work.right - work.left));
    height = std::min(height, int(work.bottom - work.top));
    HWND hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES, kWindowClass, L"Gazeora", WS_OVERLAPPEDWINDOW,
        work.left + (work.right - work.left - width) / 2,
        work.top + (work.bottom - work.top - height) / 2, width, height, nullptr, nullptr,
        instance, nullptr);
    StyleTitleBar(hwnd);

    if (!g_renderer.Create(hwnd)) {
        MessageBoxW(hwnd, L"Could not create a Direct3D 11 device.", L"Gazeora", MB_ICONERROR);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto app = std::make_unique<gazeora::app::App>(hwnd, g_renderer.device.Get(), g_dpiScale);
    app->SetupImGui();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_renderer.device.Get(), g_renderer.context.Get());
    g_app = app.get();

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    // Redraw only when something happens, so the launcher stays idle while a game runs.
    // Input keeps it drawing for a few frames (ImGui settles hover state over 2-3 frames);
    // otherwise it repaints twice a second to keep play time and capture stats fresh.
    int busyFrames = 3;
    bool running = true;
    while (running) {
        bool idle = busyFrames == 0 && !app->WantsContinuousFrames();
        if (idle || IsIconic(hwnd)) {
            MsgWaitForMultipleObjectsEx(0, nullptr, 500, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
            busyFrames = 3;
        }
        if (!running) {
            break;
        }

        app->Tick();
        if (IsIconic(hwnd)) {
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app->Draw();
        ImGui::Render();

        const float clear[4] = {18 / 255.0f, 20 / 255.0f, 30 / 255.0f, 1.0f};
        ID3D11RenderTargetView* target = g_renderer.target.Get();
        g_renderer.context->OMSetRenderTargets(1, &target, nullptr);
        g_renderer.context->ClearRenderTargetView(target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_renderer.swapChain->Present(1, 0);
        if (busyFrames > 0) {
            --busyFrames;
        }
    }

    g_app = nullptr;
    app.reset();  // saves play time of games still running
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(hwnd);
    CloseHandle(singleInstance);
    CoUninitialize();
    return 0;
}
