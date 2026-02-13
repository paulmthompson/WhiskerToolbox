#include "DimensionDescriptor.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

DimensionDescriptor::DimensionDescriptor(std::vector<AxisDescriptor> axes)
    : _axes(std::move(axes)) {
    validateAxes();
    computeStrides();
}

std::size_t DimensionDescriptor::totalElements() const noexcept {
    if (_axes.empty()) {
        return 1; // scalar
    }
    return std::accumulate(
        _axes.begin(), _axes.end(), std::size_t{1},
        [](std::size_t acc, AxisDescriptor const & a) { return acc * a.size; });
}

std::vector<std::size_t> DimensionDescriptor::shape() const {
    std::vector<std::size_t> s;
    s.reserve(_axes.size());
    for (auto const & a : _axes) {
        s.push_back(a.size);
    }
    return s;
}

AxisDescriptor const & DimensionDescriptor::axis(std::size_t i) const {
    if (i >= _axes.size()) {
        throw std::out_of_range(
            "DimensionDescriptor::axis: index " + std::to_string(i) +
            " out of range (ndim=" + std::to_string(_axes.size()) + ")");
    }
    return _axes[i];
}

std::optional<std::size_t> DimensionDescriptor::findAxis(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < _axes.size(); ++i) {
        if (_axes[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

std::size_t DimensionDescriptor::flatIndex(std::vector<std::size_t> const & indices) const {
    if (indices.size() != _axes.size()) {
        throw std::invalid_argument(
            "DimensionDescriptor::flatIndex: expected " + std::to_string(_axes.size()) +
            " indices, got " + std::to_string(indices.size()));
    }
    std::size_t offset = 0;
    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= _axes[i].size) {
            throw std::out_of_range(
                "DimensionDescriptor::flatIndex: index[" + std::to_string(i) +
                "]=" + std::to_string(indices[i]) +
                " out of range for axis '" + _axes[i].name +
                "' (size=" + std::to_string(_axes[i].size) + ")");
        }
        offset += indices[i] * _strides[i];
    }
    return offset;
}

void DimensionDescriptor::setColumnNames(std::vector<std::string> names) {
    if (_axes.empty()) {
        throw std::invalid_argument(
            "DimensionDescriptor::setColumnNames: cannot set column names on a scalar tensor");
    }
    if (names.size() != _axes.back().size) {
        throw std::invalid_argument(
            "DimensionDescriptor::setColumnNames: expected " +
            std::to_string(_axes.back().size) +
            " names (last axis size), got " + std::to_string(names.size()));
    }
    _column_names = std::move(names);
}

std::optional<std::size_t> DimensionDescriptor::findColumn(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < _column_names.size(); ++i) {
        if (_column_names[i] == name) {
            return i;
        }
    }
    return std::nullopt;
}

bool DimensionDescriptor::operator==(DimensionDescriptor const & other) const {
    return _axes == other._axes && _column_names == other._column_names;
}

void DimensionDescriptor::computeStrides() {
    _strides.resize(_axes.size());
    if (_axes.empty()) {
        return;
    }
    // Row-major: stride[last] = 1, stride[i] = stride[i+1] * size[i+1]
    _strides.back() = 1;
    for (std::size_t i = _axes.size() - 1; i > 0; --i) {
        _strides[i - 1] = _strides[i] * _axes[i].size;
    }
}

void DimensionDescriptor::validateAxes() const {
    // Check for zero-size axes
    for (auto const & a : _axes) {
        if (a.size == 0) {
            throw std::invalid_argument(
                "DimensionDescriptor: axis '" + a.name + "' has size 0");
        }
    }

    // Check for duplicate names (empty names are allowed but must be unique)
    std::unordered_set<std::string> seen;
    for (auto const & a : _axes) {
        if (!a.name.empty()) {
            auto [it, inserted] = seen.insert(a.name);
            if (!inserted) {
                throw std::invalid_argument(
                    "DimensionDescriptor: duplicate axis name '" + a.name + "'");
            }
        }
    }
}
