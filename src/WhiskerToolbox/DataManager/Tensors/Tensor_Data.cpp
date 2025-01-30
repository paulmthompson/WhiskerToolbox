#include "Tensor_Data.hpp"

void TensorData::addTensorAtTime(int time, const torch::Tensor& tensor) {
    _data[time] = tensor;
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(int time, const torch::Tensor& tensor) {
    _data[time] = tensor;
    notifyObservers();
}

torch::Tensor TensorData::getTensorAtTime(int time) const {
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    }
    return torch::Tensor();
}

std::vector<int> TensorData::getTimesWithTensors() const {
    std::vector<int> times;
    for (const auto& [time, tensor] : _data) {
        times.push_back(time);
    }
    return times;
}
