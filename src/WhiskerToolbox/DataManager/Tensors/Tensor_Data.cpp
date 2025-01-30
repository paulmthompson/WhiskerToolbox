#include "Tensor_Data.hpp"

#include <filesystem>

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

void loadNpyToTensorData(const std::string& filepath, TensorData& tensor_data) {
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "File does not exist: " << filepath << std::endl;
        return;
    }

    try {
        // Load the tensor from the .npy file
        torch::Tensor tensor = torch::from_file(filepath);

        // Assuming the tensor is a 2D tensor where the first dimension is time
        int time_steps = tensor.size(0);

        std::map<int, torch::Tensor> data;
        for (int t = 0; t < time_steps; ++t) {
            data[t] = tensor[t];
        }

        tensor_data = TensorData(data);
    } catch (const std::exception& e) {
        std::cerr << "Error loading tensor from file: " << e.what() << std::endl;
    }
}
