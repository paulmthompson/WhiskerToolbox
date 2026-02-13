#ifndef DENSE_TENSOR_STORAGE_HPP
#define DENSE_TENSOR_STORAGE_HPP

#include "TensorStorageBase.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief Tensor storage backend using a flat std::vector<float> in row-major order
 *
 * This is the **fallback storage backend** for tensors with >3 dimensions,
 * where Armadillo's arma::fcube maximum of 3D does not suffice. It has no
 * external dependencies.
 *
 * Primary use cases:
 * - >3D model I/O with LibTorch (batch × channel × height × width)
 * - Materialization target when converting between backends of different
 *   dimensionality
 * - Storage for `TensorData::createND()` when ndim > 3
 *
 * For ≤3D data, prefer `ArmadilloTensorStorage` since it provides zero-copy
 * mlpack interop.
 *
 * ## Layout
 *
 * Data is stored in **row-major** (C-contiguous) order. For a tensor with
 * shape [d0, d1, d2, d3], the element at index (i0, i1, i2, i3) is at flat
 * offset: i0 * stride[0] + i1 * stride[1] + i2 * stride[2] + i3 * stride[3]
 *
 * Strides are precomputed at construction time:
 *   stride[k] = product of shape[k+1..N-1]
 *   stride[N-1] = 1
 *
 * This matches LibTorch's default C-contiguous layout, making conversion
 * trivial (just copy the flat buffer).
 */
class DenseTensorStorage : public TensorStorageBase<DenseTensorStorage> {
public:
    // ========== Construction ==========

    /**
     * @brief Construct from flat data and shape
     *
     * @param data Flat float data in row-major order
     * @param shape Dimensions of the tensor (e.g., {2, 3, 4, 5} for a 4D tensor)
     * @throws std::invalid_argument if data.size() != product of shape
     * @throws std::invalid_argument if shape is empty
     */
    DenseTensorStorage(std::vector<float> data, std::vector<std::size_t> shape);

    /**
     * @brief Construct an uninitialized tensor with the given shape
     *
     * All elements are initialized to 0.0f. Useful for pre-allocating a tensor
     * that will be filled via mutableFlatData().
     *
     * @param shape Dimensions of the tensor
     * @throws std::invalid_argument if shape is empty
     */
    explicit DenseTensorStorage(std::vector<std::size_t> shape);

    // ========== Mutable Access ==========

    /**
     * @brief Get mutable access to the flat data buffer
     *
     * Intended for construction and bulk-fill scenarios. Callers must not
     * change the size of the underlying data.
     */
    [[nodiscard]] std::span<float> mutableFlatData();

    /**
     * @brief Set a single value by multi-dimensional index
     *
     * @param indices One index per dimension, in row-major order
     * @param value The value to set
     * @throws std::out_of_range if any index is out of bounds
     * @throws std::invalid_argument if indices.size() != ndim
     */
    void setValueAt(std::span<std::size_t const> indices, float value);

    // ========== Metadata ==========

    /**
     * @brief Get the number of dimensions
     */
    [[nodiscard]] std::size_t ndim() const noexcept { return _shape.size(); }

    /**
     * @brief Get the precomputed row-major strides
     *
     * stride[k] = product of shape[k+1..N-1], stride[N-1] = 1
     */
    [[nodiscard]] std::vector<std::size_t> const & strides() const noexcept { return _strides; }

    // ========== CRTP Implementation ==========

    [[nodiscard]] float getValueAtImpl(std::span<std::size_t const> indices) const;
    [[nodiscard]] std::span<float const> flatDataImpl() const;
    [[nodiscard]] std::vector<float> sliceAlongAxisImpl(std::size_t axis, std::size_t index) const;
    [[nodiscard]] std::vector<float> getColumnImpl(std::size_t col) const;
    [[nodiscard]] std::vector<std::size_t> shapeImpl() const;
    [[nodiscard]] std::size_t totalElementsImpl() const;
    [[nodiscard]] bool isContiguousImpl() const noexcept { return true; }
    [[nodiscard]] TensorStorageType getStorageTypeImpl() const noexcept {
        return TensorStorageType::Dense;
    }
    [[nodiscard]] TensorStorageCache tryGetCacheImpl() const;

private:
    std::vector<float> _data;
    std::vector<std::size_t> _shape;
    std::vector<std::size_t> _strides; ///< Precomputed row-major strides

    /**
     * @brief Compute the flat offset from multi-dimensional indices
     *
     * Does NOT perform bounds checking — caller must validate first.
     */
    [[nodiscard]] std::size_t flatOffset(std::span<std::size_t const> indices) const noexcept;

    /**
     * @brief Compute row-major strides from shape
     */
    static std::vector<std::size_t> computeStrides(std::vector<std::size_t> const & shape);

    /**
     * @brief Validate that indices are in bounds
     * @throws std::out_of_range if any index is out of bounds
     * @throws std::invalid_argument if indices.size() != ndim
     */
    void validateIndices(std::span<std::size_t const> indices) const;
};

#endif // DENSE_TENSOR_STORAGE_HPP
