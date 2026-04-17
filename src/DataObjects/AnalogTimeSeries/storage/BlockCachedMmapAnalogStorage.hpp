/**
 * @file BlockCachedMmapAnalogStorage.hpp
 * @brief Per-channel analog storage delegating to a SharedMmapBlockCache.
 *
 * Phase 2 of the multi-channel storage roadmap. Provides an AnalogTimeSeries
 * storage backend for one channel of a block-cached memory-mapped file.
 * Multiple instances share a single SharedMmapBlockCache via shared_ptr.
 *
 * Within a single cached block, per-channel data is contiguous, enabling
 * span access via getSpanRangeImpl. Cross-block ranges return empty spans
 * and fall back to element-by-element access.
 */

#ifndef BLOCK_CACHED_MMAP_ANALOG_STORAGE_HPP
#define BLOCK_CACHED_MMAP_ANALOG_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"
#include "SharedMmapBlockCache.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>

/**
 * @brief Analog storage backend for one channel of a block-cached mmap file.
 *
 * Holds shared ownership of the SharedMmapBlockCache. Element access goes
 * through the cache's block loading mechanism. Span access is available
 * when the requested range falls within a single cached block.
 *
 * @pre cache != nullptr
 * @pre channel < cache->numChannels()
 */
class BlockCachedMmapAnalogStorage : public AnalogDataStorageBase<BlockCachedMmapAnalogStorage> {
public:
    /**
     * @brief Construct storage for a specific channel.
     *
     * @param cache Shared pointer to the block cache (non-null).
     * @param channel The channel index this storage represents.
     */
    BlockCachedMmapAnalogStorage(
            std::shared_ptr<SharedMmapBlockCache const> cache,
            std::size_t channel)
        : _cache(std::move(cache)),
          _channel(channel) {
        assert(_cache != nullptr && "BlockCachedMmapAnalogStorage: cache must not be null");
        assert(_channel < _cache->numChannels() &&
               "BlockCachedMmapAnalogStorage: channel out of range");
    }

    ~BlockCachedMmapAnalogStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::size_t index) const {
        return _cache->getValue(index, _channel);
    }

    [[nodiscard]] std::size_t sizeImpl() const {
        return _cache->numSamplesPerChannel();
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        // Full span is not available (data split across blocks)
        return {};
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(
            std::size_t start,
            std::size_t end) const {
        if (start >= end || end > _cache->numSamplesPerChannel()) {
            return {};
        }
        std::size_t const count = end - start;
        auto span = _cache->getChannelBlockSpan(_channel, start, count);
        // Only return if the entire range fits in one block
        if (span.size() == count) {
            return span;
        }
        return {};
    }

    [[nodiscard]] bool isContiguousImpl() const {
        // Data is not globally contiguous (split across blocks)
        return false;
    }

    [[nodiscard]] AnalogStorageType getStorageTypeImpl() const {
        return AnalogStorageType::Custom;
    }

    [[nodiscard]] AnalogDataStorageCache tryGetCacheImpl() const {
        // No global contiguous cache available
        return AnalogDataStorageCache{};
    }

    // ========== Accessors ==========

    [[nodiscard]] std::shared_ptr<SharedMmapBlockCache const> const & cache() const noexcept {
        return _cache;
    }

    [[nodiscard]] std::size_t channel() const noexcept {
        return _channel;
    }

private:
    std::shared_ptr<SharedMmapBlockCache const> _cache;
    std::size_t _channel;
};

#endif// BLOCK_CACHED_MMAP_ANALOG_STORAGE_HPP
