#include "Tensor_Data.hpp"
#include "Tensor_Data_Impl.hpp"
#include <armadillo>
#include <algorithm>
#include <cmath>

#ifndef TENSOR_BACKEND_LIBTORCH

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

// ========== Setters ==========

void TensorData::addTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    _pimpl->addTensorAtTime(time, data, shape);
    notifyObservers();
}

void TensorData::overwriteTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    _pimpl->overwriteTensorAtTime(time, data, shape);
    notifyObservers();
}

// ========== Getters ==========

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

// ========== Generic Setters ==========

void TensorDataImpl::addTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    arma::fcube cube = vectorToCube(data, shape);
    _data[time] = cube;
}

void TensorDataImpl::overwriteTensorAtTime(TimeFrameIndex time, const std::vector<float>& data, const std::vector<std::size_t>& shape) {
    arma::fcube cube = vectorToCube(data, shape);
    _data[time] = cube;
}

// ========== Generic Getters ==========

std::vector<float> TensorDataImpl::getTensorDataAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        return cubeToVector(_data.at(time));
    }
    return {};
}

std::vector<std::size_t> TensorDataImpl::getTensorShapeAtTime(TimeFrameIndex time) const {
    if (_data.find(time) != _data.end()) {
        const auto& cube = _data.at(time);
        return {cube.n_rows, cube.n_cols, cube.n_slices};
    }
    return {};
}

std::vector<TimeFrameIndex> TensorDataImpl::getTimesWithTensors() const {
    std::vector<TimeFrameIndex> times;
    times.reserve(_data.size());
    for (const auto& [time, cube] : _data) {
        times.push_back(time);
    }
    return times;
}

std::vector<float> TensorDataImpl::getChannelSlice(TimeFrameIndex time, int channel) const {
    if (_data.find(time) == _data.end()) {
        return {};
    }
    
    const arma::fcube& cube = _data.at(time);
    
    if (cube.is_empty() || channel >= static_cast<int>(cube.n_slices)) {
        return {};
    }

    // Extract the channel slice (2D matrix)
    arma::fmat slice = cube.slice(channel);
    
    // Apply sigmoid to the slice
    arma::fmat sigmoid_slice = applySigmoid(slice);

    // Convert to vector
    std::vector<float> vec(sigmoid_slice.begin(), sigmoid_slice.end());
    
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

arma::fcube TensorDataImpl::vectorToCube(const std::vector<float>& data, const std::vector<std::size_t>& shape) const {
    if (shape.size() != 3) {
        throw std::invalid_argument("Armadillo backend currently supports only 3D tensors (cubes)");
    }
    
    arma::uword n_rows = shape[0];
    arma::uword n_cols = shape[1];
    arma::uword n_slices = shape[2];
    
    if (data.size() != n_rows * n_cols * n_slices) {
        throw std::invalid_argument("Data size does not match specified shape");
    }
    
    // Create cube with the specified dimensions
    arma::fcube cube(n_rows, n_cols, n_slices);
    
    // Copy data into the cube
    // Armadillo uses column-major order
    std::copy(data.begin(), data.end(), cube.begin());
    
    return cube;
}

std::vector<float> TensorDataImpl::cubeToVector(const arma::fcube& cube) const {
    if (cube.is_empty()) {
        return {};
    }
    
    std::vector<float> vec(cube.begin(), cube.end());
    return vec;
}

arma::fmat TensorDataImpl::applySigmoid(const arma::fmat& mat) const {
    // Apply sigmoid function: 1 / (1 + exp(-x))
    return 1.0f / (1.0f + arma::exp(-mat));
}

#endif // !TENSOR_BACKEND_LIBTORCH
