/**
 * @file bm_perpixel_buffer.cpp
 * @brief Benchmark: Per-pixel pre-allocated output buffer strategy.
 *
 * Tests the "allocate once at canvas resize, reuse every frame" approach:
 *   - Allocate a float buffer sized to canvas_width × 2 (one x,y per pixel column)
 *   - Decimate directly into it (MinMaxDecimator::decimateSpanInPlace)
 *   - Only reallocate on canvas resize (very rare)
 *
 * Compares against the current path: allocate a new vertex vector every frame.
 *
 * Run:
 *   ./out/build/Clang/Release/bin/adhoc/bm_perpixel_buffer
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
// B2.1: Per-pixel buffer — single channel, varying canvas width
// ============================================================================
static void BM_PerPixel_SingleChannel(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const canvas_width = static_cast<std::size_t>(state.range(1));
    auto [series, tf] = createVectorSeries(num_samples);

    auto span = series->getDataInTimeFrameIndexRange(
            TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));

    // Pre-allocate per-pixel buffer (one x,y pair per bin × 2 for min/max)
    std::vector<float> pixel_buffer(canvas_width * 4);

    for (auto _: state) {
        auto written = decimateSpanInPlace(span, canvas_width, pixel_buffer);
        benchmark::DoNotOptimize(pixel_buffer.data());
        benchmark::DoNotOptimize(written);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["canvas_width"] = static_cast<double>(canvas_width);
    state.counters["output_verts"] = static_cast<double>(canvas_width * 2);
}

// ============================================================================
// B2.2: Per-pixel buffer — multi-channel (simulating scrolling all channels)
// ============================================================================
static void BM_PerPixel_MultiChannel(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const canvas_width = static_cast<std::size_t>(state.range(1));
    auto const num_channels = static_cast<std::size_t>(state.range(2));
    auto result = createTensorColumnSeries(num_channels, num_samples);

    // Collect contiguous spans for all channels
    std::vector<std::span<float const>> channel_spans;
    channel_spans.reserve(num_channels);
    for (auto const & ch: result.channels) {
        channel_spans.push_back(ch->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1))));
    }

    // Per-channel pixel buffers (in practice: one large buffer with offsets)
    std::vector<std::vector<float>> pixel_buffers(num_channels,
                                                  std::vector<float>(canvas_width * 4));

    for (auto _: state) {
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            decimateSpanInPlace(channel_spans[ch], canvas_width, pixel_buffers[ch]);
        }
        benchmark::DoNotOptimize(pixel_buffers[0].data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples * num_channels));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["canvas_width"] = static_cast<double>(canvas_width);
    state.counters["channels"] = static_cast<double>(num_channels);
    state.counters["total_verts"] = static_cast<double>(canvas_width * 2 * num_channels);
}

// ============================================================================
// B2.3: Baseline — allocate new vector every frame (current behavior)
// ============================================================================
static void BM_AllocEveryFrame_SingleChannel(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto const canvas_width = static_cast<std::size_t>(state.range(1));
    auto [series, tf] = createVectorSeries(num_samples);

    auto span = series->getDataInTimeFrameIndexRange(
            TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));

    for (auto _: state) {
        auto result = decimateSpan(span, canvas_width);
        benchmark::DoNotOptimize(result.vertices.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
    state.counters["canvas_width"] = static_cast<double>(canvas_width);
}

// ============================================================================
// B2.4: Simulate canvas resize — measure reallocation cost
// ============================================================================
static void BM_PerPixel_CanvasResize(benchmark::State & state) {
    auto const num_samples = static_cast<std::size_t>(state.range(0));
    auto [series, tf] = createVectorSeries(num_samples);

    auto span = series->getDataInTimeFrameIndexRange(
            TimeFrameIndex(0), TimeFrameIndex(static_cast<int64_t>(num_samples - 1)));

    // Alternate between two canvas sizes (simulating resizes)
    constexpr std::size_t width_a = 1000;
    constexpr std::size_t width_b = 2000;
    std::vector<float> pixel_buffer;
    bool toggle = false;

    for (auto _: state) {
        auto const width = toggle ? width_b : width_a;
        toggle = !toggle;

        if (pixel_buffer.size() < width * 4) {
            pixel_buffer.resize(width * 4);
        }

        auto written = decimateSpanInPlace(span, width, pixel_buffer);
        benchmark::DoNotOptimize(pixel_buffer.data());
        benchmark::DoNotOptimize(written);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_samples));
    state.counters["samples"] = static_cast<double>(num_samples);
}

// ============================================================================
// Registration
// ============================================================================

// Single channel: {samples, canvas_width}
BENCHMARK(BM_PerPixel_SingleChannel)
        ->Args({100'000, 500})
        ->Args({100'000, 1000})
        ->Args({100'000, 2000})
        ->Args({100'000, 4000})
        ->Args({500'000, 1000})
        ->Args({500'000, 2000})
        ->Args({500'000, 4000})
        ->Args({1'000'000, 1000})
        ->Args({1'000'000, 2000})
        ->Args({1'000'000, 4000})
        ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AllocEveryFrame_SingleChannel)
        ->Args({100'000, 1000})
        ->Args({100'000, 2000})
        ->Args({500'000, 1000})
        ->Args({500'000, 2000})
        ->Args({1'000'000, 1000})
        ->Args({1'000'000, 2000})
        ->Unit(benchmark::kMicrosecond);

// Multi-channel: {samples, canvas_width, channels}
BENCHMARK(BM_PerPixel_MultiChannel)
        ->Args({100'000, 1000, 32})
        ->Args({100'000, 2000, 32})
        ->Args({100'000, 1000, 384})
        ->Args({100'000, 2000, 384})
        ->Args({500'000, 1000, 32})
        ->Args({500'000, 1000, 384})
        ->Unit(benchmark::kMicrosecond);

// Canvas resize
BENCHMARK(BM_PerPixel_CanvasResize)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
