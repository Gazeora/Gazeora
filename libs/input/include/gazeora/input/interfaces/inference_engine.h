#pragma once

#include <memory>
#include <string>

#include "gazeora/input/interfaces/inference_model.h"

namespace gazeora::input {

// Factory pattern: loads model files. Throws InputError if the engine or the file is missing.
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;

    virtual std::unique_ptr<IInferenceModel> Load(const std::wstring& path) = 0;
};

}  // namespace gazeora::input
