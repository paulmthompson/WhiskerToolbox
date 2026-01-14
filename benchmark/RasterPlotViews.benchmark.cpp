/**
 * @file RasterPlotViews.benchmark.cpp
 * @brief Benchmarks comparing raw vector vs DigitalEventSeries views for raster plots
 * 
 * This benchmark suite tests the performance tradeoffs between:
 * 1. Baseline: std::vector<std::vector<TimeFrameIndex>> - simple nested vectors
 * 2. View-based: std::vector<shared_ptr<DigitalEventSeries>> with view storage
 * 3. Gather-based: GatherResult<DigitalEventSeries> using the gather() utility
 * 
 * Scenario: Raster plot generation
 * - 100,000 events in the "raster" series (neural spikes, etc.)
 * - 1,000 alignment events (trial starts, stimuli, etc.)
 * - For each alignment event, gather events within a ±window_size window
 * - Populate a mock GPU buffer for rendering
 * 
 * The view-based approach provides abstractions like:
 * - Automatic time frame conversion
 * - Entity tracking for selection/highlighting
 * - Lazy filtering without data copying
 * 
 * However, this comes with potential costs:
 * - shared_ptr allocations per view
 * - Indirection through storage wrapper
 * - Reduced cache locality
 * 
 * Profiling Usage:
 * ----------------
 * # Memory profiling with heaptrack
 * heaptrack ./benchmark_RasterPlotViews
 * heaptrack_gui heaptrack.benchmark_RasterPlotViews.*.gz
 * 
 * # CPU profiling with perf
 * perf record -g ./benchmark_RasterPlotViews --benchmark_filter=FullPipeline
 * perf report
 * 
 * # Cache analysis
 * perf stat -e cache-references,cache-misses,L1-dcache-load-misses ./benchmark_RasterPlotViews
 */

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/GatherResult.hpp"

#include <benchmark/benchmark.h>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

namespace RasterPlotBenchmarks {

// ============================================================================
// Configuration
// ============================================================================

struct RasterBenchmarkConfig {
    size_t raster_event_count = 100'000;  // Events in the raster series
    size_t alignment_event_count = 1'000;  // Number of alignment events (trials)
    int64_t window_half_size = 500;        // Half-window size for gathering events
    int64_t time_range = 1'000'000;        // Total time range
    uint32_t random_seed = 42;
};

// Mock GPU buffer - represents vertices for rendering
struct MockGPUBuffer {
    std::vector<float> x_coords;
    std::vector<float> y_coords;
    
    void clear() {
        x_coords.clear();
        y_coords.clear();
    }
    
    void reserve(size_t n) {
        x_coords.reserve(n);
        y_coords.reserve(n);
    }
    
    void addPoint(float x, float y) {
        x_coords.push_back(x);
        y_coords.push_back(y);
    }
    
    [[nodiscard]] size_t size() const { return x_coords.size(); }
};

// ============================================================================
// Data Generation
// ============================================================================

/**
 * @brief Generate sorted random event times
 */
std::vector<TimeFrameIndex> generateRandomEvents(
    size_t count, int64_t time_range, std::mt19937& rng) 
{
    std::uniform_int_distribution<int64_t> dist(0, time_range - 1);
    
    std::vector<TimeFrameIndex> events;
    events.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        events.emplace_back(dist(rng));
    }
    
    std::ranges::sort(events);
    return events;
}

/**
 * @brief Generate alignment events spread across time range
 */
std::vector<TimeFrameIndex> generateAlignmentEvents(
    size_t count, int64_t time_range, int64_t window_half_size, std::mt19937& rng)
{
    // Ensure alignment events are within valid range for windowing
    int64_t const safe_start = window_half_size;
    int64_t const safe_end = time_range - window_half_size - 1;
    std::uniform_int_distribution<int64_t> dist(safe_start, safe_end);
    
    std::vector<TimeFrameIndex> events;
    events.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        events.emplace_back(dist(rng));
    }
    
    std::ranges::sort(events);
    return events;
}

// ============================================================================
// Baseline Implementation (Raw Vectors)
// ============================================================================

