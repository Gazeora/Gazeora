#include "gazeora/input/platform/onnx_runtime_engine.h"

#include "platform/onnx_model.h"
#include "platform/onnx_runtime_api.h"

namespace gazeora::input {

OnnxRuntimeEngine::OnnxRuntimeEngine(std::wstring dllPath, int threads)
    : m_dllPath(std::move(dllPath)), m_threads(threads) {}

OnnxRuntimeEngine::~OnnxRuntimeEngine() = default;

std::unique_ptr<IInferenceModel> OnnxRuntimeEngine::Load(const std::wstring& path) {
    return std::make_unique<OnnxModel>(Runtime(), path, m_threads);
}

std::shared_ptr<OnnxRuntimeApi> OnnxRuntimeEngine::Runtime() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_runtime) {
        m_runtime = std::make_shared<OnnxRuntimeApi>(m_dllPath);
    }
    return m_runtime;
}

}  // namespace gazeora::input
