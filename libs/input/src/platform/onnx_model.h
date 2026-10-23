#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gazeora/input/interfaces/inference_model.h"
#include "platform/onnx_runtime_api.h"

namespace gazeora::input {

// Adapter pattern: one ONNX model file as an IInferenceModel, run on the CPU.
class OnnxModel final : public IInferenceModel {
public:
    // Throws InputError if the file can't be loaded.
    OnnxModel(std::shared_ptr<OnnxRuntimeApi> runtime, const std::wstring& path, int threads);
    ~OnnxModel() override;

    OnnxModel(const OnnxModel&) = delete;
    OnnxModel& operator=(const OnnxModel&) = delete;

    size_t InputSize() const override;
    std::vector<std::vector<float>> Run(const std::vector<float>& input) override;

private:
    struct ValueReleaser {
        const OrtApi* api;
        void operator()(OrtValue* value) const {
            if (value) {
                api->ReleaseValue(value);
            }
        }
    };
    using OwnedValue = std::unique_ptr<OrtValue, ValueReleaser>;

    void Release();
    void CreateSession(const std::wstring& path, int threads);
    void ReadNames();
    void ReadInputShape();
    std::string Name(size_t index, bool input) const;
    std::vector<float> Values(OrtValue* value) const;

    std::shared_ptr<OnnxRuntimeApi> m_runtime;
    const OrtApi& m_api;
    OrtSession* m_session = nullptr;
    OrtMemoryInfo* m_memory = nullptr;
    std::vector<int64_t> m_inputShape;
    std::string m_inputName;
    std::vector<std::string> m_outputNames;
};

}  // namespace gazeora::input
