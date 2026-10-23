#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "gazeora/input/interfaces/inference_engine.h"

namespace gazeora::input {

class OnnxRuntimeApi;

// Adapter pattern: ONNX Runtime behind IInferenceEngine, on the CPU. The DLL loads with the
// first model, so nothing is paid until hand tracking is used.
class OnnxRuntimeEngine final : public IInferenceEngine {
public:
    // Few threads: tracking shares the CPU with the game being played.
    static constexpr int kDefaultThreads = 2;

    explicit OnnxRuntimeEngine(std::wstring dllPath, int threads = kDefaultThreads);
    ~OnnxRuntimeEngine() override;

    std::unique_ptr<IInferenceModel> Load(const std::wstring& path) override;

private:
    std::shared_ptr<OnnxRuntimeApi> Runtime();

    std::wstring m_dllPath;
    int m_threads;
    std::mutex m_mutex;
    std::shared_ptr<OnnxRuntimeApi> m_runtime;  // guarded by m_mutex
};

}  // namespace gazeora::input