/**
 * @brief Extract events in window using binary search on sorted vector
 */
std::vector<TimeFrameIndex> extractEventsInWindow(
    std::vector<TimeFrameIndex> const& all_events,
    int64_t center,
    int64_t half_window)
{
    int64_t const start = center - half_window;
    int64_t const end = center + half_window;
    
    auto it_start = std::ranges::lower_bound(all_events, TimeFrameIndex{start});
    auto it_end = std::ranges::upper_bound(all_events, TimeFrameIndex{end});
    
    return {it_start, it_end};
}

/**
 * @brief Baseline approach: Build nested vectors and populate GPU buffer
 */
void populateGPUBufferBaseline(
    std::vector<std::vector<TimeFrameIndex>> const& windowed_events,
    std::vector<TimeFrameIndex> const& alignment_events,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total events for reservation
    size_t total_events = 0;
    for (auto const& window : windowed_events) {
        total_events += window.size();
    }
    buffer.reserve(total_events);
    
    // Populate buffer
    for (size_t trial_idx = 0; trial_idx < windowed_events.size(); ++trial_idx) {
        auto const y = static_cast<float>(trial_idx);
        int64_t const center = alignment_events[trial_idx].getValue();
        
        for (auto const& event : windowed_events[trial_idx]) {
            // X is relative to alignment event
            auto const x = static_cast<float>(event.getValue() - center);
            buffer.addPoint(x, y);
        }
    }
}

// ============================================================================
// View-Based Implementation (DigitalEventSeries)
// ============================================================================

/**
 * @brief Create a view of events in a window around a center point
 * 
 * Uses DigitalEventSeries::createView with time range filtering
 */
std::shared_ptr<DigitalEventSeries> createEventWindowView(
    std::shared_ptr<DigitalEventSeries const> source,
    int64_t center,
    int64_t half_window)
{
    TimeFrameIndex const start{center - half_window};
    TimeFrameIndex const end{center + half_window};
    return DigitalEventSeries::createView(std::move(source), start, end);
}

/**
 * @brief View-based approach: Create views and populate GPU buffer
 */
void populateGPUBufferViews(
    std::vector<std::shared_ptr<DigitalEventSeries>> const& windowed_views,
    std::vector<TimeFrameIndex> const& alignment_events,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total events for reservation
    size_t total_events = 0;
    for (auto const& view : windowed_views) {
        total_events += view->size();
    }
    buffer.reserve(total_events);
    
    // Populate buffer
    for (size_t trial_idx = 0; trial_idx < windowed_views.size(); ++trial_idx) {
        auto const y = static_cast<float>(trial_idx);
        int64_t const center = alignment_events[trial_idx].getValue();
        
        for (auto const& event : windowed_views[trial_idx]->view()) {
            auto const x = static_cast<float>(event.time().getValue() - center);
            buffer.addPoint(x, y);
        }
    }
}

// ============================================================================
// Benchmark Fixture
// ============================================================================

class RasterPlotBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        config_ = RasterBenchmarkConfig{};
        std::mt19937 rng(config_.random_seed);
        
        // Generate data
        raster_events_ = generateRandomEvents(
            config_.raster_event_count, config_.time_range, rng);
        alignment_events_ = generateAlignmentEvents(
            config_.alignment_event_count, config_.time_range, 
            config_.window_half_size, rng);
        
        // Create DigitalEventSeries for view-based approach
        raster_series_ = std::make_shared<DigitalEventSeries>(raster_events_);
        
        // Pre-size buffer
        buffer_.reserve(config_.raster_event_count);
    }
    
    void TearDown(benchmark::State const&) override {
        raster_events_.clear();
        alignment_events_.clear();
        windowed_vectors_.clear();
        windowed_views_.clear();
        buffer_.clear();
        raster_series_.reset();
    }
    
    void ReportStats(benchmark::State& state, bool include_buffer = true) const {
        state.counters["raster_events"] = static_cast<double>(config_.raster_event_count);
        state.counters["alignment_events"] = static_cast<double>(config_.alignment_event_count);
        state.counters["window_size"] = static_cast<double>(config_.window_half_size * 2);
        if (include_buffer) {
            state.counters["buffer_points"] = static_cast<double>(buffer_.size());
        }
        
        // Calculate expected events per window
        double const density = static_cast<double>(config_.raster_event_count) / 
                               static_cast<double>(config_.time_range);
        double const expected_per_window = density * 
                                           static_cast<double>(config_.window_half_size * 2);
        state.counters["expected_events_per_window"] = expected_per_window;
    }
    
