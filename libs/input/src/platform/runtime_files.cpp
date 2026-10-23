#include "gazeora/input/platform/runtime_files.h"

#include <windows.h>

namespace gazeora::input {

namespace {

constexpr wchar_t kOnnxRuntimeDll[] = L"onnxruntime.dll";
constexpr wchar_t kPalmModel[] = L"models\\palm_detection_mediapipe_2023feb.onnx";
constexpr wchar_t kLandmarkModel[] = L"models\\handpose_estimation_mediapipe_2023feb.onnx";
constexpr DWORD kLongPath = 32768;

}  // namespace

std::wstring RuntimeFiles::OnnxRuntimeDll() { return ExeFolder() + kOnnxRuntimeDll; }

std::wstring RuntimeFiles::PalmModel() { return ExeFolder() + kPalmModel; }

std::wstring RuntimeFiles::LandmarkModel() { return ExeFolder() + kLandmarkModel; }

std::wstring RuntimeFiles::ExeFolder() {
    std::wstring path(kLongPath, L'\0');
    path.resize(GetModuleFileNameW(nullptr, path.data(), kLongPath));
    return path.substr(0, path.find_last_of(L"\\/") + 1);
}

}  // namespace gazeora::input
