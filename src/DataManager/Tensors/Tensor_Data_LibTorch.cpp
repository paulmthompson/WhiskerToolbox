#include "Tensor_Data.hpp"
#include "Tensor_Data_Impl.hpp"
#include <algorithm>

#ifdef TENSOR_BACKEND_LIBTORCH
#include <torch/torch.h>

// ========== TensorData PIMPL Implementation ==========

// ========== Constructors ==========

TensorData::TensorData() : _pimpl(std::make_unique<TensorDataImpl>()) {}

TensorData::~TensorData() = default;

TensorData::TensorData(const TensorData& other) 
    : ObserverData(other)
    , _pimpl(std::make_unique<TensorDataImpl>(*other._pimpl))
    , _time_frame(other._time_frame) {}

TensorData::TensorData(TensorData&& other) noexcept 
    : ObserverData(std::move(other))
    , _pimpl(std::move(other._pimpl))
    , _time_frame(std::move(other._time_frame)) {}

TensorData& TensorData::operator=(const TensorData& other) {
    if (this != &other) {
        _pimpl = std::make_unique<TensorDataImpl>(*other._pimpl);
        _time_frame = other._time_frame;
    }
    return *this;
}

TensorData& TensorData::operator=(TensorData&& other) noexcept {
    if (this != &other) {
        _pimpl = std::move(other._pimpl);
        _time_frame = std::move(other._time_frame);
    }
    return *this;
}

#ifdef TENSOR_BACKEND_LIBTORCH
template<typename T>
TensorData::TensorData(std::map<TimeFrameIndex, void*> data, std::vector<T> shape) {
    // Convert void* back to torch::Tensor for internal storage
    std::map<TimeFrameIndex, torch::Tensor> tensor_data;
    for (const auto& [time, tensor_ptr] : data) {
        tensor_data[time] = *static_cast<torch::Tensor*>(tensor_ptr);
    }
    _pimpl = std::make_unique<TensorDataImpl>(std::move(tensor_data), shape);
}

// Explicit instantiation for common types
template TensorData::TensorData(std::map<TimeFrameIndex, void*>, std::vector<int>);
template TensorData::TensorData(std::map<TimeFrameIndex, void*>, std::vector<std::size_t>);
template TensorData::TensorData(std::map<TimeFrameIndex, void*>, std::vector<long>);
#endif

// ========== Setters ==========

#ifdef TENSOR_BACKEND_LIBTORCH
void TensorData::addTensorAtTime(TimeFrameIndex time, void* tensor) {
    _pimpl->addTensorAtTime(time, *static_cast<torch::Tensor*>(tensor));
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(TimeFrameIndex time, void* tensor) {
    _pimpl->overwriteTensorAtTime(time, *static_cast<torch::Tensor*>(tensor));
    notifyObservers();
}
#endif

void TensorData::addTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    _pimpl->addTensorAtTime(time, data, shape);
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    _pimpl->overwriteTensorAtTime(time, data, shape);
    notifyObservers();
}

// ========== Getters ==========

#ifdef TENSOR_BACKEND_LIBTORCH
void* TensorData::getTensorAtTime(TimeFrameIndex time) const {
    auto tensor = _pimpl->getTensorAtTime(time);
    if (!tensor.defined()) {
        return nullptr;
    }
    // Return a pointer to the tensor (caller must manage lifetime)
    return new torch::Tensor(tensor);
}

std::map<TimeFrameIndex, void*> TensorData::getData() const {
    std::map<TimeFrameIndex, void*> result;
    auto times = _pimpl->getTimesWithTensors();
    for (const auto& time : times) {
        auto tensor = _pimpl->getTensorAtTime(time);
        if (tensor.defined()) {
            result[time] = new torch::Tensor(tensor);
        }
    }
    return result;
}
#endif

std::vector<float> TensorData::getTensorDataAtTime(TimeFrameIndex time) const {
    return _pimpl->getTensorDataAtTime(time);
}

std::vector<std::size_t> TensorData::getTensorShapeAtTime(TimeFrameIndex time) const {
    return _pimpl->getTensorShapeAtTime(time);
}