protected:
    RasterBenchmarkConfig config_;
    std::vector<TimeFrameIndex> raster_events_;
    std::vector<TimeFrameIndex> alignment_events_;
    std::shared_ptr<DigitalEventSeries> raster_series_;
    
    // Storage for benchmark iterations
    std::vector<std::vector<TimeFrameIndex>> windowed_vectors_;
    std::vector<std::shared_ptr<DigitalEventSeries>> windowed_views_;
    MockGPUBuffer buffer_;
};

// ============================================================================
// Individual Phase Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Extract windows into nested vectors (baseline)
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, ExtractWindows_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractEventsInWindow(
                raster_events_, 
                align_event.getValue(), 
                config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_vectors_.data());
    }
    
    ReportStats(state, false);
    
    // Report memory usage estimate
    size_t total_copied_events = 0;
    for (auto const& vec : windowed_vectors_) {
        total_copied_events += vec.size();
    }
    state.counters["copied_events"] = static_cast<double>(total_copied_events);
    state.counters["bytes_copied"] = static_cast<double>(
        total_copied_events * sizeof(TimeFrameIndex));
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, ExtractWindows_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Create views for windows (view-based)
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, CreateViews_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createEventWindowView(
                raster_series_,
                align_event.getValue(),
                config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_views_.data());
    }
    
    ReportStats(state, false);
    state.counters["views_created"] = static_cast<double>(windowed_views_.size());
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, CreateViews_ViewBased)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from nested vectors (baseline)
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, PopulateBuffer_Baseline)(benchmark::State& state) {
    // Pre-create windows
    windowed_vectors_.clear();
    windowed_vectors_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_vectors_.push_back(extractEventsInWindow(
            raster_events_, align_event.getValue(), config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferBaseline(windowed_vectors_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
        benchmark::DoNotOptimize(buffer_.y_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, PopulateBuffer_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from views (view-based)
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, PopulateBuffer_ViewBased)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_views_.push_back(createEventWindowView(
            raster_series_, align_event.getValue(), config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferViews(windowed_views_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
        benchmark::DoNotOptimize(buffer_.y_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, PopulateBuffer_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Full Pipeline Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Complete raster pipeline (baseline)
 * 
 * Full workflow: Extract windows → Populate GPU buffer
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, FullPipeline_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        // Phase 1: Extract windows
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractEventsInWindow(
                raster_events_, align_event.getValue(), config_.window_half_size));
        }
        
        // Phase 2: Populate buffer
        populateGPUBufferBaseline(windowed_vectors_, alignment_events_, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, FullPipeline_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Complete raster pipeline (view-based)
 * 
 * Full workflow: Create views → Populate GPU buffer
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, FullPipeline_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        // Phase 1: Create views
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createEventWindowView(
                raster_series_, align_event.getValue(), config_.window_half_size));
        }
        
        // Phase 2: Populate buffer
        populateGPUBufferViews(windowed_views_, alignment_events_, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, FullPipeline_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Scalability Benchmarks (varying parameters)
// ============================================================================

/**
 * @brief Benchmark full pipeline with varying alignment event counts
 */
class RasterScalabilityBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        config_.alignment_event_count = static_cast<size_t>(state.range(0));
        std::mt19937 rng(config_.random_seed);
        
        raster_events_ = generateRandomEvents(
            config_.raster_event_count, config_.time_range, rng);
        alignment_events_ = generateAlignmentEvents(
            config_.alignment_event_count, config_.time_range,
            config_.window_half_size, rng);
        raster_series_ = std::make_shared<DigitalEventSeries>(raster_events_);
    }

protected:
    RasterBenchmarkConfig config_;
    std::vector<TimeFrameIndex> raster_events_;
    std::vector<TimeFrameIndex> alignment_events_;
    std::shared_ptr<DigitalEventSeries> raster_series_;
    std::vector<std::vector<TimeFrameIndex>> windowed_vectors_;
    std::vector<std::shared_ptr<DigitalEventSeries>> windowed_views_;
    MockGPUBuffer buffer_;
};

BENCHMARK_DEFINE_F(RasterScalabilityBenchmark, ScaleAlignments_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractEventsInWindow(
                raster_events_, align_event.getValue(), config_.window_half_size));
        }
        populateGPUBufferBaseline(windowed_vectors_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_events_.size());
}

BENCHMARK_DEFINE_F(RasterScalabilityBenchmark, ScaleAlignments_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createEventWindowView(
                raster_series_, align_event.getValue(), config_.window_half_size));
        }
        populateGPUBufferViews(windowed_views_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_events_.size());
}

