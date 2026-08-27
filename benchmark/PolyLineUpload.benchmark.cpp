#include "PlottingOpenGL/Renderers/StreamingPolyLineRenderer.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <benchmark/benchmark.h>

#include <chrono>
#include <random>
#include <vector>

/**
 * @file PolyLineUpload.benchmark.cpp
 * @brief Benchmarks for comparing full vs incremental buffer update strategies
 * 
 * These benchmarks measure the CPU-side cost of:
 * 1. Building vertex data for analog time series
 * 2. Comparing old vs new data (for incremental updates)
 * 3. Copying data to intermediate buffers
 * 
 * NOTE: These benchmarks run without an OpenGL context, so they measure
 * CPU-side logic only, not actual GPU upload times. The actual glBufferData
 * and glBufferSubData calls would need to be tested in an integration test
 * with a real OpenGL context.
 */

namespace {

/**
 * @brief Generate synthetic analog time series data
 * 
 * Creates a sine wave pattern with optional noise for realistic testing.
 */
std::vector<float> generateSineWaveVertices(size_t point_count, 
                                             int64_t start_time, 
                                             float frequency = 0.01f,
                                             float noise_amplitude = 0.0f) {
    std::vector<float> vertices;
    vertices.reserve(point_count * 2);
    
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> noise(-noise_amplitude, noise_amplitude);
    
    for (size_t i = 0; i < point_count; ++i) {
        float x = static_cast<float>(start_time + static_cast<int64_t>(i));
        float y = std::sin(x * frequency) + (noise_amplitude > 0 ? noise(gen) : 0.0f);
        vertices.push_back(x);
        vertices.push_back(y);
    }
    
    return vertices;
}

/**
 * @brief Create a RenderablePolyLineBatch from vertex data
 */
CorePlotting::RenderablePolyLineBatch createBatch(std::vector<float> vertices) {
    CorePlotting::RenderablePolyLineBatch batch;
    batch.vertices = std::move(vertices);
    batch.line_start_indices.push_back(0);
    batch.line_vertex_counts.push_back(static_cast<int32_t>(batch.vertices.size() / 2));
    batch.global_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    batch.thickness = 1.0f;
    batch.model_matrix = glm::mat4(1.0f);
    return batch;
}

/**
 * @brief Simulate full buffer rebuild (current strategy)
 * 
 * This simulates what happens in PolyLineRenderer::uploadData():
 * - Append new vertices to existing buffer
 * - Reallocate entire buffer
 */
void fullBufferRebuild(std::vector<float>& combined_buffer,
                       std::vector<float> const& new_vertices) {
    // Clear and copy (simulating clearData + uploadData pattern)
    combined_buffer.clear();
    combined_buffer.insert(combined_buffer.end(), 
                           new_vertices.begin(), 
                           new_vertices.end());
}

/**
 * @brief Simulate incremental buffer update (optimized strategy)
 * 
 * Only copies the changed portions of the data.
 */
struct DirtyRegion {
    size_t start;
    size_t end;
};

std::vector<DirtyRegion> findDirtyRegions(std::vector<float> const& old_data,
                                           std::vector<float> const& new_data,
                                           float tolerance = 0.0f) {
    std::vector<DirtyRegion> regions;
    
    if (old_data.size() != new_data.size()) {
        // Size changed - mark everything dirty
        regions.push_back({0, new_data.size()});
        return regions;
    }
    
    bool in_dirty_region = false;
    size_t dirty_start = 0;
    
    for (size_t i = 0; i < new_data.size(); ++i) {
        bool is_different = std::abs(new_data[i] - old_data[i]) > tolerance;
        
        if (is_different && !in_dirty_region) {
            dirty_start = i;
            in_dirty_region = true;
        } else if (!is_different && in_dirty_region) {
            regions.push_back({dirty_start, i});
            in_dirty_region = false;
        }
    }
    
    if (in_dirty_region) {
        regions.push_back({dirty_start, new_data.size()});
    }
    
    return regions;
}

void incrementalBufferUpdate(std::vector<float>& cached_buffer,
                              std::vector<float> const& new_data,
                              std::vector<DirtyRegion> const& regions) {
    for (auto const& region : regions) {
        std::copy(new_data.begin() + region.start,
                  new_data.begin() + region.end,
                  cached_buffer.begin() + region.start);
    }
}

}  // anonymous namespace


// ============================================================================
// Benchmark: Full Buffer Rebuild (Current Strategy)
// ============================================================================

static void BM_FullBufferRebuild(benchmark::State& state) {
    size_t const point_count = static_cast<size_t>(state.range(0));
    
    std::vector<float> combined_buffer;
    combined_buffer.reserve(point_count * 2);
    
    // Generate initial data
    auto vertices = generateSineWaveVertices(point_count, 0);
    
    for (auto _ : state) {
        fullBufferRebuild(combined_buffer, vertices);
        benchmark::DoNotOptimize(combined_buffer.data());
        benchmark::ClobberMemory();
    }
    
    state.SetBytesProcessed(state.iterations() * vertices.size() * sizeof(float));
    state.SetLabel("Full rebuild");
}

