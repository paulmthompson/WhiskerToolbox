/**
 * @file TensorColumnAnalogStorage.hpp
 * @brief Zero-copy analog storage backend referencing a TensorData column.
 *
 * Phase 3 of the multi-channel storage roadmap. Provides a lightweight
 * AnalogTimeSeries view into a column of a TensorData object. When the
 * tensor uses Armadillo storage (column-major), span access is contiguous
 * and zero-copy. For other backends, element access falls back to
 * TensorData::at().
 */

#ifndef TENSOR_COLUMN_ANALOG_STORAGE_HPP
#define TENSOR_COLUMN_ANALOG_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/ArmadilloTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>

/**
 * @brief Zero-copy analog storage backed by a TensorData column.
 *
 * Holds shared ownership of the source TensorData and a column index.
 * When the tensor uses Armadillo storage (column-major layout), each
 * column is contiguous in memory, enabling zero-copy span access via
 * `arma::fmat::colptr()`. For non-Armadillo backends, element access
 * goes through `TensorData::at()`.
 *
 * @pre The source tensor must be 2D.
 * @pre column_index must be in [0, tensor->numColumns()).
 */
class TensorColumnAnalogStorage : public AnalogDataStorageBase<TensorColumnAnalogStorage> {
public:
    /**
     * @brief Construct storage referencing a specific column of a TensorData.
     *
     * @param tensor Shared pointer to the source tensor (const; view is read-only).
     * @param column_index The column index to view.
     *
     * @pre tensor != nullptr
     * @pre tensor->ndim() == 2
     * @pre column_index < tensor->numColumns()
     */
    TensorColumnAnalogStorage(
            std::shared_ptr<TensorData const> tensor,
            std::size_t column_index)
        : _tensor(std::move(tensor)),
          _column_index(column_index) {
        assert(_tensor != nullptr && "TensorColumnAnalogStorage: tensor must not be null");
        assert(_tensor->ndim() == 2 && "TensorColumnAnalogStorage: tensor must be 2D");
        assert(_column_index < _tensor->numColumns() &&
               "TensorColumnAnalogStorage: column_index out of range");

        _cacheColumnPointer();
    }

    ~TensorColumnAnalogStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::size_t index) const {
        if (_col_ptr) {
            return _col_ptr[index];
        }
        std::size_t indices[2] = {index, _column_index};
        return _tensor->at(std::span<std::size_t const>{indices, 2});
    }

    [[nodiscard]] std::size_t sizeImpl() const {
        return _tensor->numRows();
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        if (_col_ptr) {
            return {_col_ptr, _tensor->numRows()};
        }
        return {};
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(std::size_t start, std::size_t end) const {
        if (start >= end || end > _tensor->numRows()) {
            return {};
        }
        if (_col_ptr) {
            return {_col_ptr + start, end - start};
        }
        return {};
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return _col_ptr != nullptr;
    }

    [[nodiscard]] AnalogStorageType getStorageTypeImpl() const {
        return AnalogStorageType::Custom;
    }

    [[nodiscard]] AnalogDataStorageCache tryGetCacheImpl() const {
        if (_col_ptr) {
            return AnalogDataStorageCache{
                    .data_ptr = _col_ptr,
                    .cache_size = _tensor->numRows(),
                    .is_contiguous = true};
        }
        return AnalogDataStorageCache{};
    }

    // ========== Accessors ==========

    /**
     * @brief Get the source tensor.
     */
    [[nodiscard]] std::shared_ptr<TensorData const> const & tensor() const noexcept {
        return _tensor;
    }

    /**
     * @brief Get the column index this storage references.
     */
    [[nodiscard]] std::size_t columnIndex() const noexcept {
        return _column_index;
    }

    /**
     * @brief Get raw pointer to contiguous column data (nullptr if not Armadillo-backed).
     */
    [[nodiscard]] float const * data() const noexcept {
        return _col_ptr;
    }

private:
    std::shared_ptr<TensorData const> _tensor;
    std::size_t _column_index;
    float const * _col_ptr = nullptr;

    /**
     * @brief Cache the raw column pointer if the tensor has Armadillo storage.
     *
     * Armadillo uses column-major layout, so colptr(c) gives a contiguous
     * block of numRows() floats for column c.
     */
    void _cacheColumnPointer() {
        auto const * arma_storage = _tensor->storage().tryGetAs<ArmadilloTensorStorage>();
        if (arma_storage && arma_storage->ndim() == 2) {
            _col_ptr = arma_storage->matrix().colptr(_column_index);
        }
    }
};

#endif// TENSOR_COLUMN_ANALOG_STORAGE_HPP
