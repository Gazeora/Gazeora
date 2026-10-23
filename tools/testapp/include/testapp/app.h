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

// A depth-tested 3D scene for the D3D11 and D3D12 renderers, for 3D from depth: a checkered
// floor running into the distance and cards swaying at different depths. HLSL with vs and ps;
// its constants (b0) are SceneConstants. Drawn as a 4-vertex triangle strip, kSceneInstances
// times.
extern const char kSceneShader[];
constexpr UINT kSceneInstances = 5;
struct SceneConstants {
    float time;
    float reversed;  // 1: depth 1 near, 0 far (cleared to 0, GREATER); 0: the other way
    float aspect;
    float unused;
};

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
// D3D12 with the depth buffer in DEPTH_WRITE all the time, never going through a barrier.
int RunD3D12DepthWrite(HWND hwnd);
// `depthBarriers`: after drawing, the depth buffer goes to a shader-readable state and back
// before the next frame clears it, as in a deferred renderer. Without, it stays DEPTH_WRITE.
int RunD3D12Scene(HWND hwnd, bool depthBarriers);
int RunOpenGL(HWND hwnd);

}  // namespace testapp
