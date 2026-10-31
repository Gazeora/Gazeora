// Copyright (C) 2026 Nithin Aashik
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

// Capture: the frame transport from the hook to the host, reading frames back from the GPU
// (at full size, scaled down, for VR), screenshots, and the real hook in a real process for
// every graphics API, using gazeora_testapp.exe as the game. The last one opens a window per
// API for a few seconds.
#include <windows.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bench_util.h"
#include "gazeora/hook/core/hook_util.h"
#include "gazeora/hook/core/shared_channel.h"
#include "gazeora/hook/d3d11/d3d11_frame_ring.h"
#include "gazeora/host/capture/capture_client.h"
#include "gazeora/host/capture/frame_dump.h"
#include "gazeora/host/launch/injector.h"
#include "gazeora/host/launch/launcher.h"

namespace gazeora::bench {
namespace {

struct Device {
    Microsoft::WRL::ComPtr<ID3D11Device5> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context;
};

// A D3D11.4 device on the GPU, or on WARP if there's none. `onWarp` says which.
Device CreateDevice(bool& onWarp) {
    Device result;
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        if (SUCCEEDED(D3D11CreateDevice(nullptr, type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                        nullptr, 0, D3D11_SDK_VERSION, &device, nullptr,
                                        &context)) &&
            SUCCEEDED(device.As(&result.device)) && SUCCEEDED(context.As(&result.context))) {
            onWarp = type == D3D_DRIVER_TYPE_WARP;
            return result;
        }
    }
    return {};
}

// A width x height texture filled with `pixels` (4 bytes each, tightly packed).
Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture(ID3D11Device* device, UINT width, UINT height,
                                                      DXGI_FORMAT format,
                                                      const std::vector<uint32_t>& pixels) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = {pixels.data(), width * 4, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    device->CreateTexture2D(&desc, &data, &texture);
    return texture;
}

std::vector<uint32_t> NoisePixels(UINT width, UINT height) {
    std::vector<uint8_t> bytes(size_t(width) * height * 4);
    FillNoise(bytes);
    std::vector<uint32_t> pixels(size_t(width) * height);
    memcpy(pixels.data(), bytes.data(), bytes.size());
    return pixels;
}

const char* Where(bool onWarp) { return onWarp ? "WARP (no GPU found)" : "GPU"; }

TEST(Capture, TransportInProcess) {
    // The game and the host in this one process, as in the transport tests, on the GPU.
    const Size size = k1080p;
    std::wstring error;
    CaptureClient client;  // created first: it owns the shared block
    ASSERT_TRUE(client.Create(GetCurrentProcessId(), error)) << Narrow(error);
    ASSERT_TRUE(hook::OpenSharedChannel());
    bool onWarp = false;
    Device game = CreateDevice(onWarp);
    ASSERT_TRUE(game.device) << "no D3D11.4 device";
    hook::D3D11FrameRing ring;
    ASSERT_TRUE(ring.Create(game.device.Get(), size.width, size.height, DXGI_FORMAT_B8G8R8A8_UNORM,
                            kApiD3D11));
    client.SetCaptureEnabled(true);
    auto backBuffer =
        CreateTexture(game.device.Get(), size.width, size.height, DXGI_FORMAT_B8G8R8A8_UNORM,
                      NoisePixels(size.width, size.height));
    ASSERT_TRUE(backBuffer);

    TextureReader reader;
    std::vector<uint8_t> bgra;
    std::vector<double> hookSide, toHost, handoff, readBack, total;
    int skipped = 0;
    for (int i = 0; i < 130; ++i) {
        double start = QpcSeconds();
        // What a present hook does: claim a slot, copy the back buffer into it, publish.
        LONG64 frame = hook::NextFrameNumber();
        if (!ring.Claim(frame)) {
            ++skipped;
            continue;
        }
        game.context->CopyResource(ring.Slot(frame), backBuffer.Get());
        ring.Publish(game.context.Get(), frame, hook::QpcNow(), true);
        double published = QpcSeconds();
        ASSERT_EQ(client.WaitForFrame(5000), CaptureClient::WaitResult::Frame);
        double received = QpcSeconds();
        UINT width = 0, height = 0;
        ASSERT_TRUE(reader.Read(client.Device(), client.Frame(), width, height, bgra, error))
            << Narrow(error);
        double read = QpcSeconds();
        if (i < 10) continue;
        hookSide.push_back(published - start);
        toHost.push_back(received - published);
        handoff.push_back(client.LastHandoffMs() / 1000);
        readBack.push_back(read - received);
        total.push_back(read - start);
    }
    PrintHeader((std::string("Frame transport, 1080p BGRA8, ") + Where(onWarp)).c_str());
    Print("hook: claim + copy + publish (CPU)", Stats::Of(hookSide));
    Print("publish to WaitForFrame returning", Stats::Of(toHost));
    Print("LastHandoffMs (present to host copy)", Stats::Of(handoff));
    Print("TextureReader::Read of the frame", Stats::Of(readBack));
    Print("present to BGRA8 on the CPU", Stats::Of(total),
          skipped ? std::to_string(skipped) + " skipped" : "");
}

TEST(Capture, TextureReader) {
    bool onWarp = false;
    Device gpu = CreateDevice(onWarp);
    ASSERT_TRUE(gpu.device) << "no D3D11 device";
    PrintHeader((std::string("TextureReader::Read, ") + Where(onWarp)).c_str());
    struct Case {
        const char* name;
        Size source;
        DXGI_FORMAT format;
        bool limitTo1080p;
        bool vr;
    };
    const Case cases[] = {
        {"1080p BGRA8, full size", k1080p, DXGI_FORMAT_B8G8R8A8_UNORM, false, false},
        {"1080p RGBA8, full size", k1080p, DXGI_FORMAT_R8G8B8A8_UNORM, false, false},
        {"1080p RGB10A2, full size", k1080p, DXGI_FORMAT_R10G10B10A2_UNORM, false, false},
        {"720p BGRA8, full size", k720p, DXGI_FORMAT_B8G8R8A8_UNORM, false, false},
        {"1440p BGRA8, scaled to 1080p", k1440p, DXGI_FORMAT_B8G8R8A8_UNORM, true, false},
        {"2160p BGRA8, scaled to 1080p",
         {3840, 2160, "2160p"},
         DXGI_FORMAT_B8G8R8A8_UNORM,
         true,
         false},
        {"1080p BGRA8, VR side by side", k1080p, DXGI_FORMAT_B8G8R8A8_UNORM, true, true},
    };
    for (const Case& c : cases) {
        auto texture = CreateTexture(gpu.device.Get(), c.source.width, c.source.height, c.format,
                                     NoisePixels(c.source.width, c.source.height));
        ASSERT_TRUE(texture) << c.name;
        TextureReader reader;
        if (c.limitTo1080p) reader.SetMaxSize(1920, 1080);
        VrLayout vr;
        vr.enabled = c.vr;
        reader.SetVrLayout(vr);
        UINT width = 0, height = 0;
        std::vector<uint8_t> bgra;
        std::wstring error;
        bool ok = true;
        Stats s = Measure(60, [&] {
            ok = ok && reader.Read(gpu.device.Get(), texture.Get(), width, height, bgra, error);
        });
        ASSERT_TRUE(ok) << c.name << ": " << Narrow(error);
        Print(c.name, s, std::to_string(width) + "x" + std::to_string(height) + " out");
    }
}

TEST(Capture, ScreenshotAsBmp) {
    bool onWarp = false;
    Device gpu = CreateDevice(onWarp);
    ASSERT_TRUE(gpu.device) << "no D3D11 device";
    PrintHeader("SaveTextureAsBmp (F12 screenshot)");
    const Size size = k1080p;
    auto texture = CreateTexture(gpu.device.Get(), size.width, size.height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM, NoisePixels(size.width, size.height));
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    std::wstring path =
        std::wstring(dir) + L"gazeora_bench_" + std::to_wstring(GetCurrentProcessId()) + L".bmp";
    std::wstring error;
    Stats s = Measure(
        20,
        [&] {
            EXPECT_TRUE(SaveTextureAsBmp(gpu.device.Get(), texture.Get(), path, error))
                << Narrow(error);
        },
        2);
    DeleteFileW(path.c_str());
    Print(std::string(size.name) + " to the temp folder", s);
}

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir = path;
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

// Starts the test app with the hook, as Gazeora does with a game, and captures its frames.
TEST(Capture, RealHookPerGraphicsApi) {
    std::wstring testApp = ExeDir() + L"gazeora_testapp.exe";
    std::wstring dll = HookDllPathNextToExe();
    if (GetFileAttributesW(testApp.c_str()) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        GTEST_SKIP()
            << "gazeora_testapp.exe and gazeora_hook.dll must be next to gazeora_bench.exe";
    }
    PrintHeader("Hook in gazeora_testapp.exe (1280x720, vsync)");
    struct Api {
        const wchar_t* flag;
        const char* name;
    };
    const Api apis[] = {{L"", "D3D11"},        {L"--d3d8", "D3D8"},   {L"--d3d9", "D3D9"},
                        {L"--d3d10", "D3D10"}, {L"--d3d12", "D3D12"}, {L"--opengl", "OpenGL"}};
    constexpr int kFrames = 120;
    for (const Api& api : apis) {
        if (api.flag == std::wstring(L"--d3d8") && sizeof(void*) == 8) {
            PrintNote(api.name, "32-bit only, as Windows' D3D8 is: in the x86 release");
            continue;
        }
        LaunchSpec spec;
        spec.exePath = testApp;
        spec.arguments = api.flag;
        CaptureClient client;
        LaunchedGame game;
        std::wstring error;
        double start = QpcSeconds();
        if (!LaunchGameWithHook(spec, dll, client, game, error)) {
            PrintNote(api.name, "launch failed: " + Narrow(error));
            continue;
        }
        double launched = QpcSeconds();
        client.SetCaptureEnabled(true);
        bool gotFirst = false;
        while (!gotFirst && game.Running() && QpcSeconds() - start < 15) {
            gotFirst = client.WaitForFrame(100) == CaptureClient::WaitResult::Frame;
        }
        double firstFrame = QpcSeconds();
        if (!gotFirst) {
            PrintNote(api.name,
                      std::string(game.Running() ? "no frame in 15 s" : "the app exited") +
                          " (hook: " + client.Shared().status + ")");
            if (game.Running()) TerminateProcess(game.process, 0);
            continue;
        }
        LONG64 skippedBefore = client.Shared().skippedFrames;
        // Each frame read back as the stream does, so the timing between frames includes it.
        TextureReader reader;
        std::vector<uint8_t> bgra;
        std::vector<double> intervals, handoff, readBack;
        double previous = QpcSeconds();
        int frames = 0;
        for (; frames < kFrames; ++frames) {
            // A wake-up for a frame already taken returns Timeout; wait on, as the apps do.
            double deadline = QpcSeconds() + 2;
            bool got = false;
            while (!got && QpcSeconds() < deadline) {
                got = client.WaitForFrame(100) == CaptureClient::WaitResult::Frame;
            }
            if (!got) break;
            double now = QpcSeconds();
            intervals.push_back(now - previous);
            handoff.push_back(client.LastHandoffMs() / 1000);
            UINT width = 0, height = 0;
            if (!reader.Read(client.Device(), client.Frame(), width, height, bgra, error)) break;
            readBack.push_back(QpcSeconds() - now);
            previous = now;
        }
        const SharedHeader& shared = client.Shared();
        char note[160];
        snprintf(note, sizeof(note), "%s %ux%u, launch %s, first frame %s, %lld skipped",
                 GraphicsApiName(shared.api), shared.width, shared.height,
                 FormatSeconds(launched - start).c_str(), FormatSeconds(firstFrame - start).c_str(),
                 static_cast<long long>(shared.skippedFrames - skippedBefore));
        PrintNote(api.name, note);
        Print("  present to host copy (LastHandoffMs)", Stats::Of(handoff));
        Print("  TextureReader::Read", Stats::Of(readBack));
        Print("  between captured frames", Stats::Of(intervals),
              frames < kFrames ? "stalled after " + std::to_string(frames) : "");
        TerminateProcess(game.process, 0);
        WaitForSingleObject(game.process, 5000);
    }
}

}  // namespace
}  // namespace gazeora::bench
