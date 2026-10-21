#include "testapp/app.h"

#include <cmath>

namespace testapp {

namespace {

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

HWND CreateTestWindow(const wchar_t* title) {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"GazeoraTestApp";
    RegisterClassExW(&wc);

    RECT rect = {0, 0, LONG(kWidth), LONG(kHeight)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    return CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                           rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);
}

bool PumpMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

Scene SceneAt(float t) {
    Scene s = {{0.5f + 0.5f * std::sin(t), 0.5f + 0.5f * std::sin(t + 2.1f),
                0.5f + 0.5f * std::sin(t + 4.2f), 1.0f},
               {}};
    LONG x = LONG((kWidth - 200) * (0.5f + 0.5f * std::sin(t * 1.3f)));
    LONG y = LONG((kHeight - 200) * (0.5f + 0.5f * std::cos(t * 0.9f)));
    s.square = {x, y, x + 200, y + 200};
    return s;
}

Clock::Clock() {
    QueryPerformanceFrequency(&m_freq);
    QueryPerformanceCounter(&m_start);
}

float Clock::Seconds() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return float(double(now.QuadPart - m_start.QuadPart) / double(m_freq.QuadPart));
}

}  // namespace testapp
