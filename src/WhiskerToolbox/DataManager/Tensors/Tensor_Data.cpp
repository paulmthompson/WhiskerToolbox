#include "Tensor_Data.hpp"

//#include "npy.hpp"
#include "npy.hpp"

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
        std::cout << "File does not exist: " << filepath << std::endl;
        return;
    }

    try {

        auto npy_data = npy::read_npy<float>(filepath);

        auto options = torch::TensorOptions().dtype(torch::kFloat);

        std::cout << "Loaded numpy tensor with " << npy_data.shape << std::endl;

        std::vector<long long> shape(npy_data.shape.size());
        std::transform(npy_data.shape.begin(), npy_data.shape.end(), shape.begin(), [](unsigned long i) { return static_cast<long long>(i); });

        torch::Tensor tensor = torch::from_blob(&npy_data.data[0], {shape}, options);

        // Assuming the tensor is a 2D tensor where the first dimension is time
        int time_steps = tensor.size(0);

        std::map<int, torch::Tensor> data;
        for (int t = 0; t < time_steps; ++t) {
            data[t] = tensor[t];
        }

        std::cout << "Loaded " << data.size() << " timestamps of tensors" << std::endl;

        tensor_data = TensorData(data);
    } catch (const std::exception& e) {
        std::cout << "Error loading tensor from file: " << e.what() << std::endl;
    }
}
