/**
 * @file bm_rendering_memory_profile.cpp
 * @brief Standalone executable for heap memory profiling with heaptrack.
 *
 * NOT a Google Benchmark executable. This is a standalone main() that
 * simulates realistic rendering workloads so heaptrack can measure:
 *   - Peak memory usage
 *   - Allocation frequency per frame
 *   - Allocation hotspots (which code path allocates the most)
 *
 * Usage:
 *   # 32-channel scenario:
 *   heaptrack ./out/build/Clang/Release/bin/adhoc/bm_rendering_memory_profile
 *   heaptrack_print heaptrack.bm_rendering_memory_profile.*.gz
 *
 * The program runs two scenarios:
 *   Scenario A: 32 channels, 100 scroll frames (simulating interactive browsing)
 *   Scenario B: 384 channels, 10 scroll frames (peak memory stress test)
 *
 * Each scenario compares:
 *   Method 1: Current pipeline (allocate per frame, no decimation)
 *   Method 2: Decimated with per-pixel buffer (allocate once)
 */

#include "MinMaxDecimator.hpp"
#include "SyntheticDataFactory.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using namespace BenchmarkSynthetic;
using namespace MinMaxDecimator;

namespace {

struct ScenarioConfig {
    std::size_t num_channels;
    std::size_t num_samples;
    std::size_t window_size;
    std::size_t canvas_width;
    std::size_t scroll_frames;
    std::size_t scroll_step;
};

// ============================================================================
// Method 1: Current pipeline — allocate vertex vectors each frame
// ============================================================================
void runCurrentPipeline(std::vector<std::span<float const>> const & spans,
                        ScenarioConfig const & cfg) {
    std::cout << "  Method 1: Current pipeline (alloc per frame, no decimation)\n";
    auto const start = std::chrono::high_resolution_clock::now();

    std::size_t total_vertices = 0;
    for (std::size_t frame = 0; frame < cfg.scroll_frames; ++frame) {
        auto const offset = frame * cfg.scroll_step;
        auto const window_end = std::min(offset + cfg.window_size, cfg.num_samples);
        auto const actual_window = window_end - offset;

        for (std::size_t ch = 0; ch < cfg.num_channels; ++ch) {
            // Allocate new vector every frame (this is what the current code does)
            std::vector<float> vertices;
            vertices.reserve(actual_window * 2);
            for (std::size_t i = offset; i < window_end; ++i) {
                vertices.push_back(static_cast<float>(i));
                vertices.push_back(spans[ch][i]);
            }
            total_vertices += vertices.size() / 2;
            // vector destroyed here — heap allocation freed
        }
    }

    auto const elapsed = std::chrono::high_resolution_clock::now() - start;
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "    Total vertices: " << total_vertices << "\n";
    std::cout << "    Time: " << ms << " ms\n";
}

// ============================================================================
// Method 2: Decimated with per-pixel buffer — allocate once
// ============================================================================
void runDecimatedPipeline(std::vector<std::span<float const>> const & spans,
                          ScenarioConfig const & cfg) {
    std::cout << "  Method 2: Decimated + per-pixel buffer (alloc once)\n";
    auto const start = std::chrono::high_resolution_clock::now();

    // Pre-allocate all buffers once
    std::vector<std::vector<float>> pixel_buffers(cfg.num_channels,
                                                  std::vector<float>(cfg.canvas_width * 4));

    std::size_t total_vertices = 0;
    for (std::size_t frame = 0; frame < cfg.scroll_frames; ++frame) {
        auto const offset = frame * cfg.scroll_step;
        auto const window_end = std::min(offset + cfg.window_size, cfg.num_samples);

        for (std::size_t ch = 0; ch < cfg.num_channels; ++ch) {
            auto window_span = spans[ch].subspan(offset, window_end - offset);
            auto written = decimateSpanInPlace(window_span, cfg.canvas_width, pixel_buffers[ch]);
            total_vertices += written / 2;
        }
    }

    auto const elapsed = std::chrono::high_resolution_clock::now() - start;
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "    Total vertices: " << total_vertices << "\n";
    std::cout << "    Time: " << ms << " ms\n";
}

void runScenario(char const * name, ScenarioConfig const & cfg) {
    std::cout << "\n=== " << name << " ===\n";
    std::cout << "  Channels: " << cfg.num_channels << "\n";
    std::cout << "  Total samples: " << cfg.num_samples << "\n";
    std::cout << "  Window size: " << cfg.window_size << "\n";
    std::cout << "  Canvas width: " << cfg.canvas_width << "\n";
    std::cout << "  Scroll frames: " << cfg.scroll_frames << "\n";
    std::cout << "  Scroll step: " << cfg.scroll_step << " samples\n\n";

    auto result = createTensorColumnSeries(cfg.num_channels, cfg.num_samples);

    // Collect spans
    std::vector<std::span<float const>> spans;
    spans.reserve(cfg.num_channels);
    for (auto const & ch: result.channels) {
        spans.push_back(ch->getDataInTimeFrameIndexRange(
                TimeFrameIndex(0),
                TimeFrameIndex(static_cast<int64_t>(cfg.num_samples - 1))));
    }

    runCurrentPipeline(spans, cfg);
    runDecimatedPipeline(spans, cfg);
}

}// namespace

int main() {
    std::cout << "Rendering Memory Profile Benchmark\n";
    std::cout << "===================================\n";
    std::cout << "Run with heaptrack for allocation analysis.\n";

    // Scenario A: 32 channels, interactive scrolling
    runScenario("Scenario A: 32ch Interactive", ScenarioConfig{
                                                        .num_channels = 32,
                                                        .num_samples = 1'000'000,// 1M samples total
                                                        .window_size = 100'000,  // 100K visible
                                                        .canvas_width = 1920,
                                                        .scroll_frames = 100,// 100 scroll events
                                                        .scroll_step = 1000  // scroll 1000 samples per frame
                                                });

    // Scenario B: 384 channels, peak memory stress
    runScenario("Scenario B: 384ch Stress", ScenarioConfig{
                                                    .num_channels = 384,
                                                    .num_samples = 300'000,// 300K samples total
                                                    .window_size = 100'000,// 100K visible
                                                    .canvas_width = 1920,
                                                    .scroll_frames = 10,// fewer frames (already heavy)
                                                    .scroll_step = 10000});

    // Scenario C: 384 channels, small window (zoomed in)
    runScenario("Scenario C: 384ch Zoomed", ScenarioConfig{
                                                    .num_channels = 384,
                                                    .num_samples = 300'000,
                                                    .window_size = 10'000,// small window
                                                    .canvas_width = 1920,
                                                    .scroll_frames = 50,
                                                    .scroll_step = 200});

    std::cout << "\nDone.\n";
    return 0;
}
