/**
 * @file LazyColumnTensorStorage.cpp
 * @brief Implementation of the lazy column-based tensor storage backend
 *
 * Each column is independently computed via a type-erased provider function,
 * cached on first access, and invalidatable for recomputation.
 */

#include "LazyColumnTensorStorage.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// =============================================================================
// Construction
// =============================================================================

LazyColumnTensorStorage::LazyColumnTensorStorage(
    std::size_t num_rows, std::vector<ColumnSource> columns)
    : _num_rows(num_rows)
    , _columns(std::move(columns))
    , _mutex(std::make_unique<std::mutex>())
{
    if (_num_rows == 0) {
        throw std::invalid_argument(
            "LazyColumnTensorStorage: num_rows must be > 0");
    }
    if (_columns.empty()) {
        throw std::invalid_argument(
            "LazyColumnTensorStorage: must have at least one column");
    }
    for (std::size_t i = 0; i < _columns.size(); ++i) {
        if (!_columns[i].provider) {
            throw std::invalid_argument(
                "LazyColumnTensorStorage: column " + std::to_string(i) +
                " ('" + _columns[i].name + "') has a null provider");
        }
    }
}

// =============================================================================
// Column-Level Control
// =============================================================================

void LazyColumnTensorStorage::materializeColumn(std::size_t col) const {
    validateColumn(col);
    std::lock_guard lock(*_mutex);
    ensureMaterialized(col);
}

void LazyColumnTensorStorage::materializeAll() const {
    std::lock_guard lock(*_mutex);
    for (std::size_t i = 0; i < _columns.size(); ++i) {
        ensureMaterialized(i);
    }
}

void LazyColumnTensorStorage::invalidateColumn(std::size_t col) {
    validateColumn(col);
    std::lock_guard lock(*_mutex);
    _columns[col].cache.reset();
}

void LazyColumnTensorStorage::invalidateAll() {
    std::lock_guard lock(*_mutex);
    for (auto & col : _columns) {
        col.cache.reset();
    }
}

void LazyColumnTensorStorage::setColumnProvider(
    std::size_t col, std::string name, ColumnProviderFn provider) {
    validateColumn(col);
    if (!provider) {
        throw std::invalid_argument(
            "LazyColumnTensorStorage::setColumnProvider: provider must not be null");
    }
    std::lock_guard lock(*_mutex);
    _columns[col].name = std::move(name);
    _columns[col].provider = std::move(provider);
    _columns[col].cache.reset();
}

std::size_t LazyColumnTensorStorage::appendColumn(
    std::string name, ColumnProviderFn provider) {
    if (!provider) {
        throw std::invalid_argument(
            "LazyColumnTensorStorage::appendColumn: provider must not be null");
    }
    std::lock_guard lock(*_mutex);
    auto const new_index = _columns.size();
    _columns.push_back(ColumnSource{std::move(name), std::move(provider), {}});
    return new_index;
}

void LazyColumnTensorStorage::removeColumn(std::size_t col) {
    validateColumn(col);
    if (_columns.size() <= 1) {
        throw std::logic_error(
            "LazyColumnTensorStorage::removeColumn: cannot remove the last column");
    }
    std::lock_guard lock(*_mutex);
    _columns.erase(_columns.begin() + static_cast<std::ptrdiff_t>(col));
}

std::size_t LazyColumnTensorStorage::numColumns() const noexcept {
    return _columns.size();
}

std::size_t LazyColumnTensorStorage::numRows() const noexcept {
    return _num_rows;
}

std::vector<std::string> LazyColumnTensorStorage::columnNames() const {
    std::vector<std::string> names;
    names.reserve(_columns.size());
    for (auto const & col : _columns) {
        names.push_back(col.name);
    }
    return names;
}

