/**
 * @file IntervalPlotViews.benchmark.cpp
 * @brief Benchmarks comparing raw vector vs DigitalIntervalSeries views for interval plots
 * 
 * This benchmark suite tests the performance tradeoffs between:
 * 1. Baseline: std::vector<std::vector<Interval>> - simple nested vectors
 * 2. View-based: std::vector<shared_ptr<DigitalIntervalSeries>> with view storage
 * 
 * Scenario: Interval plot generation (similar to peri-event histograms for intervals)
 * - 100,000 intervals in the source series (e.g., behavioral epochs, neural states)
 * - 1,000 alignment events (trial starts, stimuli, etc.)
 * - For each alignment event, gather intervals overlapping a ±window_size window
 * - Populate a mock GPU buffer for rendering interval bars
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
 * heaptrack ./benchmark_IntervalPlotViews
 * heaptrack_gui heaptrack.benchmark_IntervalPlotViews.*.gz
 * 
 * # CPU profiling with perf
 * perf record -g ./benchmark_IntervalPlotViews --benchmark_filter=FullPipeline
 * perf report
 * 
 * # Cache analysis
 * perf stat -e cache-references,cache-misses,L1-dcache-load-misses ./benchmark_IntervalPlotViews
 */

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <benchmark/benchmark.h>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

namespace IntervalPlotBenchmarks {

// ============================================================================
// Configuration
// ============================================================================

struct IntervalBenchmarkConfig {
    size_t interval_count = 100'000;       // Intervals in the source series
    size_t alignment_event_count = 1'000;  // Number of alignment events (trials)
    int64_t window_half_size = 500;        // Half-window size for gathering intervals
    int64_t time_range = 1'000'000;        // Total time range
    int64_t min_interval_length = 5;       // Minimum interval duration
    int64_t max_interval_length = 50;      // Maximum interval duration
    uint32_t random_seed = 42;
};

// Mock GPU buffer - represents vertices for rendering interval bars
struct MockGPUBuffer {
    std::vector<float> x_start;   // Start position of each interval bar
    std::vector<float> x_end;     // End position of each interval bar
    std::vector<float> y_coords;  // Row (trial) index
    
    void clear() {
        x_start.clear();
        x_end.clear();
        y_coords.clear();
    }
    
    void reserve(size_t n) {
        x_start.reserve(n);
        x_end.reserve(n);
        y_coords.reserve(n);
    }
    
    void addBar(float start, float end, float y) {
        x_start.push_back(start);
        x_end.push_back(end);
        y_coords.push_back(y);
    }
    
    [[nodiscard]] size_t size() const { return x_start.size(); }
};

// ============================================================================
// Data Generation
// ============================================================================

/**
 * @brief Generate sorted random intervals
 */
std::vector<Interval> generateRandomIntervals(
    size_t count, int64_t time_range, 
    int64_t min_len, int64_t max_len,
    std::mt19937& rng) 
{
    std::uniform_int_distribution<int64_t> start_dist(0, time_range - max_len - 1);
    std::uniform_int_distribution<int64_t> len_dist(min_len, max_len);
    
    std::vector<Interval> intervals;
    intervals.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        int64_t const start = start_dist(rng);
        int64_t const length = len_dist(rng);
        intervals.emplace_back(Interval{start, start + length});
    }
    
    // Sort by start time
    std::ranges::sort(intervals, [](Interval const& a, Interval const& b) {
        return a.start < b.start;
    });
    
    return intervals;
}

/**
 * @brief Generate alignment events spread across time range
 */
std::vector<int64_t> generateAlignmentEvents(
    size_t count, int64_t time_range, int64_t window_half_size, std::mt19937& rng)
{
    // Ensure alignment events are within valid range for windowing
    int64_t const safe_start = window_half_size;
    int64_t const safe_end = time_range - window_half_size - 1;
    std::uniform_int_distribution<int64_t> dist(safe_start, safe_end);
    
    std::vector<int64_t> events;
    events.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        events.push_back(dist(rng));
    }
    
    std::ranges::sort(events);
    return events;
}

// ============================================================================
// Baseline Implementation (Raw Vectors)
// ============================================================================

