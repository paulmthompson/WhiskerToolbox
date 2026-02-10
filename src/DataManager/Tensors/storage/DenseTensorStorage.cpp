#include "DenseTensorStorage.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>

// =============================================================================
// Construction
// =============================================================================

DenseTensorStorage::DenseTensorStorage(std::vector<float> data, std::vector<std::size_t> shape)
    : _data(std::move(data))
    , _shape(std::move(shape))
    , _strides(computeStrides(_shape)) {
    if (_shape.empty()) {
        throw std::invalid_argument(
            "DenseTensorStorage: shape must not be empty");
    }

    auto const expected = std::accumulate(
        _shape.begin(), _shape.end(), std::size_t{1}, std::multiplies<>());

    if (_data.size() != expected) {
        throw std::invalid_argument(
            "DenseTensorStorage: data size (" + std::to_string(_data.size()) +
            ") != product of shape (" + std::to_string(expected) + ")");
    }
}

DenseTensorStorage::DenseTensorStorage(std::vector<std::size_t> shape)
    : _shape(std::move(shape))
    , _strides(computeStrides(_shape)) {
    if (_shape.empty()) {
        throw std::invalid_argument(
            "DenseTensorStorage: shape must not be empty");
    }

    auto const total = std::accumulate(
        _shape.begin(), _shape.end(), std::size_t{1}, std::multiplies<>());
    _data.resize(total, 0.0f);
}

// =============================================================================
// Mutable Access
// =============================================================================

std::span<float> DenseTensorStorage::mutableFlatData() {
    return {_data.data(), _data.size()};
}

void DenseTensorStorage::setValueAt(std::span<std::size_t const> indices, float value) {
    validateIndices(indices);
    _data[flatOffset(indices)] = value;
}

// =============================================================================
// CRTP Implementation
// =============================================================================

float DenseTensorStorage::getValueAtImpl(std::span<std::size_t const> indices) const {
    validateIndices(indices);
    return _data[flatOffset(indices)];
}

std::span<float const> DenseTensorStorage::flatDataImpl() const {
    return {_data.data(), _data.size()};
}

std::vector<float> DenseTensorStorage::sliceAlongAxisImpl(
    std::size_t axis, std::size_t index) const {

    if (axis >= _shape.size()) {
        throw std::out_of_range(
            "DenseTensorStorage::sliceAlongAxis: axis " + std::to_string(axis) +
            " >= ndim " + std::to_string(_shape.size()));
    }
    if (index >= _shape[axis]) {
        throw std::out_of_range(
            "DenseTensorStorage::sliceAlongAxis: index " + std::to_string(index) +
            " >= shape[" + std::to_string(axis) + "] = " + std::to_string(_shape[axis]));
    }

    // Compute the shape of the result (all dimensions except the sliced one)
    std::vector<std::size_t> result_shape;
    result_shape.reserve(_shape.size() - 1);
    for (std::size_t d = 0; d < _shape.size(); ++d) {
        if (d != axis) {
            result_shape.push_back(_shape[d]);
        }
    }

    // If we're slicing a 1D tensor, return a single element
    if (result_shape.empty()) {
        return {_data[index]};
    }

    auto const result_total = std::accumulate(
        result_shape.begin(), result_shape.end(), std::size_t{1}, std::multiplies<>());
    std::vector<float> result(result_total);

    // Iterate over all positions in the result tensor.
    // For each result position, reconstruct the source index by inserting
    // `index` at position `axis`.
    std::vector<std::size_t> result_indices(result_shape.size(), 0);
    std::vector<std::size_t> source_indices(_shape.size(), 0);

    for (std::size_t i = 0; i < result_total; ++i) {
        // Build source indices: insert the fixed index at the sliced axis
        std::size_t ri = 0;
        for (std::size_t d = 0; d < _shape.size(); ++d) {
            if (d == axis) {
                source_indices[d] = index;
            } else {
                source_indices[d] = result_indices[ri];
                ++ri;
            }
        }

        result[i] = _data[flatOffset(source_indices)];

        // Increment result_indices (row-major odometer)
        for (auto d = static_cast<std::ptrdiff_t>(result_indices.size()) - 1; d >= 0; --d) {
            auto ud = static_cast<std::size_t>(d);
            ++result_indices[ud];
            if (result_indices[ud] < result_shape[ud]) {
                break;
            }
            result_indices[ud] = 0;
        }
    }

    return result;
}