std::vector<float> LazyColumnTensorStorage::materializeFlat() const {
    materializeAll();

    auto const num_cols = _columns.size();
    std::vector<float> flat(_num_rows * num_cols);

    // Assemble row-major: flat[row * num_cols + col] = columns[col].cache[row]
    std::lock_guard lock(*_mutex);
    for (std::size_t col = 0; col < num_cols; ++col) {
        auto const & data = *_columns[col].cache;
        for (std::size_t row = 0; row < _num_rows; ++row) {
            flat[row * num_cols + col] = data[row];
        }
    }

    return flat;
}

// =============================================================================
// CRTP Implementation
// =============================================================================

float LazyColumnTensorStorage::getValueAtImpl(
    std::span<std::size_t const> indices) const {

    if (indices.size() != 2) {
        throw std::invalid_argument(
            "LazyColumnTensorStorage::getValueAt: expected 2 indices, got " +
            std::to_string(indices.size()));
    }

    auto const row = indices[0];
    auto const col = indices[1];

    if (row >= _num_rows) {
        throw std::out_of_range(
            "LazyColumnTensorStorage::getValueAt: row " + std::to_string(row) +
            " >= num_rows " + std::to_string(_num_rows));
    }
    validateColumn(col);

    std::lock_guard lock(*_mutex);
    ensureMaterialized(col);
    return (*_columns[col].cache)[row];
}

std::span<float const> LazyColumnTensorStorage::flatDataImpl() const {
    throw std::runtime_error(
        "LazyColumnTensorStorage::flatData: not available (non-contiguous). "
        "Use materializeFlat() or getColumn() instead.");
}

std::vector<float> LazyColumnTensorStorage::sliceAlongAxisImpl(
    std::size_t axis, std::size_t index) const {

    if (axis == 0) {
        // Slice along rows → return one row (all columns at that row)
        if (index >= _num_rows) {
            throw std::out_of_range(
                "LazyColumnTensorStorage::sliceAlongAxis: row index " +
                std::to_string(index) + " >= num_rows " + std::to_string(_num_rows));
        }
        auto const num_cols = _columns.size();
        std::vector<float> result(num_cols);
        std::lock_guard lock(*_mutex);
        for (std::size_t c = 0; c < num_cols; ++c) {
            ensureMaterialized(c);
            result[c] = (*_columns[c].cache)[index];
        }
        return result;
    }

    if (axis == 1) {
        // Slice along columns → return one column (all rows at that column)
        return getColumnImpl(index);
    }

    throw std::out_of_range(
        "LazyColumnTensorStorage::sliceAlongAxis: axis " + std::to_string(axis) +
        " >= ndim 2");
}

std::vector<float> LazyColumnTensorStorage::getColumnImpl(std::size_t col) const {
    validateColumn(col);
    std::lock_guard lock(*_mutex);
    ensureMaterialized(col);
    return *_columns[col].cache;
}

std::vector<std::size_t> LazyColumnTensorStorage::shapeImpl() const {
    return {_num_rows, _columns.size()};
}

std::size_t LazyColumnTensorStorage::totalElementsImpl() const {
    return _num_rows * _columns.size();
}

TensorStorageCache LazyColumnTensorStorage::tryGetCacheImpl() const {
    // Not contiguous — cache is always invalid
    TensorStorageCache cache;
    cache.is_valid = false;
    return cache;
}

// =============================================================================
// Private Helpers
// =============================================================================

void LazyColumnTensorStorage::ensureMaterialized(std::size_t col) const {
    // Caller must hold _mutex
    if (_columns[col].cache.has_value()) {
        return;
    }

    auto data = _columns[col].provider();
    if (data.size() != _num_rows) {
        throw std::runtime_error(
            "LazyColumnTensorStorage: column '" + _columns[col].name +
            "' provider returned " + std::to_string(data.size()) +
            " elements, expected " + std::to_string(_num_rows));
    }
    _columns[col].cache = std::move(data);
}

void LazyColumnTensorStorage::validateColumn(std::size_t col) const {
    if (col >= _columns.size()) {
        throw std::out_of_range(
            "LazyColumnTensorStorage: column index " + std::to_string(col) +
            " >= num_columns " + std::to_string(_columns.size()));
    }
}
