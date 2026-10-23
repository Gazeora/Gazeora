#pragma once

#include <windows.h>

#include <string>

#include <onnxruntime_c_api.h>

namespace gazeora::input {

// onnxruntime.dll, loaded from a given path at run time (it ships only an MSVC import
// library), with its C API and one logging environment. Kept alive by every model loaded
// through it.
class OnnxRuntimeApi {
public:
    // Throws InputError if the DLL is missing or too old.
    explicit OnnxRuntimeApi(const std::wstring& dllPath);
    ~OnnxRuntimeApi();

    OnnxRuntimeApi(const OnnxRuntimeApi&) = delete;
    OnnxRuntimeApi& operator=(const OnnxRuntimeApi&) = delete;

    const OrtApi& Api() const { return *m_api; }
    OrtEnv* Env() const { return m_env; }
    // Throws InputError with ONNX Runtime's message if `status` is an error.
    void Check(OrtStatus* status) const;

private:
    HMODULE m_dll = nullptr;
    const OrtApi* m_api = nullptr;
    OrtEnv* m_env = nullptr;
};

}  // namespace gazeora::input
