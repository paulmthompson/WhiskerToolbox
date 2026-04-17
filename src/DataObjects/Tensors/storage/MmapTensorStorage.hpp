/**
 * @file MmapTensorStorage.hpp
 * @brief TensorData storage backend backed by a SharedMmapBlockCache.
 *
 * Phase 2 of the multi-channel storage roadmap. Provides a TensorData view
 * of shape [num_samples, num_channels] over a block-cached memory-mapped
 * interleaved binary file.
 *
 * This is a header-only file. It is NOT compiled as part of the TensorData
 * library (to avoid a circular dependency with AnalogTimeSeries). Instead,
 * it is compiled by the consumer that creates the storage (e.g., DataManagerIO).
 */

#ifndef MMAP_TENSOR_STORAGE_HPP
#define MMAP_TENSOR_STORAGE_HPP

#include "TensorStorageBase.hpp"

#include "AnalogTimeSeries/storage/SharedMmapBlockCache.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

/**
 * @brief TensorData storage backed by a SharedMmapBlockCache.
 *
 * Presents a 2D tensor of shape [num_samples_per_channel, num_channels] over
 * a block-cached memory-mapped interleaved binary file. Data is not contiguous
 * in memory (split across cache blocks), so flatData() throws.
 *
 * @pre cache != nullptr
 */
class MmapTensorStorage : public TensorStorageBase<MmapTensorStorage> {
public:
    explicit MmapTensorStorage(std::shared_ptr<SharedMmapBlockCache const> cache)
        : _cache(std::move(cache)) {
        assert(_cache != nullptr && "MmapTensorStorage: cache must not be null");
    }

    ~MmapTensorStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::span<std::size_t const> indices) const {
        if (indices.size() != 2) {
            throw std::invalid_argument("MmapTensorStorage: expected 2 indices (row, col)");
        }
        return _cache->getValue(indices[0], indices[1]);
    }

    [[nodiscard]] std::span<float const> flatDataImpl() const {
        throw std::runtime_error(
                "MmapTensorStorage: flatData() not available (non-contiguous block-cached storage)");
    }

    [[nodiscard]] std::vector<float> sliceAlongAxisImpl(
            std::size_t axis,
            std::size_t index) const {
        if (axis == 0) {
            // Row slice: all channels at time `index`
            if (index >= _cache->numSamplesPerChannel()) {
                throw std::out_of_range("MmapTensorStorage: row index out of range");
            }
            std::vector<float> result(_cache->numChannels());
            for (std::size_t ch = 0; ch < _cache->numChannels(); ++ch) {
                result[ch] = _cache->getValue(index, ch);
            }
            return result;
        }
        if (axis == 1) {
            // Column slice: all samples for channel `index`
            return getColumnImpl(index);
        }
        throw std::out_of_range("MmapTensorStorage: axis must be 0 or 1 for 2D tensor");
    }

    [[nodiscard]] std::vector<float> getColumnImpl(std::size_t col) const {
        if (col >= _cache->numChannels()) {
            throw std::out_of_range("MmapTensorStorage: column index out of range");
        }
        std::vector<float> result;
        result.reserve(_cache->numSamplesPerChannel());

        std::size_t pos = 0;
        while (pos < _cache->numSamplesPerChannel()) {
            auto span = _cache->getChannelBlockSpan(
                    col, pos, _cache->numSamplesPerChannel() - pos);
            result.insert(result.end(), span.begin(), span.end());
            pos += span.size();
        }
        return result;
    }

    [[nodiscard]] std::vector<std::size_t> shapeImpl() const {
        return {_cache->numSamplesPerChannel(), _cache->numChannels()};
    }

    [[nodiscard]] std::size_t totalElementsImpl() const {
        return _cache->numSamplesPerChannel() * _cache->numChannels();
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return false;
    }

    [[nodiscard]] TensorStorageType getStorageTypeImpl() const {
        return TensorStorageType::MemoryMapped;
    }

    [[nodiscard]] TensorStorageCache tryGetCacheImpl() const {
        return TensorStorageCache{};// Not contiguous
    }

    // ========== Accessors ==========

    [[nodiscard]] std::shared_ptr<SharedMmapBlockCache const> const & cache() const noexcept {
        return _cache;
    }

private:
    std::shared_ptr<SharedMmapBlockCache const> _cache;
};

#endif// MMAP_TENSOR_STORAGE_HPP
