#include "RuntimeModel.hpp"

#include <stdexcept>

namespace dl {

RuntimeModel::RuntimeModel(RuntimeModelSpec spec)
    : _spec(std::move(spec))
    , _input_slots(_spec.inputDescriptors())
    , _output_slots(_spec.outputDescriptors())
{
    _input_order.reserve(_input_slots.size());
    for (auto const & slot : _input_slots) {
        _input_order.push_back(slot.name);
    }

    _output_order.reserve(_output_slots.size());
    for (auto const & slot : _output_slots) {
        _output_order.push_back(slot.name);
    }

    // Auto-load weights if specified in the spec
    if (_spec.weights_path.has_value() && !_spec.weights_path->empty()) {
        _execution.load(_spec.weights_path.value());
    }
}

std::string RuntimeModel::modelId() const
{
    return _spec.model_id;
}

std::string RuntimeModel::displayName() const
{
    return _spec.display_name;
}

std::string RuntimeModel::description() const
{
    return _spec.description.value_or("");
}

std::vector<TensorSlotDescriptor> RuntimeModel::inputSlots() const
{
    return _input_slots;
}

std::vector<TensorSlotDescriptor> RuntimeModel::outputSlots() const
{
    return _output_slots;
}

void RuntimeModel::loadWeights(std::filesystem::path const & path)
{
    _execution.load(path);
}

bool RuntimeModel::isReady() const
{
    return _execution.isLoaded();
}

int RuntimeModel::preferredBatchSize() const
{
    return _spec.preferred_batch_size.value_or(0);
}

int RuntimeModel::maxBatchSize() const
{
    return _spec.max_batch_size.value_or(0);
}

std::unordered_map<std::string, torch::Tensor>
RuntimeModel::forward(std::unordered_map<std::string, torch::Tensor> const & inputs)
{
    if (!isReady()) {
        throw std::runtime_error(
            "RuntimeModel::forward(): model '" + _spec.model_id +
            "' not ready (weights not loaded)");
    }

    auto output_tensors = _execution.executeNamed(inputs, _input_order);

    std::unordered_map<std::string, torch::Tensor> result;
    for (std::size_t i = 0; i < output_tensors.size() && i < _output_order.size(); ++i) {
        result[_output_order[i]] = std::move(output_tensors[i]);
    }
    return result;
}

RuntimeModelSpec const & RuntimeModel::spec() const
{
    return _spec;
}

} // namespace dl
