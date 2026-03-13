/**
 * @file LibTorchTensorStorage.cpp
 * @brief Implementation of the LibTorch tensor storage backend
 *
 * Wraps a torch::Tensor with the TensorStorageBase CRTP interface.
 * Available only when built with TENSOR_BACKEND_LIBTORCH.
 */

#ifdef TENSOR_BACKEND_LIBTORCH

#include "Tensors/storage/LibTorchTensorStorage.hpp"
#include "Tensors/storage/DenseTensorStorage.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// Construction
// =============================================================================

LibTorchTensorStorage::LibTorchTensorStorage(torch::Tensor tensor)
    : _tensor(std::move(tensor))
{
    if (!_tensor.defined()) {
        throw std::invalid_argument(
            "LibTorchTensorStorage: tensor must be defined");
    }
    if (_tensor.scalar_type() != torch::kFloat32) {
        throw std::invalid_argument(
            "LibTorchTensorStorage: tensor must be float32 (kFloat), got " +
            std::string(torch::toString(_tensor.scalar_type())));
    }
    if (_tensor.dim() == 0) {
        throw std::invalid_argument(
            "LibTorchTensorStorage: scalar tensors (0-dim) are not supported; "
            "use at least 1D");
    }
}

LibTorchTensorStorage LibTorchTensorStorage::fromDense(
    DenseTensorStorage const & dense)
{
    auto const s = dense.shape();
    auto const flat = dense.flatData();

    // Convert shape to int64_t for torch
    std::vector<int64_t> torch_shape;
    torch_shape.reserve(s.size());
    for (auto dim : s) {
        torch_shape.push_back(static_cast<int64_t>(dim));
    }

    // Create tensor from data — clone to own the memory
    torch::Tensor tensor = torch::from_blob(
        const_cast<float *>(flat.data()),
        torch_shape,
        torch::kFloat32
    ).clone();

    return LibTorchTensorStorage{std::move(tensor)};
}

LibTorchTensorStorage LibTorchTensorStorage::fromFlatData(
    std::vector<float> const & data,
    std::vector<std::size_t> const & shape)
{
    // Validate size
    std::size_t total = 1;
    for (auto s : shape) {
        total *= s;
    }
    if (data.size() != total) {
        throw std::invalid_argument(
            "LibTorchTensorStorage::fromFlatData: data size (" +
            std::to_string(data.size()) +
            ") doesn't match shape product (" + std::to_string(total) + ")");
    }

    // Convert shape to int64_t for torch
    std::vector<int64_t> torch_shape;
    torch_shape.reserve(shape.size());
    for (auto dim : shape) {
        torch_shape.push_back(static_cast<int64_t>(dim));
    }

    // Create tensor — clone to own memory
    torch::Tensor tensor = torch::from_blob(
        const_cast<float *>(data.data()),
        torch_shape,
        torch::kFloat32
    ).clone();

    return LibTorchTensorStorage{std::move(tensor)};
}

// =============================================================================
// Direct Torch Access
// =============================================================================

torch::Tensor const & LibTorchTensorStorage::tensor() const noexcept
{
    return _tensor;
}

torch::Tensor & LibTorchTensorStorage::mutableTensor() noexcept
{
    return _tensor;
}

// =============================================================================
// Device Management
// =============================================================================

bool LibTorchTensorStorage::isCuda() const noexcept
{
    return _tensor.is_cuda();
}

bool LibTorchTensorStorage::isCpu() const noexcept
{
    return _tensor.device().is_cpu();
}

void LibTorchTensorStorage::toCpu()
{
    if (!isCpu()) {
        _tensor = _tensor.to(torch::kCPU);
    }
}

void LibTorchTensorStorage::toCuda(int device)
{
    if (!torch::cuda::is_available()) {
        throw std::runtime_error(
            "LibTorchTensorStorage::toCuda: CUDA is not available");
    }
    _tensor = _tensor.to(torch::Device(torch::kCUDA, device));
}

// =============================================================================
// Metadata
// =============================================================================

std::size_t LibTorchTensorStorage::ndim() const noexcept
{
    return static_cast<std::size_t>(_tensor.dim());
}

// =============================================================================
// CRTP Implementation
// =============================================================================

float LibTorchTensorStorage::getValueAtImpl(std::span<std::size_t const> indices) const
{
    if (static_cast<int>(indices.size()) != _tensor.dim()) {
        throw std::invalid_argument(
            "LibTorchTensorStorage::getValueAt: expected " +
            std::to_string(_tensor.dim()) + " indices, got " +
            std::to_string(indices.size()));
    }

    ensureCpuForAccess();

    // Build torch index from the multi-dimensional indices
    // Using direct pointer arithmetic for efficiency
    auto contiguous = _tensor.contiguous();
    float const * ptr = contiguous.data_ptr<float>();

    // Compute flat offset using strides
    std::size_t offset = 0;
    for (int d = 0; d < _tensor.dim(); ++d) {
        auto idx = indices[static_cast<std::size_t>(d)];
        auto dim_size = static_cast<std::size_t>(_tensor.size(d));
        if (idx >= dim_size) {
            throw std::out_of_range(
                "LibTorchTensorStorage::getValueAt: index " +
                std::to_string(idx) + " out of range for axis " +
                std::to_string(d) + " (size " + std::to_string(dim_size) + ")");
        }
        offset += idx * static_cast<std::size_t>(contiguous.stride(d));
    }

    return ptr[offset];
}