std::vector<float> DenseTensorStorage::getColumnImpl(std::size_t col) const {
    if (_shape.size() < 2) {
        throw std::invalid_argument(
            "DenseTensorStorage::getColumn: not supported for " +
            std::to_string(_shape.size()) + "D tensor (need at least 2D)");
    }

    // Column axis is axis 1
    constexpr std::size_t column_axis = 1;
    if (col >= _shape[column_axis]) {
        throw std::out_of_range(
            "DenseTensorStorage::getColumn: col " + std::to_string(col) +
            " >= shape[1] = " + std::to_string(_shape[column_axis]));
    }

    // For a 2D tensor [nrows, ncols]: result is nrows elements.
    // For higher dims: result is (product of all dims except axis 1) elements,
    // which equals totalElements / shape[1].
    auto const result_size = _data.size() / _shape[column_axis];
    std::vector<float> result;
    result.reserve(result_size);

    // Iterate through flat data. An element at flat offset `i` belongs to
    // column `c` if: (i / stride[1]) % shape[1] == c
    auto const stride_1 = _strides[column_axis];
    auto const shape_1 = _shape[column_axis];

    for (std::size_t i = 0; i < _data.size(); ++i) {
        if ((i / stride_1) % shape_1 == col) {
            result.push_back(_data[i]);
        }
    }

    return result;
}

std::vector<std::size_t> DenseTensorStorage::shapeImpl() const {
    return _shape;
}

std::size_t DenseTensorStorage::totalElementsImpl() const {
    return _data.size();
}

TensorStorageCache DenseTensorStorage::tryGetCacheImpl() const {
    TensorStorageCache cache;
    cache.data_ptr = _data.data();
    cache.total_elements = _data.size();
    cache.shape = _shape;
    cache.strides = _strides;
    cache.is_valid = true;
    return cache;
}

// =============================================================================
// Private Helpers
// =============================================================================

std::size_t DenseTensorStorage::flatOffset(std::span<std::size_t const> indices) const noexcept {
    std::size_t offset = 0;
    for (std::size_t d = 0; d < indices.size(); ++d) {
        offset += indices[d] * _strides[d];
    }
    return offset;
}

std::vector<std::size_t> DenseTensorStorage::computeStrides(
    std::vector<std::size_t> const & shape) {
    if (shape.empty()) {
        return {};
    }

    std::vector<std::size_t> strides(shape.size());
    strides.back() = 1;
    for (auto i = static_cast<std::ptrdiff_t>(shape.size()) - 2; i >= 0; --i) {
        auto ui = static_cast<std::size_t>(i);
        strides[ui] = strides[ui + 1] * shape[ui + 1];
    }
    return strides;
}

void DenseTensorStorage::validateIndices(std::span<std::size_t const> indices) const {
    if (indices.size() != _shape.size()) {
        throw std::invalid_argument(
            "DenseTensorStorage: expected " + std::to_string(_shape.size()) +
            " indices, got " + std::to_string(indices.size()));
    }
    for (std::size_t d = 0; d < indices.size(); ++d) {
        if (indices[d] >= _shape[d]) {
            throw std::out_of_range(
                "DenseTensorStorage: index[" + std::to_string(d) + "] = " +
                std::to_string(indices[d]) + " >= shape[" + std::to_string(d) +
                "] = " + std::to_string(_shape[d]));
        }
    }
}