BENCHMARK_REGISTER_F(RasterScalabilityBenchmark, ScaleAlignments_Baseline)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(5000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(RasterScalabilityBenchmark, ScaleAlignments_ViewBased)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(5000)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Memory Allocation Comparison
// ============================================================================

/**
 * @brief Benchmark: Focus on allocation overhead
 * 
 * Only creates the windows/views, doesn't iterate them.
 * Shows allocation cost difference more clearly.
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, AllocationOnly_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractEventsInWindow(
                raster_events_, align_event.getValue(), config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_vectors_.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_DEFINE_F(RasterPlotBenchmark, AllocationOnly_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createEventWindowView(
                raster_series_, align_event.getValue(), config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_views_.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, AllocationOnly_Baseline)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(RasterPlotBenchmark, AllocationOnly_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Iteration-Only Comparison  
// ============================================================================

/**
 * @brief Benchmark: Focus on iteration overhead
 * 
 * Pre-allocate windows/views, then only benchmark iteration.
 * Shows cache locality and indirection cost.
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, IterationOnly_Baseline)(benchmark::State& state) {
    // Pre-create windows
    windowed_vectors_.clear();
    windowed_vectors_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_vectors_.push_back(extractEventsInWindow(
            raster_events_, align_event.getValue(), config_.window_half_size));
    }
    
    int64_t sum = 0;
    for (auto _ : state) {
        sum = 0;
        for (size_t i = 0; i < windowed_vectors_.size(); ++i) {
            for (auto const& event : windowed_vectors_[i]) {
                sum += event.getValue();
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_DEFINE_F(RasterPlotBenchmark, IterationOnly_ViewBased)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_views_.push_back(createEventWindowView(
            raster_series_, align_event.getValue(), config_.window_half_size));
    }
    
    int64_t sum = 0;
    for (auto _ : state) {
        sum = 0;
        for (size_t i = 0; i < windowed_views_.size(); ++i) {
            for (auto const& event : windowed_views_[i]->view()) {
                sum += event.time().getValue();
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, IterationOnly_Baseline)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(RasterPlotBenchmark, IterationOnly_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// GatherResult-Based Benchmarks
// ============================================================================

/**
 * @brief Helper to create alignment intervals from alignment events
 * 
 * Converts alignment events + window size into a DigitalIntervalSeries
 * suitable for use with gather().
 */
std::shared_ptr<DigitalIntervalSeries> createAlignmentIntervals(
    std::vector<TimeFrameIndex> const& alignment_events,
    int64_t half_window)
{
    std::vector<Interval> intervals;
    intervals.reserve(alignment_events.size());
    
    for (auto const& event : alignment_events) {
        int64_t const center = event.getValue();
        intervals.push_back(Interval{center - half_window, center + half_window});
    }
    
    return std::make_shared<DigitalIntervalSeries>(std::move(intervals));
}

/**
 * @brief Populate GPU buffer from GatherResult
 */
void populateGPUBufferGather(
    GatherResult<DigitalEventSeries> const& gathered,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total events for reservation
    size_t total_events = 0;
    for (auto const& view : gathered) {
        total_events += view->size();
    }
    buffer.reserve(total_events);
    
    // Populate buffer using transformWithInterval
    for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
        auto const y = static_cast<float>(trial_idx);
        Interval const interval = gathered.intervalAt(trial_idx);
        int64_t const center = (interval.start + interval.end) / 2;
        
        for (auto const& event : gathered[trial_idx]->view()) {
            auto const x = static_cast<float>(event.time().getValue() - center);
            buffer.addPoint(x, y);
        }
    }
}

