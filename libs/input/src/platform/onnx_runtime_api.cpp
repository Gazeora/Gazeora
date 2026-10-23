#include "platform/onnx_runtime_api.h"

#include "gazeora/common/text/utf8.h"
#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

constexpr char kLogId[] = "gazeora";
using GetApiBase = const OrtApiBase*(ORT_API_CALL*)();

const OrtApi* LoadApi(HMODULE dll) {
    auto getApiBase =
        reinterpret_cast<GetApiBase>(reinterpret_cast<void*>(GetProcAddress(dll, "OrtGetApiBase")));
    const OrtApiBase* base = getApiBase ? getApiBase() : nullptr;
    return base ? base->GetApi(ORT_API_VERSION) : nullptr;
}

}  // namespace

OnnxRuntimeApi::OnnxRuntimeApi(const std::wstring& dllPath) {
    // The full path, so Windows' own older copy in System32 isn't picked up instead.
    m_dll = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m_dll) {
        throw InputError(L"ONNX Runtime is missing: " + dllPath + L". Rebuild Gazeora.");
    }
    m_api = LoadApi(m_dll);
    if (!m_api) {
        FreeLibrary(m_dll);
        throw InputError(L"ONNX Runtime is too old: " + dllPath + L". Rebuild Gazeora.");
    }
    Check(m_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, kLogId, &m_env));
}

OnnxRuntimeApi::~OnnxRuntimeApi() {
    if (m_env) {
        m_api->ReleaseEnv(m_env);
    }
    FreeLibrary(m_dll);
}

void OnnxRuntimeApi::Check(OrtStatus* status) const {
    if (!status) {
        return;
    }
    std::string message = m_api->GetErrorMessage(status);
    m_api->ReleaseStatus(status);
    throw InputError(L"ONNX Runtime: " + Utf8::ToWide(message));
}

}  // namespace gazeora::input
