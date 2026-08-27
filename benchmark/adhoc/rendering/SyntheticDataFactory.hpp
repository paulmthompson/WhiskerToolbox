/**
 * @file SyntheticDataFactory.hpp
 * @brief Factory helpers to create AnalogTimeSeries with each storage backend
 *        for rendering pipeline benchmarks.
 *
 * Creates sine-wave data with controllable sample count and channel count.
 * For mmap backends, writes temporary int16 interleaved binary files to /tmp.
 */
#ifndef BENCHMARK_ADHOC_RENDERING_SYNTHETICDATAFACTORY_HPP
#define BENCHMARK_ADHOC_RENDERING_SYNTHETICDATAFACTORY_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/storage/BlockCachedMmapAnalogStorage.hpp"
#include "AnalogTimeSeries/storage/MemoryMappedAnalogDataStorage.hpp"
#include "AnalogTimeSeries/storage/SharedMmapBlockCache.hpp"
#include "AnalogTimeSeries/storage/TensorColumnAnalogStorage.hpp"
#include "AnalogTimeSeries/storage/VectorAnalogDataStorage.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <armadillo>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace BenchmarkSynthetic {

// ============================================================================
// TimeFrame helper
// ============================================================================

inline std::shared_ptr<TimeFrame> makeTimeFrame(std::size_t num_samples) {
    std::vector<int> times(static_cast<int>(num_samples));
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

// ============================================================================
// Synthetic waveform generation
// ============================================================================

/// Generate a sine wave with some harmonics so the signal isn't trivially flat.
inline std::vector<float> generateSineWave(std::size_t num_samples, float frequency = 10.0f,
                                           float sample_rate = 30000.0f) {
    std::vector<float> data(num_samples);
    float const dt = 1.0f / sample_rate;
    for (std::size_t i = 0; i < num_samples; ++i) {
        float const t = static_cast<float>(i) * dt;
        data[i] = std::sin(2.0f * 3.14159265f * frequency * t) + 0.3f * std::sin(2.0f * 3.14159265f * frequency * 3.0f * t) + 0.1f * std::sin(2.0f * 3.14159265f * frequency * 7.0f * t);
    }
    return data;
}

// ============================================================================
// 1. Vector-backed AnalogTimeSeries
// ============================================================================

struct VectorSeriesResult {
    std::shared_ptr<AnalogTimeSeries> series;
    std::shared_ptr<TimeFrame> time_frame;
};

inline VectorSeriesResult createVectorSeries(std::size_t num_samples) {
    auto tf = makeTimeFrame(num_samples);
    auto data = generateSineWave(num_samples);
    auto series = std::make_shared<AnalogTimeSeries>(std::move(data), num_samples);
    series->setTimeFrame(tf);
    return {series, tf};
}

// ============================================================================
// 2. TensorColumn-backed AnalogTimeSeries (Armadillo, contiguous columns)
// ============================================================================

struct TensorColumnResult {
    std::shared_ptr<TensorData> tensor;
    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    std::shared_ptr<TimeFrame> time_frame;
};

inline TensorColumnResult createTensorColumnSeries(std::size_t num_channels,
                                                   std::size_t num_samples) {
    auto tf = makeTimeFrame(num_samples);
    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(num_samples);

    // Build an Armadillo matrix (column-major): each column is a channel
    arma::fmat matrix(num_samples, num_channels);
    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        float const freq = 10.0f + static_cast<float>(ch) * 2.0f;
        auto wave = generateSineWave(num_samples, freq);
        std::copy(wave.begin(), wave.end(), matrix.colptr(ch));
    }

    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(std::move(matrix), time_storage, tf));

    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    channels.reserve(num_channels);
    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        auto col_storage = TensorColumnAnalogStorage(tensor, ch);
        AnalogDataStorageWrapper wrapper(std::move(col_storage));
        auto ch_time_storage = TimeIndexStorageFactory::createDenseFromZero(num_samples);
        auto analog = AnalogTimeSeries::createFromStorage(std::move(wrapper), ch_time_storage);
        analog->setTimeFrame(tf);
        channels.push_back(std::move(analog));
    }

    return {tensor, std::move(channels), tf};
}

// ============================================================================
// 3. Memory-mapped per-channel (MemoryMappedAnalogDataStorage)
// ============================================================================

