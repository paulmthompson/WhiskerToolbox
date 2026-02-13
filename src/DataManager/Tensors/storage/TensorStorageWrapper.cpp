/**
 * @file TensorStorageWrapper.cpp
 * @brief Implementation of the Sean Parent type-erasure wrapper for tensor storage
 *
 * Non-template methods of TensorStorageWrapper are defined here.
 * Template methods (constructor, tryGetAs, tryGetMutableAs) are in the header.
 */

#include "Tensors/storage/TensorStorageWrapper.hpp"

#include <stdexcept>

// =============================================================================
// Validity
// =============================================================================

bool TensorStorageWrapper::isValid() const noexcept
{
    return _impl != nullptr;
}

// =============================================================================
// Internal Validation
// =============================================================================

void TensorStorageWrapper::ensureValid() const
{
    if (!_impl) {
        throw std::runtime_error(
            "TensorStorageWrapper: operation on null wrapper "
            "(default-constructed or moved-from)");
    }
}

// =============================================================================
// Delegated Interface
// =============================================================================

float TensorStorageWrapper::getValueAt(std::span<std::size_t const> indices) const
{
    ensureValid();
    return _impl->getValueAt(indices);
}

std::span<float const> TensorStorageWrapper::flatData() const
{
    ensureValid();
    return _impl->flatData();
}

std::vector<float> TensorStorageWrapper::sliceAlongAxis(
    std::size_t axis, std::size_t index) const
{
    ensureValid();
    return _impl->sliceAlongAxis(axis, index);
}

std::vector<float> TensorStorageWrapper::getColumn(std::size_t col) const
{
    ensureValid();
    return _impl->getColumn(col);
}

std::vector<std::size_t> TensorStorageWrapper::shape() const
{
    ensureValid();
    return _impl->shape();
}

std::size_t TensorStorageWrapper::totalElements() const
{
    ensureValid();
    return _impl->totalElements();
}

bool TensorStorageWrapper::isContiguous() const
{
    ensureValid();
    return _impl->isContiguous();
}

TensorStorageType TensorStorageWrapper::getStorageType() const
{
    ensureValid();
    return _impl->getStorageType();
}

TensorStorageCache TensorStorageWrapper::tryGetCache() const
{
    ensureValid();
    return _impl->tryGetCache();
}

// =============================================================================
// Shared Ownership Access
// =============================================================================

std::shared_ptr<void const> TensorStorageWrapper::sharedStorage() const
{
    return _impl;
}