/**
 * @brief Extract intervals overlapping window using binary search on sorted vector
 */
std::vector<Interval> extractIntervalsInWindow(
    std::vector<Interval> const& all_intervals,
    int64_t center,
    int64_t half_window)
{
    int64_t const window_start = center - half_window;
    int64_t const window_end = center + half_window;
    
    // Find first interval that might overlap (start <= window_end)
    auto it_start = std::ranges::lower_bound(all_intervals, window_start,
        {}, [](Interval const& i) { return i.start; });
    
    // Scan backwards to catch intervals that started before but extend into window
    while (it_start != all_intervals.begin()) {
        --it_start;
        if (it_start->end < window_start) {
            ++it_start;
            break;
        }
    }
    
    std::vector<Interval> result;
    for (auto it = it_start; it != all_intervals.end() && it->start <= window_end; ++it) {
        // Check if interval actually overlaps with window
        if (it->end >= window_start && it->start <= window_end) {
            result.push_back(*it);
        }
    }
    
    return result;
}

/**
 * @brief Baseline approach: Build nested vectors and populate GPU buffer
 */
void populateGPUBufferBaseline(
    std::vector<std::vector<Interval>> const& windowed_intervals,
    std::vector<int64_t> const& alignment_events,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total intervals for reservation
    size_t total_intervals = 0;
    for (auto const& window : windowed_intervals) {
        total_intervals += window.size();
    }
    buffer.reserve(total_intervals);
    
    // Populate buffer
    for (size_t trial_idx = 0; trial_idx < windowed_intervals.size(); ++trial_idx) {
        auto const y = static_cast<float>(trial_idx);
        int64_t const center = alignment_events[trial_idx];
        
        for (auto const& interval : windowed_intervals[trial_idx]) {
            // X coordinates are relative to alignment event
            auto const x_start = static_cast<float>(interval.start - center);
            auto const x_end = static_cast<float>(interval.end - center);
            buffer.addBar(x_start, x_end, y);
        }
    }
}

// ============================================================================
// View-Based Implementation (DigitalIntervalSeries)
// ============================================================================

/**
 * @brief Create a view of intervals overlapping a window around a center point
 * 
 * Uses DigitalIntervalSeries::createView with time range filtering
 */
std::shared_ptr<DigitalIntervalSeries> createIntervalWindowView(
    std::shared_ptr<DigitalIntervalSeries const> source,
    int64_t center,
    int64_t half_window)
{
    int64_t const start = center - half_window;
    int64_t const end = center + half_window;
    return DigitalIntervalSeries::createView(std::move(source), start, end);
}

/**
 * @brief View-based approach: Create views and populate GPU buffer
 */
void populateGPUBufferViews(
    std::vector<std::shared_ptr<DigitalIntervalSeries>> const& windowed_views,
    std::vector<int64_t> const& alignment_events,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total intervals for reservation
    size_t total_intervals = 0;
    for (auto const& view : windowed_views) {
        total_intervals += view->size();
    }
    buffer.reserve(total_intervals);
    
    // Populate buffer
    for (size_t trial_idx = 0; trial_idx < windowed_views.size(); ++trial_idx) {
        auto const y = static_cast<float>(trial_idx);
        int64_t const center = alignment_events[trial_idx];
        
        for (auto const& interval_with_id : windowed_views[trial_idx]->view()) {
            auto const& interval = interval_with_id.interval;
            auto const x_start = static_cast<float>(interval.start - center);
            auto const x_end = static_cast<float>(interval.end - center);
            buffer.addBar(x_start, x_end, y);
        }
    }
}

// ============================================================================
// Benchmark Fixture
// ============================================================================

class IntervalPlotBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        config_ = IntervalBenchmarkConfig{};
        std::mt19937 rng(config_.random_seed);
        
        // Generate data
        source_intervals_ = generateRandomIntervals(
            config_.interval_count, config_.time_range,
            config_.min_interval_length, config_.max_interval_length, rng);
        alignment_events_ = generateAlignmentEvents(
            config_.alignment_event_count, config_.time_range, 
            config_.window_half_size, rng);
        
        // Create DigitalIntervalSeries for view-based approach
        interval_series_ = std::make_shared<DigitalIntervalSeries>(source_intervals_);
        
        // Pre-size buffer
        buffer_.reserve(config_.interval_count);
    }
    
    void TearDown(benchmark::State const&) override {
        source_intervals_.clear();
        alignment_events_.clear();
        windowed_vectors_.clear();
        windowed_views_.clear();
        buffer_.clear();
        interval_series_.reset();
    }
    
    void ReportStats(benchmark::State& state, bool include_buffer = true) const {
        state.counters["source_intervals"] = static_cast<double>(config_.interval_count);
        state.counters["alignment_events"] = static_cast<double>(config_.alignment_event_count);
        state.counters["window_size"] = static_cast<double>(config_.window_half_size * 2);
        if (include_buffer) {
            state.counters["buffer_bars"] = static_cast<double>(buffer_.size());
        }
        
        // Calculate expected intervals per window
        double const density = static_cast<double>(config_.interval_count) / 
                               static_cast<double>(config_.time_range);
        double const avg_interval_len = static_cast<double>(
            config_.min_interval_length + config_.max_interval_length) / 2.0;
        double const expected_per_window = density * 
            (static_cast<double>(config_.window_half_size * 2) + avg_interval_len);
        state.counters["expected_intervals_per_window"] = expected_per_window;
    }
    
protected:
    IntervalBenchmarkConfig config_;
    std::vector<Interval> source_intervals_;
    std::vector<int64_t> alignment_events_;
    std::shared_ptr<DigitalIntervalSeries> interval_series_;
    
    // Storage for benchmark iterations
    std::vector<std::vector<Interval>> windowed_vectors_;
    std::vector<std::shared_ptr<DigitalIntervalSeries>> windowed_views_;
    MockGPUBuffer buffer_;
};

// ============================================================================
// Individual Phase Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Extract windows into nested vectors (baseline)
 */
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, ExtractWindows_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractIntervalsInWindow(
                source_intervals_, 
                align_event, 
                config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_vectors_.data());
    }
    
    ReportStats(state, false);
    
    // Report memory usage estimate
    size_t total_copied_intervals = 0;
    for (auto const& vec : windowed_vectors_) {
        total_copied_intervals += vec.size();
    }
    state.counters["copied_intervals"] = static_cast<double>(total_copied_intervals);
    state.counters["bytes_copied"] = static_cast<double>(
        total_copied_intervals * sizeof(Interval));
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, ExtractWindows_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Create views for windows (view-based)
 */
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, CreateViews_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createIntervalWindowView(
                interval_series_,
                align_event,
                config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_views_.data());
    }
    
    ReportStats(state, false);
    state.counters["views_created"] = static_cast<double>(windowed_views_.size());
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, CreateViews_ViewBased)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from nested vectors (baseline)
 */
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, PopulateBuffer_Baseline)(benchmark::State& state) {
    // Pre-create windows
    windowed_vectors_.clear();
    windowed_vectors_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_vectors_.push_back(extractIntervalsInWindow(
            source_intervals_, align_event, config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferBaseline(windowed_vectors_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_start.data());
        benchmark::DoNotOptimize(buffer_.x_end.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, PopulateBuffer_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from views (view-based)
 */
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, PopulateBuffer_ViewBased)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_views_.push_back(createIntervalWindowView(
            interval_series_, align_event, config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferViews(windowed_views_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_start.data());
        benchmark::DoNotOptimize(buffer_.x_end.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, PopulateBuffer_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Full Pipeline Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Complete interval pipeline (baseline)
 * 
 * Full workflow: Extract windows → Populate GPU buffer
 */
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, FullPipeline_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        // Phase 1: Extract windows
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractIntervalsInWindow(
                source_intervals_, align_event, config_.window_half_size));
        }
        
        // Phase 2: Populate buffer
        populateGPUBufferBaseline(windowed_vectors_, alignment_events_, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_start.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, FullPipeline_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Complete interval pipeline (view-based)
 * 
 * Full workflow: Create views → Populate GPU buffer
 */
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, FullPipeline_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        // Phase 1: Create views
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createIntervalWindowView(
                interval_series_, align_event, config_.window_half_size));
        }
        
        // Phase 2: Populate buffer
        populateGPUBufferViews(windowed_views_, alignment_events_, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_start.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, FullPipeline_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Scalability Benchmarks (varying parameters)
// ============================================================================

/**
 * @brief Benchmark full pipeline with varying alignment event counts
 */
class IntervalScalabilityBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        config_.alignment_event_count = static_cast<size_t>(state.range(0));
        std::mt19937 rng(config_.random_seed);
        
        source_intervals_ = generateRandomIntervals(
            config_.interval_count, config_.time_range,
            config_.min_interval_length, config_.max_interval_length, rng);
        alignment_events_ = generateAlignmentEvents(
            config_.alignment_event_count, config_.time_range,
            config_.window_half_size, rng);
        interval_series_ = std::make_shared<DigitalIntervalSeries>(source_intervals_);
    }

protected:
    IntervalBenchmarkConfig config_;
    std::vector<Interval> source_intervals_;
    std::vector<int64_t> alignment_events_;
    std::shared_ptr<DigitalIntervalSeries> interval_series_;
    std::vector<std::vector<Interval>> windowed_vectors_;
    std::vector<std::shared_ptr<DigitalIntervalSeries>> windowed_views_;
    MockGPUBuffer buffer_;
};

BENCHMARK_DEFINE_F(IntervalScalabilityBenchmark, ScaleAlignments_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractIntervalsInWindow(
                source_intervals_, align_event, config_.window_half_size));
        }
        populateGPUBufferBaseline(windowed_vectors_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_start.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_events_.size());
}

