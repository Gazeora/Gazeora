#include "platform/onnx_model.h"

#include <functional>
#include <numeric>

#include "gazeora/input/exceptions/input_error.h"

namespace gazeora::input {

namespace {

constexpr int64_t kDynamicDimension = -1;

}  // namespace

OnnxModel::OnnxModel(std::shared_ptr<OnnxRuntimeApi> runtime, const std::wstring& path, int threads)
    : m_runtime(std::move(runtime)), m_api(m_runtime->Api()) {
    try {
        CreateSession(path, threads);
        m_runtime->Check(
            m_api.CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &m_memory));
        ReadNames();
        ReadInputShape();
    } catch (...) {
        Release();
        throw;
    }
}

OnnxModel::~OnnxModel() { Release(); }

void OnnxModel::Release() {
    if (m_memory) {
        m_api.ReleaseMemoryInfo(m_memory);
        m_memory = nullptr;
    }
    if (m_session) {
        m_api.ReleaseSession(m_session);
        m_session = nullptr;
    }
}

void OnnxModel::CreateSession(const std::wstring& path, int threads) {
    OrtSessionOptions* options = nullptr;
    m_runtime->Check(m_api.CreateSessionOptions(&options));
    OrtStatus* status = m_api.SetIntraOpNumThreads(options, threads);
    if (!status) {
        status = m_api.CreateSession(m_runtime->Env(), path.c_str(), options, &m_session);
    }
    m_api.ReleaseSessionOptions(options);
    m_runtime->Check(status);
}

void OnnxModel::ReadNames() {
    size_t inputs = 0, outputs = 0;
    m_runtime->Check(m_api.SessionGetInputCount(m_session, &inputs));
    m_runtime->Check(m_api.SessionGetOutputCount(m_session, &outputs));
    if (inputs != 1) {
        throw InputError(L"A hand tracking model should have exactly one input.");
    }
    m_inputName = Name(0, true);
    for (size_t i = 0; i < outputs; ++i) {
        m_outputNames.push_back(Name(i, false));
    }
}

std::string OnnxModel::Name(size_t index, bool input) const {
    OrtAllocator* allocator = nullptr;
    m_runtime->Check(m_api.GetAllocatorWithDefaultOptions(&allocator));
    char* name = nullptr;
    m_runtime->Check(input ? m_api.SessionGetInputName(m_session, index, allocator, &name)
                           : m_api.SessionGetOutputName(m_session, index, allocator, &name));
    std::string copy = name;
    m_runtime->Check(m_api.AllocatorFree(allocator, name));
    return copy;
}

void OnnxModel::ReadInputShape() {
    OrtTypeInfo* type = nullptr;
    m_runtime->Check(m_api.SessionGetInputTypeInfo(m_session, 0, &type));
    const OrtTensorTypeAndShapeInfo* tensor = nullptr;
    size_t count = 0;
    OrtStatus* status = m_api.CastTypeInfoToTensorInfo(type, &tensor);
    if (!status) {
        status = m_api.GetDimensionsCount(tensor, &count);
    }
    m_inputShape.resize(count);
    if (!status) {
        status = m_api.GetDimensions(tensor, m_inputShape.data(), count);
    }
    m_api.ReleaseTypeInfo(type);
    m_runtime->Check(status);
    for (int64_t& dimension : m_inputShape) {
        dimension = dimension == kDynamicDimension ? 1 : dimension;  // a batch of one
    }
}

size_t OnnxModel::InputSize() const {
    return size_t(std::accumulate(m_inputShape.begin(), m_inputShape.end(), int64_t(1),
                                  std::multiplies<int64_t>()));
}

std::vector<std::vector<float>> OnnxModel::Run(const std::vector<float>& input) {
    if (input.size() != InputSize()) {
        throw InputError(L"A hand tracking model was given the wrong amount of input.");
    }
    OrtValue* tensor = nullptr;
    m_runtime->Check(m_api.CreateTensorWithDataAsOrtValue(
        m_memory, const_cast<float*>(input.data()), input.size() * sizeof(float),
        m_inputShape.data(), m_inputShape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &tensor));
    OwnedValue ownedTensor(tensor, ValueReleaser{&m_api});
    const char* inputName = m_inputName.c_str();
    std::vector<const char*> outputNames;
    for (const std::string& name : m_outputNames) {
        outputNames.push_back(name.c_str());
    }
    std::vector<OrtValue*> outputs(m_outputNames.size(), nullptr);
    OrtStatus* status = m_api.Run(m_session, nullptr, &inputName, &tensor, 1, outputNames.data(),
                                  outputNames.size(), outputs.data());
    std::vector<OwnedValue> ownedOutputs;
    for (OrtValue* output : outputs) {
        ownedOutputs.emplace_back(output, ValueReleaser{&m_api});
    }
    m_runtime->Check(status);
    std::vector<std::vector<float>> results;
    for (const OwnedValue& output : ownedOutputs) {
        results.push_back(Values(output.get()));
    }
    return results;
}

std::vector<float> OnnxModel::Values(OrtValue* value) const {
    float* data = nullptr;
    OrtTensorTypeAndShapeInfo* info = nullptr;
    size_t count = 0;
    m_runtime->Check(m_api.GetTensorMutableData(value, reinterpret_cast<void**>(&data)));
    m_runtime->Check(m_api.GetTensorTypeAndShape(value, &info));
    OrtStatus* status = m_api.GetTensorShapeElementCount(info, &count);
    m_api.ReleaseTensorTypeAndShapeInfo(info);
    m_runtime->Check(status);
    return std::vector<float>(data, data + count);
}

}  // namespace gazeora::input
