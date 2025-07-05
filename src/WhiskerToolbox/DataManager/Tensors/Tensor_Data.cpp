#include "Tensor_Data.hpp"

//#include "npy.hpp"
#include "npy.hpp"

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

#if defined(_WIN32) || defined(__APPLE__)
std::vector<long long> convertShape(const std::vector<unsigned long>& shape) {
    std::vector<long long> convertedShape(shape.size());
    std::transform(shape.begin(), shape.end(), convertedShape.begin(), [](unsigned long i) { return static_cast<long long>(i); });
    return convertedShape;
}
#else
std::vector<long> convertShape(const std::vector<unsigned long>& shape) {
    std::vector<long> convertedShape(shape.size());
    std::transform(shape.begin(), shape.end(), convertedShape.begin(), [](unsigned long i) { return static_cast<long>(i); });
    return convertedShape;
}
#endif

void loadNpyToTensorData(const std::string& filepath, TensorData& tensor_data) {
    if (!std::filesystem::exists(filepath)) {
        std::cout << "File does not exist: " << filepath << std::endl;
        return;
    }

    try {

        auto npy_data = npy::read_npy<float>(filepath);

        auto options = torch::TensorOptions().dtype(torch::kFloat);

        std::cout << "Loaded numpy tensor with " << npy_data.shape << std::endl;

        auto shape = convertShape(npy_data.shape);
        torch::Tensor const tensor = torch::from_blob(&npy_data.data[0], {shape}, options);

        // Assuming the tensor is a 2D tensor where the first dimension is time
        auto const time_steps = tensor.size(0);

        std::map<TimeFrameIndex, torch::Tensor> data;
        for (size_t t = 0; t < time_steps; ++t) {
            data[TimeFrameIndex(t)] = tensor[t].clone();
        }

        std::cout << "Loaded " << data.size() << " timestamps of tensors" << std::endl;

        shape.erase(shape.begin());
        tensor_data = TensorData(data, shape);
    } catch (const std::exception& e) {
        std::cout << "Error loading tensor from file: " << e.what() << std::endl;
    }
}