BENCHMARK(BM_FullBufferRebuild)
    ->Arg(1000)      // 1K points
    ->Arg(10000)     // 10K points
    ->Arg(100000)    // 100K points
    ->Arg(1000000);  // 1M points


// ============================================================================
// Benchmark: Scrolling Scenario - Full Rebuild
// ============================================================================

static void BM_ScrollingFullRebuild(benchmark::State& state) {
    size_t const point_count = static_cast<size_t>(state.range(0));
    int64_t const scroll_step = state.range(1);
    
    std::vector<float> combined_buffer;
    combined_buffer.reserve(point_count * 2);
    
    int64_t start_time = 0;
    
    for (auto _ : state) {
        // Simulate scrolling by shifting the time window
        auto vertices = generateSineWaveVertices(point_count, start_time);
        fullBufferRebuild(combined_buffer, vertices);
        benchmark::DoNotOptimize(combined_buffer.data());
        start_time += scroll_step;
    }
    
    state.SetBytesProcessed(state.iterations() * point_count * 2 * sizeof(float));
}

BENCHMARK(BM_ScrollingFullRebuild)
    ->Args({10000, 10})    // 10K points, scroll by 10
    ->Args({10000, 100})   // 10K points, scroll by 100
    ->Args({100000, 10})   // 100K points, scroll by 10
    ->Args({100000, 1000}); // 100K points, scroll by 1000


// ============================================================================
// Benchmark: Scrolling Scenario - Incremental Update
// 
// Simulates realistic scrolling where we extract different windows from
// a longer time series. When scrolling by S points out of N visible:
// - N-S points are "recycled" (same values, just need to shift indices)
// - S new points enter the view
// 
// This tests the dirty region detection overhead when most data is the same.
// ============================================================================

static void BM_ScrollingIncremental(benchmark::State& state) {
    size_t const point_count = static_cast<size_t>(state.range(0));
    size_t const scroll_step = static_cast<size_t>(state.range(1));
    
    // Simulate a longer underlying time series (10x visible window)
    size_t const total_points = point_count * 10;
    std::vector<float> full_series = generateSineWaveVertices(total_points, 0);
    
    std::vector<float> cached_buffer;
    
    // Initialize with first window [0, point_count)
    size_t window_start_point = 0;
    size_t const floats_per_point = 2;
    cached_buffer.assign(full_series.begin(), 
                         full_series.begin() + static_cast<std::ptrdiff_t>(point_count * floats_per_point));
    
    size_t total_bytes_updated = 0;
    int iterations_counted = 0;
    
    for (auto _ : state) {
        // Scroll: move window by scroll_step points
        window_start_point += scroll_step;
        if (window_start_point + point_count > total_points) {
            window_start_point = 0;  // Wrap around
        }
        
        // Extract new window from full series
        size_t const start_idx = window_start_point * floats_per_point;
        std::vector<float> new_vertices(
            full_series.begin() + static_cast<std::ptrdiff_t>(start_idx),
            full_series.begin() + static_cast<std::ptrdiff_t>(start_idx + point_count * floats_per_point));
        
        // Find dirty regions
        auto regions = findDirtyRegions(cached_buffer, new_vertices);
        
        // Count bytes that would be uploaded
        for (auto const& r : regions) {
            total_bytes_updated += (r.end - r.start) * sizeof(float);
        }
        
        // Apply incremental update
        if (!regions.empty()) {
            incrementalBufferUpdate(cached_buffer, new_vertices, regions);
        }
        
        benchmark::DoNotOptimize(cached_buffer.data());
        iterations_counted++;
    }
    
    state.SetBytesProcessed(state.iterations() * point_count * floats_per_point * sizeof(float));
    
    // Report actual bytes uploaded ratio
    if (iterations_counted > 0) {
        double const avg_bytes_per_iter = static_cast<double>(total_bytes_updated) / 
                                           static_cast<double>(iterations_counted);
        double const total_bytes = static_cast<double>(point_count * floats_per_point * sizeof(float));
        double const upload_pct = (avg_bytes_per_iter / total_bytes) * 100.0;
        state.counters["upload_%"] = benchmark::Counter(upload_pct, 
                                                         benchmark::Counter::kAvgThreads);
    }
}

BENCHMARK(BM_ScrollingIncremental)
    ->Args({10000, 10})    // 10K points, scroll by 10
    ->Args({10000, 100})   // 10K points, scroll by 100
    ->Args({100000, 10})   // 100K points, scroll by 10
    ->Args({100000, 1000}); // 100K points, scroll by 1000