std::span<float const> LibTorchTensorStorage::flatDataImpl() const
{
    ensureCpuForAccess();

    // Make contiguous and get pointer
    // Note: if already contiguous, this is a no-op ref-count bump
    auto contiguous = _tensor.contiguous();
    return {contiguous.data_ptr<float>(),
            static_cast<std::size_t>(contiguous.numel())};
}

std::vector<float> LibTorchTensorStorage::sliceAlongAxisImpl(
    std::size_t axis, std::size_t index) const
{
    if (static_cast<int>(axis) >= _tensor.dim()) {
        throw std::out_of_range(
            "LibTorchTensorStorage::sliceAlongAxis: axis " +
            std::to_string(axis) + " out of range (ndim = " +
            std::to_string(_tensor.dim()) + ")");
    }
    if (index >= static_cast<std::size_t>(_tensor.size(static_cast<int>(axis)))) {
        throw std::out_of_range(
            "LibTorchTensorStorage::sliceAlongAxis: index " +
            std::to_string(index) + " out of range for axis " +
            std::to_string(axis) + " (size " +
            std::to_string(_tensor.size(static_cast<int>(axis))) + ")");
    }

    ensureCpuForAccess();

    // Select along the axis: reduces dimensionality by 1
    auto sliced = _tensor.select(static_cast<int64_t>(axis),
                                  static_cast<int64_t>(index));
    auto contiguous = sliced.contiguous();

    float const * ptr = contiguous.data_ptr<float>();
    auto numel = static_cast<std::size_t>(contiguous.numel());

    return {ptr, ptr + numel};
}

std::vector<float> LibTorchTensorStorage::getColumnImpl(std::size_t col) const
{
    // "Column" = slice along the last axis, returning one value per row
    // For a 2D [R, C] tensor: returns R values (one per row at column col)
    // For ND: returns all elements with last-axis index == col

    auto const nd = _tensor.dim();
    if (nd < 2) {
        throw std::logic_error(
            "LibTorchTensorStorage::getColumn: requires at least 2D tensor, got " +
            std::to_string(nd) + "D");
    }

    auto const last_axis = nd - 1;
    auto const last_size = static_cast<std::size_t>(_tensor.size(last_axis));
    if (col >= last_size) {
        throw std::out_of_range(
            "LibTorchTensorStorage::getColumn: column " +
            std::to_string(col) + " out of range (size " +
            std::to_string(last_size) + ")");
    }

    ensureCpuForAccess();

    // Use torch::Tensor::select on the last axis
    auto selected = _tensor.select(last_axis, static_cast<int64_t>(col));
    auto contiguous = selected.contiguous();

    float const * ptr = contiguous.data_ptr<float>();
    auto numel = static_cast<std::size_t>(contiguous.numel());

    return {ptr, ptr + numel};
}

std::vector<std::size_t> LibTorchTensorStorage::shapeImpl() const
{
    std::vector<std::size_t> result;
    result.reserve(static_cast<std::size_t>(_tensor.dim()));
    for (int d = 0; d < _tensor.dim(); ++d) {
        result.push_back(static_cast<std::size_t>(_tensor.size(d)));
    }
    return result;
}

std::size_t LibTorchTensorStorage::totalElementsImpl() const
{
    return static_cast<std::size_t>(_tensor.numel());
}

bool LibTorchTensorStorage::isContiguousImpl() const noexcept
{
    return _tensor.is_contiguous();
}

TensorStorageCache LibTorchTensorStorage::tryGetCacheImpl() const
{
    // Cache is only valid for CPU contiguous tensors
    if (!isCpu() || !_tensor.is_contiguous()) {
        return TensorStorageCache{
            .data_ptr = nullptr,
            .total_elements = static_cast<std::size_t>(_tensor.numel()),
            .shape = shapeImpl(),
            .strides = {},
            .is_valid = false
        };
    }

    // Build strides in element count (not bytes) — match row-major convention
    std::vector<std::size_t> strides;
    strides.reserve(static_cast<std::size_t>(_tensor.dim()));
    for (int d = 0; d < _tensor.dim(); ++d) {
        strides.push_back(static_cast<std::size_t>(_tensor.stride(d)));
    }

    return TensorStorageCache{
        .data_ptr = _tensor.data_ptr<float>(),
        .total_elements = static_cast<std::size_t>(_tensor.numel()),
        .shape = shapeImpl(),
        .strides = std::move(strides),
        .is_valid = true
    };
}

// =============================================================================
// Private Helpers
// =============================================================================

void LibTorchTensorStorage::ensureCpuForAccess() const
{
    if (!isCpu()) {
        throw std::runtime_error(
            "LibTorchTensorStorage: tensor is on CUDA device; "
            "call toCpu() before accessing data via the CRTP interface");
    }
}

#endif // TENSOR_BACKEND_LIBTORCH
