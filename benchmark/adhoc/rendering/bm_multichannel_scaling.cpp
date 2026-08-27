/**
 * @file bm_multichannel_scaling.cpp
 * @brief Benchmark: Full multi-channel rendering simulation.
 *
 * The core question: how does total rendering time scale with channel count?
 * Tests realistic scenarios for electrophysiology:
 *   - 1, 16, 32, 64, 128, 256, 384 channels
 *   - Different window sizes (30K at 1 kHz = 30s, 300K = 5min, 900K = 15min)
 *   - With/without min/max decimation
 *   - Different storage backends
 *
 * Run:
 *   ./out/build/Clang/Release/bin/adhoc/bm_multichannel_scaling
 */

#include "MinMaxDecimator.hpp"
#include "SyntheticDataFactory.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace BenchmarkSynthetic;
using namespace MinMaxDecimator;

// ============================================================================
// C1.1: Multi-channel raw vertices — TensorColumn (current-like path)
//       No decimation: 1 vertex per sample per channel
// ============================================================================
static void BM_MultiCh_Raw_TensorColumn(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createTensorColumnSeries(num_channels, num_samples);

    // Get spans for all channels
    std::vector<std::span<float const>> spans;
    spans.reserve(num_channels);
    for (auto const & ch: result.channels) {
        spans.push_back(ch->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1))));
    }

    for (auto _: state) {
        std::vector<float> all_vertices;
        all_vertices.reserve(num_samples * num_channels * 2);

        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            for (std::size_t i = 0; i < spans[ch].size(); ++i) {
                all_vertices.push_back(static_cast<float>(i));
                all_vertices.push_back(spans[ch][i] + static_cast<float>(ch));// offset per channel
            }
        }

        benchmark::DoNotOptimize(all_vertices.data());
        benchmark::ClobberMemory();
    }

    auto const total_samples = static_cast<int64_t>(num_samples * num_channels);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * total_samples);
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * total_samples * 2 * sizeof(float));
    state.counters["samples_per_ch"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
    state.counters["total_vertices"] = static_cast<double>(num_samples * num_channels);
}

// ============================================================================
// C1.2: Multi-channel decimated — TensorColumn + per-pixel buffer
// ============================================================================
static void BM_MultiCh_Decimated_TensorColumn(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto const canvas_width = static_cast<std::size_t>(state.range(2));
    auto result = createTensorColumnSeries(num_channels, num_samples);

    // Get spans for all channels
    std::vector<std::span<float const>> spans;
    spans.reserve(num_channels);
    for (auto const & ch: result.channels) {
        spans.push_back(ch->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1))));
    }

    // Pre-allocated buffers per channel
    std::vector<std::vector<float>> pixel_buffers(num_channels,
                                                  std::vector<float>(canvas_width * 4));

    for (auto _: state) {
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            decimateSpanInPlace(spans[ch], canvas_width, pixel_buffers[ch]);
        }
        benchmark::DoNotOptimize(pixel_buffers[0].data());
        benchmark::ClobberMemory();
    }

    auto const total_samples = static_cast<int64_t>(num_samples * num_channels);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * total_samples);
    state.counters["samples_per_ch"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
    state.counters["canvas_width"] = static_cast<double>(canvas_width);
    state.counters["total_output_verts"] = static_cast<double>(canvas_width * 2 * num_channels);
    state.counters["reduction_factor"] =
            static_cast<double>(num_samples * num_channels) /
            static_cast<double>(canvas_width * 2 * num_channels);
}

// ============================================================================
// C1.3: Multi-channel raw — BlockCachedMmap (warm cache)
// ============================================================================
static void BM_MultiCh_Raw_BlockCached(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result_data = createBlockCachedMmapSeries(num_channels, num_samples);

    // Cache viewValues() ranges for all channels + warm the block cache
    std::vector<decltype(result_data.channels[0]->viewValues())> channel_views;
    channel_views.reserve(num_channels);
    for (auto const & ch: result_data.channels) {
        auto vals = ch->viewValues();
        for (float v: vals) {
            benchmark::DoNotOptimize(v);
        }
        channel_views.push_back(ch->viewValues());
    }

    for (auto _: state) {
        std::vector<float> all_vertices;
        all_vertices.reserve(num_samples * num_channels * 2);

        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            std::size_t i = 0;
            for (float v: channel_views[ch]) {
                all_vertices.push_back(static_cast<float>(i));
                all_vertices.push_back(v + static_cast<float>(ch));
                ++i;
            }
        }

        benchmark::DoNotOptimize(all_vertices.data());
        benchmark::ClobberMemory();
    }

    auto const total_samples = static_cast<int64_t>(num_samples * num_channels);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * total_samples);
    state.counters["samples_per_ch"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);

    cleanupTempFile(result_data.file_path);
}