BENCHMARK_DEFINE_F(IntervalScalabilityBenchmark, ScaleAlignments_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createIntervalWindowView(
                interval_series_, align_event, config_.window_half_size));
        }
        populateGPUBufferViews(windowed_views_, alignment_events_, buffer_);
        benchmark::DoNotOptimize(buffer_.x_start.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_events_.size());
}

BENCHMARK_REGISTER_F(IntervalScalabilityBenchmark, ScaleAlignments_Baseline)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(5000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(IntervalScalabilityBenchmark, ScaleAlignments_ViewBased)
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
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, AllocationOnly_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_vectors_.push_back(extractIntervalsInWindow(
                source_intervals_, align_event, config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_vectors_.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_DEFINE_F(IntervalPlotBenchmark, AllocationOnly_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_events_.size());
        
        for (auto const& align_event : alignment_events_) {
            windowed_views_.push_back(createIntervalWindowView(
                interval_series_, align_event, config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_views_.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, AllocationOnly_Baseline)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, AllocationOnly_ViewBased)
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
BENCHMARK_DEFINE_F(IntervalPlotBenchmark, IterationOnly_Baseline)(benchmark::State& state) {
    // Pre-create windows
    windowed_vectors_.clear();
    windowed_vectors_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_vectors_.push_back(extractIntervalsInWindow(
            source_intervals_, align_event, config_.window_half_size));
    }
    
    int64_t sum = 0;
    for (auto _ : state) {
        sum = 0;
        for (size_t i = 0; i < windowed_vectors_.size(); ++i) {
            for (auto const& interval : windowed_vectors_[i]) {
                sum += interval.start + interval.end;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_DEFINE_F(IntervalPlotBenchmark, IterationOnly_ViewBased)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_events_.size());
    for (auto const& align_event : alignment_events_) {
        windowed_views_.push_back(createIntervalWindowView(
            interval_series_, align_event, config_.window_half_size));
    }
    
    int64_t sum = 0;
    for (auto _ : state) {
        sum = 0;
        for (size_t i = 0; i < windowed_views_.size(); ++i) {
            for (auto const& interval_with_id : windowed_views_[i]->view()) {
                auto const& interval = interval_with_id.interval;
                sum += interval.start + interval.end;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, IterationOnly_Baseline)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(IntervalPlotBenchmark, IterationOnly_ViewBased)
    ->Unit(benchmark::kMicrosecond);

}  // namespace IntervalPlotBenchmarks

BENCHMARK_MAIN();