std::vector<TimeFrameIndex> TensorData::getTimesWithTensors() const {
    return _pimpl->getTimesWithTensors();
}

std::vector<float> TensorData::getChannelSlice(TimeFrameIndex time, int channel) const {
    return _pimpl->getChannelSlice(time, channel);
}

std::size_t TensorData::size() const {
    return _pimpl->size();
}

std::vector<std::size_t> TensorData::getFeatureShape() const {
    return _pimpl->getFeatureShape();
}

void TensorData::setFeatureShape(const std::vector<std::size_t>& shape) {
    _pimpl->setFeatureShape(shape);
}

// ========== TensorDataImpl Implementation ==========

// ========== Constructors ==========

TensorDataImpl::TensorDataImpl(const TensorDataImpl& other) = default;

TensorDataImpl::TensorDataImpl(TensorDataImpl&& other) noexcept = default;

TensorDataImpl& TensorDataImpl::operator=(const TensorDataImpl& other) = default;

TensorDataImpl& TensorDataImpl::operator=(TensorDataImpl&& other) noexcept = default;

// ========== Template Constructor Implementation ==========
template<typename T>
TensorDataImpl::TensorDataImpl(std::map<TimeFrameIndex, torch::Tensor> data, std::vector<T> shape)
    : _data(std::move(data)) {
    for (auto s : shape) {
        _feature_shape.push_back(static_cast<std::size_t>(s));
    }
}

// Explicit instantiation for common types
template TensorDataImpl::TensorDataImpl(std::map<TimeFrameIndex, torch::Tensor>, std::vector<int>);
template TensorDataImpl::TensorDataImpl(std::map<TimeFrameIndex, torch::Tensor>, std::vector<std::size_t>);
template TensorDataImpl::TensorDataImpl(std::map<TimeFrameIndex, torch::Tensor>, std::vector<long>);

// ========== LibTorch-specific Setters ==========

void TensorDataImpl::addTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor) {
    _data[time] = tensor;
}

void TensorDataImpl::overwriteTensorAtTime(TimeFrameIndex time, torch::Tensor const & tensor) {
    _data[time] = tensor;
}

// ========== Generic Setters ==========

void TensorDataImpl::addTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    torch::Tensor tensor = vectorToTensor(data, shape);
    _data[time] = tensor;
}

void TensorDataImpl::overwriteTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    torch::Tensor tensor = vectorToTensor(data, shape);
    _data[time] = tensor;
}

// ========== LibTorch-specific Getters ==========

torch::Tensor TensorDataImpl::getTensorAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    }
    return torch::Tensor{};
}

std::map<TimeFrameIndex, torch::Tensor> const & TensorDataImpl::getData() const {
    return _data;
}

// ========== Generic Getters ==========

std::vector<float> TensorDataImpl::getTensorDataAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        return tensorToVector(_data.at(time));
    }
    return {};
}

std::vector<std::size_t> TensorDataImpl::getTensorShapeAtTime(TimeFrameIndex time) const {
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

std::vector<TimeFrameIndex> TensorDataImpl::getTimesWithTensors() const {
    std::vector<TimeFrameIndex> times;
    times.reserve(_data.size());
    for (const auto& [time, tensor] : _data) {
        times.push_back(time);
    }
    return times;
}

std::vector<float> TensorDataImpl::getChannelSlice(TimeFrameIndex time, int channel) const {
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

std::size_t TensorDataImpl::size() const {
    return _data.size();
}

std::vector<std::size_t> TensorDataImpl::getFeatureShape() const {
    return _feature_shape;
}

void TensorDataImpl::setFeatureShape(const std::vector<std::size_t>& shape) {
    _feature_shape = shape;
}

// ========== Private Helper Methods ==========

torch::Tensor TensorDataImpl::vectorToTensor(const std::vector<float>& data, const std::vector<std::size_t>& shape) const {
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

std::vector<float> TensorDataImpl::tensorToVector(const torch::Tensor& tensor) const {
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