// ============================================================================
// C1.4: Multi-channel decimated — BlockCachedMmap + generic decimation
// ============================================================================
static void BM_MultiCh_Decimated_BlockCached(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto const canvas_width = static_cast<std::size_t>(state.range(2));
    auto result_data = createBlockCachedMmapSeries(num_channels, num_samples);

    // Warm all channels and prepare viewValues ranges
    std::vector<decltype(result_data.channels[0]->viewValues())> channel_views;
    channel_views.reserve(num_channels);
    for (auto const & ch: result_data.channels) {
        for (float v: ch->viewValues()) {
            benchmark::DoNotOptimize(v);
        }
        channel_views.push_back(ch->viewValues());
    }

    // Pre-allocated buffers
    std::vector<std::vector<float>> pixel_buffers(num_channels,
                                                  std::vector<float>(canvas_width * 4));

    for (auto _: state) {
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            auto const & vals = channel_views[ch];
            decimateGenericInPlace(
                    [&vals](std::size_t i) { return vals[i]; },
                    num_samples, canvas_width, pixel_buffers[ch]);
        }
        benchmark::DoNotOptimize(pixel_buffers[0].data());
        benchmark::ClobberMemory();
    }

    auto const total_samples = static_cast<int64_t>(num_samples * num_channels);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * total_samples);
    state.counters["samples_per_ch"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
    state.counters["canvas_width"] = static_cast<double>(canvas_width);

    cleanupTempFile(result_data.file_path);
}

// ============================================================================
// C1.5: Channel scaling test — fixed window, vary channel count
//       This isolates the scaling factor from channel count alone
// ============================================================================
static void BM_ChannelScaling_Decimated(benchmark::State & state) {
    auto const num_channels = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t num_samples = 100'000;// 100K samples (e.g. 100s at 1kHz)
    constexpr std::size_t canvas_width = 1920;  // Full HD width
    auto result = createTensorColumnSeries(num_channels, num_samples);

    std::vector<std::span<float const>> spans;
    spans.reserve(num_channels);
    for (auto const & ch: result.channels) {
        spans.push_back(ch->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1))));
    }

    std::vector<std::vector<float>> pixel_buffers(num_channels,
                                                  std::vector<float>(canvas_width * 4));

    for (auto _: state) {
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            decimateSpanInPlace(spans[ch], canvas_width, pixel_buffers[ch]);
        }
        benchmark::DoNotOptimize(pixel_buffers[0].data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples * num_channels));
    state.counters["channels"] = static_cast<double>(num_channels);
    state.counters["samples_per_ch"] = static_cast<double>(num_samples);
    state.counters["total_output_verts"] = static_cast<double>(canvas_width * 2 * num_channels);
}

// ============================================================================
// Registration
// ============================================================================

// Raw vertices (no decimation): {samples, channels}
BENCHMARK(BM_MultiCh_Raw_TensorColumn)
        ->Args({30'000, 32})
        ->Args({30'000, 128})
        ->Args({30'000, 384})
        ->Args({100'000, 32})
        ->Args({100'000, 128})
        ->Args({100'000, 384})
        ->Args({300'000, 32})
        ->Args({300'000, 128})
        ->Unit(benchmark::kMillisecond);

// Decimated: {samples, channels, canvas_width}
BENCHMARK(BM_MultiCh_Decimated_TensorColumn)
        ->Args({30'000, 32, 1920})
        ->Args({30'000, 128, 1920})
        ->Args({30'000, 384, 1920})
        ->Args({100'000, 32, 1920})
        ->Args({100'000, 128, 1920})
        ->Args({100'000, 384, 1920})
        ->Args({300'000, 32, 1920})
        ->Args({300'000, 128, 1920})
        ->Unit(benchmark::kMillisecond);

// BlockCachedMmap raw: {samples, channels}
BENCHMARK(BM_MultiCh_Raw_BlockCached)
        ->Args({30'000, 32})
        ->Args({30'000, 128})
        ->Args({100'000, 32})
        ->Unit(benchmark::kMillisecond);

// BlockCachedMmap decimated: {samples, channels, canvas_width}
BENCHMARK(BM_MultiCh_Decimated_BlockCached)
        ->Args({30'000, 32, 1920})
        ->Args({30'000, 128, 1920})
        ->Args({100'000, 32, 1920})
        ->Unit(benchmark::kMillisecond);

// Channel count scaling: {channels}
BENCHMARK(BM_ChannelScaling_Decimated)
        ->Arg(1)
        ->Arg(4)
        ->Arg(16)
        ->Arg(32)
        ->Arg(64)
        ->Arg(128)
        ->Arg(256)
        ->Arg(384)
        ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
