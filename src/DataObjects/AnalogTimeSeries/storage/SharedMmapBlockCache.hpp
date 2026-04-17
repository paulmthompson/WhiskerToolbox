/**
 * @file SharedMmapBlockCache.hpp
 * @brief Shared block-cached memory-mapped file for multi-channel binary data.
 *
 * Phase 2 of the multi-channel storage roadmap. Provides a single mmap of an
 * interleaved binary file with a circular buffer of decoded float32 blocks.
 * When one channel loads a time window, all channels' data for that window is
 * decoded into the cache. Subsequent channel accesses hit the cache.
 *
 * Block layout in cache is column-major: all samples for channel 0, then all
 * samples for channel 1, etc. This enables contiguous per-channel span access
 * within a single block.
 */

#ifndef SHARED_MMAP_BLOCK_CACHE_HPP
#define SHARED_MMAP_BLOCK_CACHE_HPP

#include "MmapAnalogConfig.hpp"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

/**
 * @brief Configuration for SharedMmapBlockCache.
 */
struct SharedMmapBlockCacheConfig {
    std::filesystem::path file_path;             ///< Path to interleaved binary file
    std::size_t header_size = 0;                 ///< Bytes to skip at file start
    std::size_t num_channels = 1;                ///< Number of interleaved channels
    MmapDataType data_type = MmapDataType::Int16;///< Element data type in file
    float scale_factor = 1.0f;                   ///< Multiplicative scale applied on decode
    float offset_value = 0.0f;                   ///< Additive offset applied on decode
    std::size_t block_size_samples = 4096;       ///< Samples per channel per block
    std::size_t max_cached_blocks = 16;          ///< Maximum blocks held in cache
};

/**
 * @brief Shared block-cached memory-mapped storage for interleaved multi-channel data.
 *
 * Opens a single mmap for the entire file. Maintains a FIFO circular buffer of
 * decoded float blocks. Each block covers `block_size_samples` time steps for
 * all channels. Data is decoded (type-converted, scaled, offset) at block load
 * time.
 *
 * Intended to be shared (via `std::shared_ptr`) between a `MmapTensorStorage`
 * and multiple `BlockCachedMmapAnalogStorage` instances.
 *
 * @note Spans returned by `getChannelBlockSpan()` are invalidated when the
 *       underlying block is evicted. Callers must not hold spans across
 *       operations that may trigger cache eviction.
 *
 * @pre config.num_channels >= 1
 * @pre config.block_size_samples >= 1
 * @pre config.max_cached_blocks >= 1
 */
class SharedMmapBlockCache {
public:
    explicit SharedMmapBlockCache(SharedMmapBlockCacheConfig config);
    ~SharedMmapBlockCache();

    // Non-copyable, non-movable (shared via shared_ptr)
    SharedMmapBlockCache(SharedMmapBlockCache const &) = delete;
    SharedMmapBlockCache & operator=(SharedMmapBlockCache const &) = delete;
    SharedMmapBlockCache(SharedMmapBlockCache &&) = delete;
    SharedMmapBlockCache & operator=(SharedMmapBlockCache &&) = delete;

    /**
     * @brief Get a single decoded float value.
     *
     * @param time_sample Time sample index (0-based, per channel).
     * @param channel Channel index.
     * @return Decoded float value with scale and offset applied.
     *
     * @pre time_sample < numSamplesPerChannel()
     * @pre channel < numChannels()
     */
    [[nodiscard]] float getValue(std::size_t time_sample, std::size_t channel) const;

    /**
     * @brief Get a contiguous span of decoded floats for one channel within a block.
     *
     * Returns up to `max_count` samples starting at `start_sample`. The span
     * is limited to the current block boundary. The returned span is valid
     * until the block is evicted by subsequent cache operations.
     *
     * @param channel Channel index.
     * @param start_sample Starting time sample index.
     * @param max_count Maximum number of samples to return.
     * @return Span of decoded floats. May be shorter than max_count at block/file boundary.
     *
     * @pre channel < numChannels()
     * @pre start_sample < numSamplesPerChannel()
     */
    [[nodiscard]] std::span<float const> getChannelBlockSpan(
            std::size_t channel,
            std::size_t start_sample,
            std::size_t max_count) const;

    [[nodiscard]] std::size_t numSamplesPerChannel() const noexcept { return _num_samples_per_channel; }
    [[nodiscard]] std::size_t numChannels() const noexcept { return _config.num_channels; }
    [[nodiscard]] std::size_t blockSizeSamples() const noexcept { return _config.block_size_samples; }
    [[nodiscard]] SharedMmapBlockCacheConfig const & config() const noexcept { return _config; }

private:
    struct CachedBlock {
        std::size_t block_index = std::numeric_limits<std::size_t>::max();
        std::vector<float> data;///< Column-major: [ch0 samples..., ch1 samples..., ...]
    };

    [[nodiscard]] CachedBlock const & _getOrLoadBlock(std::size_t block_index) const;
    void _loadBlock(CachedBlock & block, std::size_t block_index) const;
    [[nodiscard]] float _convertToFloat(void const * ptr) const;
    [[nodiscard]] std::size_t _getElementSize() const;
    [[nodiscard]] std::size_t _actualBlockSize(std::size_t block_index) const;
    void _openAndMapFile();
    void _closeAndUnmap();

    SharedMmapBlockCacheConfig _config;
    std::size_t _num_samples_per_channel = 0;
    std::size_t _element_size = 0;
    std::size_t _total_blocks = 0;

    /// @brief Opaque platform-specific mmap state (defined in .cpp)
    struct PlatformMmapState;
    std::unique_ptr<PlatformMmapState> _platform;

    // Block cache (mutable for const access pattern)
    mutable std::vector<CachedBlock> _cache;
    mutable std::size_t _next_evict = 0;
    mutable std::mutex _mutex;
};

#endif// SHARED_MMAP_BLOCK_CACHE_HPP
