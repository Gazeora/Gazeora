#pragma once

#include <vector>

namespace gazeora::input {

// A loaded neural network with one float input and float outputs. Throws InputError if a
// run fails.
class IInferenceModel {
public:
    virtual ~IInferenceModel() = default;

    // The input's size in floats, and each output's values in the model's order.
    virtual size_t InputSize() const = 0;
    virtual std::vector<std::vector<float>> Run(const std::vector<float>& input) = 0;
};

}  // namespace gazeora::input
