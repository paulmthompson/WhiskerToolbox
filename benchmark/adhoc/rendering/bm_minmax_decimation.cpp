/**
 * @file bm_minmax_decimation.cpp
 * @brief Benchmark: Min/max decimation algorithm performance.
 *
 * Compares decimation throughput across:
 *   - Contiguous span path (fastest)
 *   - Generic per-element path (any storage)
 *   - Different input sizes and output bin counts
 *   - Allocating vs in-place variants
 *
 * Also compares total pipeline time (decimate + vertex construction) vs
 * current pipeline (raw vertex generation without decimation).
 *
 * Run:
 *   ./out/build/Clang/Release/bin/adhoc/bm_minmax_decimation
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
// B1.1: Contiguous span decimation (allocating)
// ============================================================================
static void BM_Decimate_Span_Alloc(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_bins = static_cast<std::size_t>(state.range(1));
    auto [series, tf] = createVectorSeries(num_samples);

    // Get span once
    auto span = series->getDataInTimeFrameIndexRange(
            TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));

    for (auto _: state) {
        auto result = decimateSpan(span, num_bins);
        benchmark::DoNotOptimize(result.vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["input_samples"] = static_cast<double>(num_samples);
    state.counters["output_bins"] = static_cast<double>(num_bins);
    state.counters["decimation_ratio"] = static_cast<double>(num_samples) / static_cast<double>(num_bins * 2);
}

// ============================================================================
// B1.2: Contiguous span decimation (in-place, zero alloc after warmup)
// ============================================================================
static void BM_Decimate_Span_InPlace(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_bins = static_cast<std::size_t>(state.range(1));
    auto [series, tf] = createVectorSeries(num_samples);

    auto span = series->getDataInTimeFrameIndexRange(
            TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));

    // Pre-allocate output buffer
    std::vector<float> out_buffer(num_bins * 4);

    for (auto _: state) {
        auto written = decimateSpanInPlace(span, num_bins, out_buffer);
        benchmark::DoNotOptimize(out_buffer.data());
        benchmark::DoNotOptimize(written);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["input_samples"] = static_cast<double>(num_samples);
    state.counters["output_bins"] = static_cast<double>(num_bins);
}

// ============================================================================
// B1.3: Generic per-element decimation (works with any storage)
// ============================================================================
static void BM_Decimate_Generic_Vector(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_bins = static_cast<std::size_t>(state.range(1));
    auto [series, tf] = createVectorSeries(num_samples);

    // viewValues() returns a random-access range backed by views::iota + transform
    auto vals = series->viewValues();

    for (auto _: state) {
        auto result = decimateGeneric(
                [&vals](std::size_t i) { return vals[i]; },
                num_samples, num_bins);
        benchmark::DoNotOptimize(result.vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["input_samples"] = static_cast<double>(num_samples);
    state.counters["output_bins"] = static_cast<double>(num_bins);
}

// ============================================================================
// B1.4: Generic decimation with BlockCachedMmap storage
// ============================================================================
static void BM_Decimate_Generic_BlockCached(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_bins = static_cast<std::size_t>(state.range(1));
    auto const num_channels = static_cast<std::size_t>(state.range(2));
    auto result_data = createBlockCachedMmapSeries(num_channels, num_samples);
    auto & series = result_data.channels[0];

    // Warm cache
    for (float v: series->viewValues()) {
        benchmark::DoNotOptimize(v);
    }

    auto vals = series->viewValues();

    for (auto _: state) {
        auto result = decimateGeneric(
                [&vals](std::size_t i) { return vals[i]; },
                num_samples, num_bins);
        benchmark::DoNotOptimize(result.vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["input_samples"] = static_cast<double>(num_samples);
    state.counters["output_bins"] = static_cast<double>(num_bins);
    state.counters["channels"] = static_cast<double>(num_channels);

    cleanupTempFile(result_data.file_path);
}

// ============================================================================
// B1.5: Comparison baseline — raw vertex generation (no decimation)
//       Same as current pipeline: 1 vertex per sample
// ============================================================================
static void BM_RawVertices_NoDecimation(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    auto span = series->getDataInTimeFrameIndexRange(
            TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));

    for (auto _: state) {
        std::vector<float> vertices;
        vertices.reserve(num_samples * 2);
        for (std::size_t i = 0; i < span.size(); ++i) {
            vertices.push_back(static_cast<float>(i));
            vertices.push_back(span[i]);
        }
        benchmark::DoNotOptimize(vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples) * 2 * sizeof(float));
    state.counters["input_samples"] = static_cast<double>(num_samples);
    state.counters["output_vertices"] = static_cast<double>(num_samples);
}

// ============================================================================
// Registration
// ============================================================================

// Span decimation: {samples, bins}
BENCHMARK(BM_Decimate_Span_Alloc)
        ->Args({100'000, 500})
        ->Args({100'000, 1000})
        ->Args({100'000, 2000})
        ->Args({100'000, 4000})
        ->Args({500'000, 1000})
        ->Args({500'000, 2000})
        ->Args({1'000'000, 1000})
        ->Args({1'000'000, 2000})
        ->Args({1'000'000, 4000})
        ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Decimate_Span_InPlace)
        ->Args({100'000, 500})
        ->Args({100'000, 1000})
        ->Args({100'000, 2000})
        ->Args({100'000, 4000})
        ->Args({500'000, 1000})
        ->Args({500'000, 2000})
        ->Args({1'000'000, 1000})
        ->Args({1'000'000, 2000})
        ->Args({1'000'000, 4000})
        ->Unit(benchmark::kMicrosecond);

// Generic per-element: {samples, bins}
BENCHMARK(BM_Decimate_Generic_Vector)
        ->Args({100'000, 1000})
        ->Args({100'000, 2000})
        ->Args({1'000'000, 1000})
        ->Args({1'000'000, 2000})
        ->Unit(benchmark::kMicrosecond);

// BlockCached generic: {samples, bins, channels}
BENCHMARK(BM_Decimate_Generic_BlockCached)
        ->Args({100'000, 1000, 32})
        ->Args({100'000, 2000, 32})
        ->Args({100'000, 1000, 384})
        ->Args({100'000, 2000, 384})
        ->Unit(benchmark::kMicrosecond);

// Baseline comparison (no decimation)
BENCHMARK(BM_RawVertices_NoDecimation)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