// ============================================================================
// Benchmark: Ring Buffer Strategy for Scrolling
// 
// Alternative approach: maintain a larger buffer and use modular arithmetic
// to avoid copying unchanged data. This simulates what a ring buffer
// implementation would achieve.
// ============================================================================

static void BM_ScrollingRingBuffer(benchmark::State& state) {
    size_t const visible_points = static_cast<size_t>(state.range(0));
    size_t const scroll_step = static_cast<size_t>(state.range(1));
    
    // Ring buffer holds 3x visible window
    size_t const ring_buffer_points = visible_points * 3;
    size_t const floats_per_point = 2;
    
    // Pre-allocate ring buffer
    std::vector<float> ring_buffer(ring_buffer_points * floats_per_point);
    
    // Generate full data (10x visible for scrolling)
    size_t const total_points = visible_points * 10;
    std::vector<float> full_series = generateSineWaveVertices(total_points, 0);
    
    // Initialize ring buffer with first portion
    std::copy(full_series.begin(), 
              full_series.begin() + static_cast<std::ptrdiff_t>(ring_buffer_points * floats_per_point),
              ring_buffer.begin());
    
    size_t ring_head = 0;  // Index of oldest data in ring
    size_t data_offset = 0;  // Offset into full_series
    size_t bytes_uploaded = 0;
    
    for (auto _ : state) {
        // Scroll: advance by scroll_step points
        data_offset = (data_offset + scroll_step) % (total_points - ring_buffer_points);
        
        // Only upload the NEW points (scroll_step worth of data)
        size_t const new_data_start = data_offset + ring_buffer_points - scroll_step;
        size_t const ring_write_pos = (ring_head + ring_buffer_points - scroll_step) % ring_buffer_points;
        
        // Copy only the new data into the ring buffer
        for (size_t i = 0; i < scroll_step; ++i) {
            size_t const src_idx = (new_data_start + i) * floats_per_point;
            size_t const dst_idx = ((ring_write_pos + i) % ring_buffer_points) * floats_per_point;
            ring_buffer[dst_idx] = full_series[src_idx];
            ring_buffer[dst_idx + 1] = full_series[src_idx + 1];
        }
        
        bytes_uploaded += scroll_step * floats_per_point * sizeof(float);
        ring_head = (ring_head + scroll_step) % ring_buffer_points;
        
        benchmark::DoNotOptimize(ring_buffer.data());
    }
    
    state.SetBytesProcessed(state.iterations() * visible_points * floats_per_point * sizeof(float));
    
    // Report upload efficiency
    double const full_upload = static_cast<double>(visible_points * floats_per_point * sizeof(float));
    double const actual_upload = static_cast<double>(scroll_step * floats_per_point * sizeof(float));
    double const upload_pct = (actual_upload / full_upload) * 100.0;
    state.counters["upload_%"] = benchmark::Counter(upload_pct, benchmark::Counter::kAvgThreads);
}

BENCHMARK(BM_ScrollingRingBuffer)
    ->Args({10000, 10})    // 10K visible, scroll by 10 = 0.1% upload
    ->Args({10000, 100})   // 10K visible, scroll by 100 = 1% upload
    ->Args({100000, 10})   // 100K visible, scroll by 10 = 0.01% upload
    ->Args({100000, 1000}); // 100K visible, scroll by 1000 = 1% upload


// ============================================================================
// Benchmark: Dirty Region Detection Overhead
// ============================================================================

static void BM_DirtyRegionDetection(benchmark::State& state) {
    size_t const point_count = static_cast<size_t>(state.range(0));
    size_t const num_dirty_points = static_cast<size_t>(state.range(1));
    
    // Create two similar buffers
    auto buffer1 = generateSineWaveVertices(point_count, 0);
    auto buffer2 = buffer1;  // Copy
    
    // Modify some points in buffer2 to create dirty regions
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dist(0, point_count - 1);
    for (size_t i = 0; i < num_dirty_points; ++i) {
        size_t idx = dist(gen) * 2 + 1;  // Modify Y values
        buffer2[idx] += 0.1f;
    }
    
    for (auto _ : state) {
        auto regions = findDirtyRegions(buffer1, buffer2);
        benchmark::DoNotOptimize(regions.data());
    }
    
    state.SetBytesProcessed(state.iterations() * buffer1.size() * sizeof(float));
}

BENCHMARK(BM_DirtyRegionDetection)
    ->Args({10000, 10})      // 10K points, 10 dirty
    ->Args({10000, 100})     // 10K points, 100 dirty
    ->Args({10000, 1000})    // 10K points, 1K dirty
    ->Args({100000, 100})    // 100K points, 100 dirty
    ->Args({100000, 10000}); // 100K points, 10K dirty


BENCHMARK_MAIN();
