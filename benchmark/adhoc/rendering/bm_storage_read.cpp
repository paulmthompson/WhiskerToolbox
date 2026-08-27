/**
 * @file bm_storage_read.cpp
 * @brief Benchmark: Sequential read throughput for each AnalogTimeSeries storage backend.
 *
 * Measures raw data access speed independent of vertex generation.
 * Compares: VectorAnalogDataStorage, TensorColumnAnalogStorage (Armadillo),
 *           BlockCachedMmapAnalogStorage, MemoryMappedAnalogDataStorage.
 *
 * For contiguous backends, also benchmarks the fast tryGetCache() raw pointer
 * loop vs the getValueAt() virtual dispatch path.
 *
 * Run:
 *   ./out/build/Clang/Release/bin/adhoc/bm_storage_read
 *   ./out/build/Clang/Release/bin/adhoc/bm_storage_read --benchmark_filter="BM_Vector.*"
 */

#include "SyntheticDataFactory.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/storage/AnalogDataStorageCache.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <numeric>

using namespace BenchmarkSynthetic;

// ============================================================================
// A1.1: Vector storage — getValueAt (virtual dispatch path)
// ============================================================================
static void BM_Vector_GetValueAt(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    for (auto _: state) {
        float sum = 0.0f;
        for (float v: series->viewValues()) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples) * sizeof(float));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// A1.2: Vector storage — getSpanRange (zero-copy span)
// ============================================================================
static void BM_Vector_SpanRange(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    for (auto _: state) {
        auto span = series->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));
        float sum = 0.0f;
        for (float v: span) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// A1.3: Vector storage — raw pointer via viewValues()
// ============================================================================
static void BM_Vector_RawPointer(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    for (auto _: state) {
        float sum = 0.0f;
        for (float v: series->viewValues()) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// A1.4: TensorColumn storage — getValueAt (virtual dispatch)
// ============================================================================
static void BM_TensorColumn_GetValueAt(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createTensorColumnSeries(num_channels, num_samples);
    auto & series = result.channels[0];// benchmark first channel

    for (auto _: state) {
        float sum = 0.0f;
        for (float v: series->viewValues()) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
}

// ============================================================================
// A1.5: TensorColumn storage — getSpanRange (zero-copy)
// ============================================================================
static void BM_TensorColumn_SpanRange(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createTensorColumnSeries(num_channels, num_samples);
    auto & series = result.channels[0];

    for (auto _: state) {
        auto span = series->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));
        float sum = 0.0f;
        for (float v: span) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
}

// ============================================================================
// A1.6: BlockCachedMmap — sequential read (warm cache)
// ============================================================================
static void BM_BlockCached_Warm(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createBlockCachedMmapSeries(num_channels, num_samples);
    auto & series = result.channels[0];

    // Warm up the cache by reading all samples once
    for (float v: series->viewValues()) {
        benchmark::DoNotOptimize(v);
    }

    for (auto _: state) {
        float sum = 0.0f;
        for (float v: series->viewValues()) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);

    cleanupTempFile(result.file_path);
}

// ============================================================================
// A1.7: BlockCachedMmap — cold cache (first access triggers block loads)
// ============================================================================
static void BM_BlockCached_Cold(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));

    for (auto _: state) {
        state.PauseTiming();
        auto result = createBlockCachedMmapSeries(num_channels, num_samples);
        auto & series = result.channels[0];
        state.ResumeTiming();

        float sum = 0.0f;
        for (float v: series->viewValues()) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);

        state.PauseTiming();
        cleanupTempFile(result.file_path);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
}

// ============================================================================
// A1.8: MemoryMapped — per-element read (strided, type conversion)
// ============================================================================
static void BM_Mmap_PerElement(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createMemoryMappedSeries(num_channels, num_samples);
    auto & series = result.channels[0];// first channel

    for (auto _: state) {
        float sum = 0.0f;
        for (float v: series->viewValues()) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);

    cleanupTempFile(result.file_path);
}

// ============================================================================
// A1.9: viewTimeValueRange — the actual rendering path (Vector storage)
// ============================================================================
static void BM_Vector_ViewTimeValueRange(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_samples - 1));

    for (auto _: state) {
        float sum = 0.0f;
        for (auto const & tv: series->getTimeValueRangeInTimeFrameIndexRange(start, end)) {
            sum += tv.value();
        }
        benchmark::DoNotOptimize(sum);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// Registration
// ============================================================================

// Vector storage
BENCHMARK(BM_Vector_GetValueAt)->Arg(10'000)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Vector_SpanRange)->Arg(10'000)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Vector_RawPointer)->Arg(10'000)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Vector_ViewTimeValueRange)->Arg(10'000)->Arg(100'000)->Arg(1'000'000)->Unit(benchmark::kMicrosecond);

// TensorColumn storage: {samples, channels}
BENCHMARK(BM_TensorColumn_GetValueAt)->Args({100'000, 32})->Args({100'000, 384})->Args({1'000'000, 32})->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_TensorColumn_SpanRange)->Args({100'000, 32})->Args({100'000, 384})->Args({1'000'000, 32})->Unit(benchmark::kMicrosecond);

// BlockCachedMmap: {samples, channels}
BENCHMARK(BM_BlockCached_Warm)->Args({10'000, 32})->Args({100'000, 32})->Args({10'000, 384})->Args({100'000, 384})->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_BlockCached_Cold)->Args({10'000, 32})->Args({100'000, 32})->Unit(benchmark::kMillisecond);

// MemoryMapped: {samples, channels}
BENCHMARK(BM_Mmap_PerElement)->Args({10'000, 32})->Args({10'000, 384})->Args({100'000, 32})->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