/// Write an interleaved int16 binary file: ch0t0, ch1t0, ch2t0, ..., ch0t1, ch1t1, ...
inline std::filesystem::path writeInterleavedInt16File(
        std::size_t num_channels, std::size_t num_samples,
        std::string const & suffix = "") {
    auto path = std::filesystem::temp_directory_path() / ("bm_interleaved_" + suffix + ".bin");
    std::ofstream ofs(path, std::ios::binary);
    assert(ofs.is_open());

    for (std::size_t t = 0; t < num_samples; ++t) {
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            float const freq = 10.0f + static_cast<float>(ch) * 2.0f;
            float const dt = 1.0f / 30000.0f;
            float const val = std::sin(2.0f * 3.14159265f * freq * static_cast<float>(t) * dt);
            auto const sample = static_cast<int16_t>(val * 1000.0f);// scale to int16 range
            ofs.write(reinterpret_cast<char const *>(&sample), sizeof(sample));
        }
    }
    ofs.close();
    return path;
}

struct MmapPerChannelResult {
    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    std::shared_ptr<TimeFrame> time_frame;
    std::filesystem::path file_path;// caller should clean up
};

inline MmapPerChannelResult createMemoryMappedSeries(std::size_t num_channels,
                                                     std::size_t num_samples) {
    auto file_path = writeInterleavedInt16File(num_channels, num_samples, "mmap");
    auto tf = makeTimeFrame(num_samples);

    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    channels.reserve(num_channels);

    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        MmapStorageConfig config;
        config.file_path = file_path;
        config.header_size = 0;
        config.offset = ch;          // channel offset in samples
        config.stride = num_channels;// interleaved
        config.num_samples = num_samples;
        config.data_type = MmapDataType::Int16;
        config.scale_factor = 0.001f;// int16 → float scaling
        config.offset_value = 0.0f;

        std::vector<TimeFrameIndex> time_vec;
        time_vec.reserve(num_samples);
        for (std::size_t i = 0; i < num_samples; ++i) {
            time_vec.emplace_back(static_cast<int64_t>(i));
        }

        auto analog = AnalogTimeSeries::createMemoryMapped(std::move(config), std::move(time_vec));
        analog->setTimeFrame(tf);
        channels.push_back(std::move(analog));
    }

    return {std::move(channels), tf, file_path};
}

// ============================================================================
// 4. BlockCachedMmap-backed AnalogTimeSeries (shared block cache)
// ============================================================================

struct BlockCachedResult {
    std::shared_ptr<SharedMmapBlockCache> cache;
    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    std::shared_ptr<TimeFrame> time_frame;
    std::filesystem::path file_path;// caller should clean up
};

inline BlockCachedResult createBlockCachedMmapSeries(
        std::size_t num_channels, std::size_t num_samples,
        std::size_t block_size = 4096, std::size_t max_cached_blocks = 16) {
    auto file_path = writeInterleavedInt16File(num_channels, num_samples, "blockcache");
    auto tf = makeTimeFrame(num_samples);

    SharedMmapBlockCacheConfig cache_config;
    cache_config.file_path = file_path;
    cache_config.header_size = 0;
    cache_config.num_channels = num_channels;
    cache_config.data_type = MmapDataType::Int16;
    cache_config.scale_factor = 0.001f;
    cache_config.offset_value = 0.0f;
    cache_config.block_size_samples = block_size;
    cache_config.max_cached_blocks = max_cached_blocks;

    auto cache = std::make_shared<SharedMmapBlockCache>(std::move(cache_config));

    std::vector<std::shared_ptr<AnalogTimeSeries>> channels;
    channels.reserve(num_channels);

    for (std::size_t ch = 0; ch < num_channels; ++ch) {
        auto block_storage = BlockCachedMmapAnalogStorage(cache, ch);
        AnalogDataStorageWrapper wrapper(std::move(block_storage));
        auto time_storage = TimeIndexStorageFactory::createDenseFromZero(num_samples);
        auto analog = AnalogTimeSeries::createFromStorage(std::move(wrapper), time_storage);
        analog->setTimeFrame(tf);
        channels.push_back(std::move(analog));
    }

    return {cache, std::move(channels), tf, file_path};
}

// ============================================================================
// Cleanup helper
// ============================================================================

inline void cleanupTempFile(std::filesystem::path const & path) {
    if (!path.empty() && std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

}// namespace BenchmarkSynthetic

#endif// BENCHMARK_ADHOC_RENDERING_SYNTHETICDATAFACTORY_HPP
