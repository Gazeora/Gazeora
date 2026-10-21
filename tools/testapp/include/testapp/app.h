// Shared pieces of the test app: window, animated scene, and one Run function per API.
#pragma once

#include <windows.h>

namespace testapp {

constexpr UINT kWidth = 1280;
constexpr UINT kHeight = 720;
constexpr float kWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};

HWND CreateTestWindow(const wchar_t* title);

// Returns false once the window is closed.
bool PumpMessages();

// What to draw at time t (seconds): a color-cycling background and a moving square.
struct Scene {
    float background[4];
    RECT square;
};
Scene SceneAt(float t);

class Clock {
public:
    Clock();
    float Seconds() const;

private:
    LARGE_INTEGER m_freq;
    LARGE_INTEGER m_start;
};

// Render loops; return the process exit code.
#ifdef TESTAPP_HAVE_D3D8
int RunD3D8(HWND hwnd);
#endif
int RunD3D9(HWND hwnd);
int RunD3D10(HWND hwnd);
int RunD3D11(HWND hwnd);
int RunD3D12(HWND hwnd);
int RunOpenGL(HWND hwnd);

}  // namespace testapp