/**
 * @brief Benchmark: Create views using gather() function
 * 
 * Uses the GatherResult utility to create all views at once.
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, CreateViews_Gather)(benchmark::State& state) {
    // Create alignment intervals once (not part of benchmark)
    auto alignment_intervals = createAlignmentIntervals(
        alignment_events_, config_.window_half_size);
    
    for (auto _ : state) {
        auto gathered = gather(raster_series_, alignment_intervals);
        
        benchmark::DoNotOptimize(gathered.source());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, CreateViews_Gather)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from GatherResult
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, PopulateBuffer_Gather)(benchmark::State& state) {
    // Pre-create gather result
    auto alignment_intervals = createAlignmentIntervals(
        alignment_events_, config_.window_half_size);
    auto gathered = gather(raster_series_, alignment_intervals);
    
    for (auto _ : state) {
        populateGPUBufferGather(gathered, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
        benchmark::DoNotOptimize(buffer_.y_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, PopulateBuffer_Gather)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Complete raster pipeline using gather()
 * 
 * Full workflow: gather() → Populate GPU buffer
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, FullPipeline_Gather)(benchmark::State& state) {
    // Create alignment intervals once (this would typically be done once at setup)
    auto alignment_intervals = createAlignmentIntervals(
        alignment_events_, config_.window_half_size);
    
    for (auto _ : state) {
        // Phase 1: Gather views
        auto gathered = gather(raster_series_, alignment_intervals);
        
        // Phase 2: Populate buffer
        populateGPUBufferGather(gathered, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, FullPipeline_Gather)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Iteration only using GatherResult
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, IterationOnly_Gather)(benchmark::State& state) {
    // Pre-create gather result
    auto alignment_intervals = createAlignmentIntervals(
        alignment_events_, config_.window_half_size);
    auto gathered = gather(raster_series_, alignment_intervals);
    
    int64_t sum = 0;
    for (auto _ : state) {
        sum = 0;
        for (auto const& view : gathered) {
            for (auto const& event : view->view()) {
                sum += event.time().getValue();
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, IterationOnly_Gather)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Using GatherResult::transform() for analysis
 * 
 * Shows how transform() can be used to compute per-trial statistics.
 */
BENCHMARK_DEFINE_F(RasterPlotBenchmark, Transform_Gather)(benchmark::State& state) {
    // Pre-create gather result
    auto alignment_intervals = createAlignmentIntervals(
        alignment_events_, config_.window_half_size);
    auto gathered = gather(raster_series_, alignment_intervals);
    
    for (auto _ : state) {
        auto counts = gathered.transform([](auto const& view) {
            return view->size();
        });
        
        benchmark::DoNotOptimize(counts.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
    state.counters["num_trials"] = static_cast<double>(gathered.size());
}

BENCHMARK_REGISTER_F(RasterPlotBenchmark, Transform_Gather)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Scalability Benchmarks for Gather
// ============================================================================

BENCHMARK_DEFINE_F(RasterScalabilityBenchmark, ScaleAlignments_Gather)(benchmark::State& state) {
    auto alignment_intervals = createAlignmentIntervals(
        alignment_events_, config_.window_half_size);
    
    for (auto _ : state) {
        auto gathered = gather(raster_series_, alignment_intervals);
        populateGPUBufferGather(gathered, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_events_.size());
}

BENCHMARK_REGISTER_F(RasterScalabilityBenchmark, ScaleAlignments_Gather)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(5000)
    ->Unit(benchmark::kMicrosecond);

}  // namespace RasterPlotBenchmarks

BENCHMARK_MAIN();
