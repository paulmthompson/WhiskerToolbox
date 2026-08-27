/**
 * @file bm_cache_extraction.cpp
 * @brief Benchmark: AnalogVertexCache extraction cost.
 *
 * Measures the cost of getVerticesForRange() — copying from the ring buffer
 * to a flat float vector — which happens every scene rebuild even with a
 * warm cache. Compares against a hypothetical zero-copy approach.
 *
 * Run:
 *   ./out/build/Clang/Release/bin/adhoc/bm_cache_extraction
 */

#include "AnalogVertexCache.hpp"

#include "TimeFrame/TimeFrame.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace DataViewer;

// ============================================================================
// Helper: populate a cache with synthetic data
// ============================================================================
static AnalogVertexCache populateCache(std::size_t num_vertices) {
    AnalogVertexCache cache;
    cache.initialize(num_vertices + 100);// slight margin

    std::vector<CachedAnalogVertex> vertices;
    vertices.reserve(num_vertices);
    for (std::size_t i = 0; i < num_vertices; ++i) {
        vertices.push_back(CachedAnalogVertex{
                ClockTicks(static_cast<int64_t>(i)),               // x
                static_cast<float>(i) * 0.001f,        // y
                TimeFrameIndex(static_cast<int64_t>(i))// time_idx
        });
    }

    cache.setVertices(vertices,
                      TimeFrameIndex(0),
                      TimeFrameIndex(static_cast<int64_t>(num_vertices)));
    return cache;
}

// ============================================================================
// A3.1: Current path — getVerticesForRange (allocates + copies every call)
// ============================================================================
static void BM_CacheExtract_Current(benchmark::State & state) {
    auto const num_vertices = static_cast<std::size_t>(state.range(0));
    auto cache = populateCache(num_vertices);

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_vertices));

    for (auto _: state) {
        auto flat = cache.getVerticesForRange(start, end, ClockTicks(0));
        benchmark::DoNotOptimize(flat.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_vertices));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_vertices) * 2 * sizeof(float));
    state.counters["vertices"] = static_cast<double>(num_vertices);
    state.counters["bytes_copied"] = static_cast<double>(num_vertices * 2 * sizeof(float));
}

// ============================================================================
// A3.2: Hypothetical — pre-allocated extraction buffer (reuse across calls)
// ============================================================================
static void BM_CacheExtract_PreAlloc(benchmark::State & state) {
    auto const num_vertices = static_cast<std::size_t>(state.range(0));
    auto cache = populateCache(num_vertices);

    TimeFrameIndex const start(0);
    TimeFrameIndex const end(static_cast<int64_t>(num_vertices));

    // Pre-allocate once (simulating a persistent buffer)
    std::vector<float> reuse_buffer(num_vertices * 2);

    for (auto _: state) {
        // Still calls getVerticesForRange (no zero-copy API exists),
        // but demonstrates the cost difference if we could write directly
        auto flat = cache.getVerticesForRange(start, end, ClockTicks(0));
        // Simulate copying into pre-allocated buffer
        std::copy(flat.begin(), flat.end(), reuse_buffer.begin());
        benchmark::DoNotOptimize(reuse_buffer.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_vertices));
    state.counters["vertices"] = static_cast<double>(num_vertices);
}

// ============================================================================
// A3.3: Simulated zero-copy — direct linear copy from a contiguous source
//       (lower bound: what it would cost if cache were contiguous)
// ============================================================================
static void BM_CacheExtract_ZeroCopyBaseline(benchmark::State & state) {
    auto const num_vertices = static_cast<std::size_t>(state.range(0));

    // Contiguous source (simulating what a flat cache would look like)
    std::vector<float> source(num_vertices * 2);
    for (std::size_t i = 0; i < num_vertices; ++i) {
        source[i * 2 + 0] = static_cast<float>(i);
        source[i * 2 + 1] = static_cast<float>(i) * 0.001f;
    }

    std::vector<float> dest(num_vertices * 2);

    for (auto _: state) {
        std::memcpy(dest.data(), source.data(), num_vertices * 2 * sizeof(float));
        benchmark::DoNotOptimize(dest.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_vertices));
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_vertices) * 2 * sizeof(float));
    state.counters["vertices"] = static_cast<double>(num_vertices);
}

// ============================================================================
// A3.4: Incremental scroll — only regenerate edge data
//       (measures overhead of getMissingRanges + partial operations)
// ============================================================================
static void BM_CacheScroll_Incremental(benchmark::State & state) {
    auto const num_vertices = static_cast<std::size_t>(state.range(0));
    auto const scroll_amount = static_cast<std::size_t>(state.range(1));
    auto cache = populateCache(num_vertices);

    int64_t offset = 0;

    for (auto _: state) {
        offset += static_cast<int64_t>(scroll_amount);
        TimeFrameIndex const start(offset);
        TimeFrameIndex const end(offset + static_cast<int64_t>(num_vertices));

        if (cache.needsUpdate(start, end)) {
            auto missing = cache.getMissingRanges(start, end);
            for (auto const & range: missing) {
                std::vector<CachedAnalogVertex> new_vertices;
                auto const count = static_cast<std::size_t>(
                        range.end.getValue() - range.start.getValue());
                new_vertices.reserve(count);
                for (std::size_t i = 0; i < count; ++i) {
                    auto const idx = range.start.getValue() + static_cast<int64_t>(i);
                    new_vertices.push_back(CachedAnalogVertex{
                            ClockTicks(idx),
                            static_cast<float>(idx) * 0.001f,
                            TimeFrameIndex(idx)});
                }
                if (range.prepend) {
                    cache.prependVertices(new_vertices, range.start);
                } else {
                    cache.appendVertices(new_vertices, range.end);
                }
            }
        }

        auto flat = cache.getVerticesForRange(start, end, ClockTicks(offset));
        benchmark::DoNotOptimize(flat.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(num_vertices));
    state.counters["window"] = static_cast<double>(num_vertices);
    state.counters["scroll"] = static_cast<double>(scroll_amount);
}

// ============================================================================
// Registration
// ============================================================================

BENCHMARK(BM_CacheExtract_Current)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CacheExtract_PreAlloc)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CacheExtract_ZeroCopyBaseline)
        ->Arg(10'000)
        ->Arg(100'000)
        ->Arg(500'000)
        ->Arg(1'000'000)
        ->Unit(benchmark::kMicrosecond);

// {window_size, scroll_amount}
BENCHMARK(BM_CacheScroll_Incremental)
        ->Args({100'000, 10})
        ->Args({100'000, 100})
        ->Args({100'000, 1000})
        ->Args({500'000, 100})
        ->Args({500'000, 1000})
        ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
