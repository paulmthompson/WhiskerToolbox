#include "Tensor_Data.hpp"

#include <filesystem>

// ========== Setters ==========

void TensorData::addTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor) {
    _data[time] = tensor;
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor) {
    _data[time] = tensor;
    notifyObservers();
}

// ========== Getters ==========

torch::Tensor TensorData::getTensorAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    }
    return torch::Tensor{};
}

std::vector<TimeFrameIndex> TensorData::getTimesWithTensors() const {
    std::vector<TimeFrameIndex> times;
    times.reserve(_data.size());
    for (const auto& [time, tensor] : _data) {
        times.push_back(time);
    }
    return times;
}

std::vector<float> TensorData::getChannelSlice(TimeFrameIndex time, int channel) const
{

    torch::Tensor const tensor = getTensorAtTime(time);

    //std::cout << "Tensor at time " << time << " with " << tensor.numel() << " elements" << std::endl;

    auto sub_tensor = tensor.index({
                                    torch::indexing::Slice(),
                                    torch::indexing::Slice(),
                                    channel});

    // Apply sigmoid
    sub_tensor = torch::sigmoid(sub_tensor);

    //std::cout << "Sub tensor with" << sub_tensor.numel() << " elements " << std::endl;

    std::vector<float> vec(sub_tensor.data_ptr<float>(), sub_tensor.data_ptr<float>() + sub_tensor.numel());

    //std::cout << "Tensor data has " << vec.size() << " elements" << std::endl;

    return vec;
}