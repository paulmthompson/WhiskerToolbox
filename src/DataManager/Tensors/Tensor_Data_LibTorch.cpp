#include "Tensor_Data.hpp"
#include <algorithm>

#ifdef TENSOR_BACKEND_LIBTORCH

// ========== Template Constructor Implementation ==========
template<typename T>
TensorData::TensorData(std::map<TimeFrameIndex, torch::Tensor> data, std::vector<T> shape)
    : _data(std::move(data)) {
    for (auto s : shape) {
        _feature_shape.push_back(static_cast<std::size_t>(s));
    }
}

// Explicit instantiation for common types
template TensorData::TensorData(std::map<TimeFrameIndex, torch::Tensor>, std::vector<int>);
template TensorData::TensorData(std::map<TimeFrameIndex, torch::Tensor>, std::vector<std::size_t>);
template TensorData::TensorData(std::map<TimeFrameIndex, torch::Tensor>, std::vector<long>);

// ========== LibTorch-specific Setters ==========

void TensorData::addTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor) {
    _data[time] = tensor;
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor) {
    _data[time] = tensor;
    notifyObservers();
}

// ========== Generic Setters ==========

void TensorData::addTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    torch::Tensor tensor = vectorToTensor(data, shape);
    _data[time] = tensor;
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    torch::Tensor tensor = vectorToTensor(data, shape);
    _data[time] = tensor;
    notifyObservers();
}

// ========== LibTorch-specific Getters ==========

torch::Tensor TensorData::getTensorAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    }
    return torch::Tensor{};
}

std::map<TimeFrameIndex, torch::Tensor> const & TensorData::getData() const {
    return _data;
}

// ========== Generic Getters ==========

std::vector<float> TensorData::getTensorDataAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        return tensorToVector(_data.at(time));
    }
    return {};
}

std::vector<std::size_t> TensorData::getTensorShapeAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        const auto& tensor = _data.at(time);
        std::vector<std::size_t> shape;
        for (int i = 0; i < tensor.dim(); ++i) {
            shape.push_back(static_cast<std::size_t>(tensor.size(i)));
        }
        return shape;
    }
    return {};
}

std::vector<TimeFrameIndex> TensorData::getTimesWithTensors() const {
    std::vector<TimeFrameIndex> times;
    times.reserve(_data.size());
    for (const auto& [time, tensor] : _data) {
        times.push_back(time);
    }
    return times;
}

std::vector<float> TensorData::getChannelSlice(TimeFrameIndex time, int channel) const {
    torch::Tensor const tensor = getTensorAtTime(time);
    
    if (!tensor.defined() || tensor.numel() == 0) {
        return {};
    }

    auto sub_tensor = tensor.index({
        torch::indexing::Slice(),
        torch::indexing::Slice(),
        channel
    });

    // Apply sigmoid
    sub_tensor = torch::sigmoid(sub_tensor);

    std::vector<float> vec(sub_tensor.data_ptr<float>(), 
                          sub_tensor.data_ptr<float>() + sub_tensor.numel());

    return vec;
}

std::size_t TensorData::size() const {
    return _data.size();
}

std::vector<std::size_t> TensorData::getFeatureShape() const {
    return _feature_shape;
}

void TensorData::setFeatureShape(const std::vector<std::size_t>& shape) {
    _feature_shape = shape;
}

// ========== Private Helper Methods ==========

torch::Tensor TensorData::vectorToTensor(const std::vector<float>& data, const std::vector<std::size_t>& shape) const {
    // Convert shape to torch IntArrayRef
    std::vector<int64_t> torch_shape;
    torch_shape.reserve(shape.size());
    for (std::size_t dim : shape) {
        torch_shape.push_back(static_cast<int64_t>(dim));
    }
    
    // Create tensor from data
    torch::Tensor tensor = torch::from_blob(
        const_cast<float*>(data.data()), 
        torch_shape, 
        torch::kFloat32
    ).clone(); // Clone to ensure data ownership
    
    return tensor;
}

std::vector<float> TensorData::tensorToVector(const torch::Tensor& tensor) const {
    if (!tensor.defined() || tensor.numel() == 0) {
        return {};
    }
    
    // Ensure tensor is contiguous and on CPU
    torch::Tensor cpu_tensor = tensor.contiguous().to(torch::kCPU);
    
    std::vector<float> vec(cpu_tensor.data_ptr<float>(), 
                          cpu_tensor.data_ptr<float>() + cpu_tensor.numel());
    return vec;
}

#endif // TENSOR_BACKEND_LIBTORCH
