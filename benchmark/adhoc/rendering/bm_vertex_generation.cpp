/**
 * @file bm_vertex_generation.cpp
 * @brief Benchmark: End-to-end vertex generation from AnalogTimeSeries.
 *
 * Reproduces the SceneBuildingHelpers::generateVerticesForRange() hot path
 * without Qt/OpenGL dependencies. Compares:
 *   - Current pipeline: mapAnalogSeriesWithIndices() → materialize to vector
 *   - Proposed span-based fast path: getSpanRange() + arithmetic X coords
 *
 * Run:
 *   ./out/build/Clang/Release/bin/adhoc/bm_vertex_generation
 */

#include "SyntheticDataFactory.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace BenchmarkSynthetic;

// ============================================================================
// Minimal SeriesLayout for the mapper (no actual positioning)
// ============================================================================
static CorePlotting::SeriesLayout makeLocalLayout() {
    CorePlotting::SeriesLayout layout;
    layout.y_transform.offset = 0.0f;
    return layout;
}

// ============================================================================
// A2.1: Current pipeline — mapAnalogSeriesWithIndices → materialize
// ============================================================================
static void BM_CurrentPipeline_Vector(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);
    auto layout = makeLocalLayout();

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_samples - 1));

    for (auto _: state) {
        auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
                *series, layout, *tf, 1.0f, start, end);

        // Materialize to flat float vector (like buildAnalogSeriesBatchCached does)
        std::vector<float> vertices;
        vertices.reserve(num_samples * 2);
        for (auto const & v: mapped_range) {
            vertices.push_back(v.x);
            vertices.push_back(v.y);
        }
        benchmark::DoNotOptimize(vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples) * 2 * sizeof(float));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["vertices_per_sample"] = 1.0;
}

// ============================================================================
// A2.2: Span-based fast path — getSpanRange + arithmetic X
// ============================================================================
static void BM_SpanFastPath_Vector(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_samples - 1));

    for (auto _: state) {
        // Get contiguous span directly (bypasses virtual dispatch per sample)
        auto span = series->getDataInTimeFrameIndexRange(start, end);

        std::vector<float> vertices;
        vertices.reserve(span.size() * 2);
        for (std::size_t i = 0; i < span.size(); ++i) {
            vertices.push_back(static_cast<float>(i));// x = sample index
            vertices.push_back(span[i]);              // y = data value
        }
        benchmark::DoNotOptimize(vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples) * 2 * sizeof(float));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// A2.3: Current pipeline with TensorColumn storage
// ============================================================================
static void BM_CurrentPipeline_TensorColumn(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createTensorColumnSeries(num_channels, num_samples);
    auto & series = result.channels[0];
    auto layout = makeLocalLayout();

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_samples - 1));

    for (auto _: state) {
        auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
                *series, layout, *result.time_frame, 1.0f, start, end);

        std::vector<float> vertices;
        vertices.reserve(num_samples * 2);
        for (auto const & v: mapped_range) {
            vertices.push_back(v.x);
            vertices.push_back(v.y);
        }
        benchmark::DoNotOptimize(vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);
}

// ============================================================================
// A2.4: Current pipeline with BlockCachedMmap storage
// ============================================================================
static void BM_CurrentPipeline_BlockCached(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const num_channels = static_cast<std::size_t>(state.range(1));
    auto result = createBlockCachedMmapSeries(num_channels, num_samples);
    auto & series = result.channels[0];
    auto layout = makeLocalLayout();

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_samples - 1));

    // Warm the cache
    for (float v: series->viewValues()) {
        benchmark::DoNotOptimize(v);
    }

    for (auto _: state) {
        auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
                *series, layout, *result.time_frame, 1.0f, start, end);

        std::vector<float> vertices;
        vertices.reserve(num_samples * 2);
        for (auto const & v: mapped_range) {
            vertices.push_back(v.x);
            vertices.push_back(v.y);
        }
        benchmark::DoNotOptimize(vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["channels"] = static_cast<double>(num_channels);

    cleanupTempFile(result.file_path);
}

// ============================================================================
// A2.5: getTimeValueSpanInTimeFrameIndexRange — existing zero-copy API
//       that the rendering pipeline doesn't currently use
// ============================================================================
static void BM_SpanPairPath_Vector(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_samples - 1));

    for (auto _: state) {
        auto span_pair = series->getTimeValueSpanInTimeFrameIndexRange(start, end);

        std::vector<float> vertices;
        vertices.reserve(span_pair.values.size() * 2);

        std::size_t idx = 0;
        for (float v: span_pair.values) {
            vertices.push_back(static_cast<float>(idx));// x
            vertices.push_back(v);                      // y
            ++idx;
        }
        benchmark::DoNotOptimize(vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// Registration
// ============================================================================

// Current pipeline (the path actually used by DataViewer today)
BENCHMARK(BM_CurrentPipeline_Vector)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

// Span-based fast path (hypothetical optimization)
BENCHMARK(BM_SpanFastPath_Vector)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

// getTimeValueSpanInTimeFrameIndexRange path
BENCHMARK(BM_SpanPairPath_Vector)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

// TensorColumn: {samples, channels}
BENCHMARK(BM_CurrentPipeline_TensorColumn)
        ->Args({100'000, 32})
        ->Args({100'000, 384})
        ->Args({1'000'000, 32})
        ->Unit(benchmark::kMicrosecond);

// BlockCachedMmap: {samples, channels}
BENCHMARK(BM_CurrentPipeline_BlockCached)
        ->Args({10'000, 32})
        ->Args({100'000, 32})
        ->Args({10'000, 384})
        ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
