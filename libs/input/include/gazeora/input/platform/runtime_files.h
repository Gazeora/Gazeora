#pragma once

#include <string>

namespace gazeora::input {

// Where hand tracking's files sit next to the executable: the build copies onnxruntime.dll
// beside it and the models into models\.
class RuntimeFiles {
public:
    static std::wstring OnnxRuntimeDll();
    static std::wstring PalmModel();
    static std::wstring LandmarkModel();

private:
    static std::wstring ExeFolder();
};

}  // namespace gazeora::input
