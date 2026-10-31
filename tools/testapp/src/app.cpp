// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

#include "testapp/app.h"

#include <cmath>

#include "testapp/mouse_look.h"

namespace testapp {

namespace {

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_INPUT) {
        if (MouseLook* look = MouseLook::Of(hwnd)) {
            look->OnRawInput(reinterpret_cast<HRAWINPUT>(lp));
        }
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

const char kSceneShader[] = R"(
cbuffer Scene : register(b0) {
    float time; float reversed; float aspect; float yaw; float pitch; float3 unused;
};
struct Vertex { float4 position : SV_Position; float3 color : COLOR; float2 cell : TEXCOORD; };
Vertex vs(uint id : SV_VertexID, uint instance : SV_InstanceID) {
    float2 corner = float2(id & 1, id >> 1);
    float3 p;
    Vertex v;
    if (instance == 0) {  // the floor, all around, so every turn shows
        p = float3(lerp(-60, 60, corner.x), -1, lerp(-60, 60, corner.y));
        v.color = float3(0.3, 0.7, 0.4);
        v.cell = float2(p.x, p.z);
    } else {              // a card standing on it, swaying
        float z = 1 + instance * 2.5;
        float x = sin(time * 0.7 + instance * 1.7) * 2.5;
        p = float3(x + corner.x - 0.5, -1 + corner.y * 1.6, z);
        v.color = float3(0.9, 0.5 + 0.1 * instance, 0.2);
        v.cell = corner * 4;
    }
    // Into the camera's view: turned back by its yaw, then by its pitch.
    p = float3(p.x * cos(yaw) + p.z * sin(yaw), p.y, -p.x * sin(yaw) + p.z * cos(yaw));
    p = float3(p.x, p.y * cos(pitch) - p.z * sin(pitch), p.y * sin(pitch) + p.z * cos(pitch));
    // 60 degrees up and down; near plane 0.1. Reversed depth uses an infinite far plane.
    const float f = 1.732, near = 0.1, far = 100;
    float depth = reversed > 0 ? near : (p.z - near) * far / (far - near);
    v.position = float4(p.x * f / aspect, p.y * f, depth, p.z);
    return v;
}
float4 ps(Vertex v) : SV_Target {
    bool light = (int(floor(v.cell.x)) + int(floor(v.cell.y))) & 1;
    return float4(v.color * (light ? 1.0 : 0.55), 1);
}
)";

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
