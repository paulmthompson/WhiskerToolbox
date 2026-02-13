#ifndef LIBTORCH_TENSOR_STORAGE_HPP
#define LIBTORCH_TENSOR_STORAGE_HPP

#ifdef TENSOR_BACKEND_LIBTORCH

#include "TensorStorageBase.hpp"

// torch's c10 logging defines a CHECK macro that conflicts with testing
// frameworks (Catch2) and other libraries. Save/restore around the include.
#pragma push_macro("CHECK")
#include <torch/torch.h>
#pragma pop_macro("CHECK")

#include <cstddef>
#include <span>
#include <vector>

class DenseTensorStorage;

/**
 * @brief Tensor storage backend wrapping a torch::Tensor
 *
 * Provides zero-copy access to LibTorch tensors for model inference.
 * Available only when built with `TENSOR_BACKEND_LIBTORCH` defined.
 *
 * ## Design
 *
 * The owned torch::Tensor is kept in its native format. The CRTP interface
 * presents row-major semantics to consumers. Since LibTorch default layout
 * is C-contiguous (row-major), this is usually transparent.
 *
 * ## Element Type
 *
 * The tensor must be float32 (kFloat). The constructor validates this and
 * throws if the tensor has a different dtype.
 *
 * ## Device
 *
 * flatData() and getValueAt() require the tensor to be on CPU. If the tensor
 * is on CUDA, call toCpu() first or use asTorchTensor() for direct GPU access.
 *
 * ## Ownership
 *
 * The storage owns the torch::Tensor (via value semantics — torch::Tensor is
 * itself a reference-counted handle to underlying TensorImpl). Copies of this
 * storage share the underlying tensor data (torch's COW semantics).
 *
 * ## Thread Safety
 *
 * Same as torch::Tensor. No additional synchronization is provided.
 */
class LibTorchTensorStorage : public TensorStorageBase<LibTorchTensorStorage> {
public:
    // ========== Construction ==========

    /**
     * @brief Construct from an existing torch::Tensor
     *
     * @param tensor A defined float32 tensor
     * @throws std::invalid_argument if tensor is not defined
     * @throws std::invalid_argument if tensor dtype is not kFloat
     * @throws std::invalid_argument if tensor has zero dimensions
     */
    explicit LibTorchTensorStorage(torch::Tensor tensor);

    /**
     * @brief Create from a DenseTensorStorage (copies data into a torch::Tensor)
     *
     * The DenseTensorStorage's flat row-major data is copied into a new
     * CPU-resident float32 torch::Tensor with the matching shape.
     *
     * @param dense The source DenseTensorStorage
     * @return A new LibTorchTensorStorage wrapping the converted tensor
     */
    [[nodiscard]] static LibTorchTensorStorage fromDense(
        DenseTensorStorage const & dense);

    /**
     * @brief Create from flat row-major data and shape
     *
     * @param data Flat row-major float data
     * @param shape Tensor dimensions
     * @throws std::invalid_argument if data size doesn't match product of shape
     */
    [[nodiscard]] static LibTorchTensorStorage fromFlatData(
        std::vector<float> const & data,
        std::vector<std::size_t> const & shape);

    // ========== Direct Torch Access (zero-copy for model I/O) ==========

    /**
     * @brief Get const reference to the underlying torch::Tensor
     */
    [[nodiscard]] torch::Tensor const & tensor() const noexcept;

    /**
     * @brief Get mutable reference to the underlying torch::Tensor
     *
     * Use with care — mutation bypasses observer notifications.
     */
    [[nodiscard]] torch::Tensor & mutableTensor() noexcept;

    // ========== Device Management ==========

    /**
     * @brief Check whether the tensor resides on a CUDA device
     */
    [[nodiscard]] bool isCuda() const noexcept;

    /**
     * @brief Check whether the tensor resides on CPU
     */
    [[nodiscard]] bool isCpu() const noexcept;

    /**
     * @brief Move the tensor to CPU (no-op if already on CPU)
     */
    void toCpu();

    /**
     * @brief Move the tensor to a CUDA device
     *
     * @param device CUDA device index (default 0)
     * @throws std::runtime_error if CUDA is not available
     */
    void toCuda(int device = 0);

    // ========== Metadata ==========

    /**
     * @brief Number of dimensions
     */
    [[nodiscard]] std::size_t ndim() const noexcept;

    // ========== CRTP Implementation ==========

    [[nodiscard]] float getValueAtImpl(std::span<std::size_t const> indices) const;
    [[nodiscard]] std::span<float const> flatDataImpl() const;
    [[nodiscard]] std::vector<float> sliceAlongAxisImpl(std::size_t axis, std::size_t index) const;
    [[nodiscard]] std::vector<float> getColumnImpl(std::size_t col) const;
    [[nodiscard]] std::vector<std::size_t> shapeImpl() const;
    [[nodiscard]] std::size_t totalElementsImpl() const;
    [[nodiscard]] bool isContiguousImpl() const noexcept;
    [[nodiscard]] TensorStorageType getStorageTypeImpl() const noexcept {
        return TensorStorageType::LibTorch;
    }
    [[nodiscard]] TensorStorageCache tryGetCacheImpl() const;

private:
    torch::Tensor _tensor;

    /**
     * @brief Ensure the tensor is on CPU and contiguous, preparing for data access
     *
     * @throws std::runtime_error if tensor is on CUDA (caller must toCpu() first)
     */
    void ensureCpuForAccess() const;
};

#endif // TENSOR_BACKEND_LIBTORCH

#endif // LIBTORCH_TENSOR_STORAGE_HPP
