/**
 * @file LinePlotViews.benchmark.cpp
 * @brief Benchmarks comparing raw vector vs AnalogTimeSeries views for line plots
 * 
 * This benchmark suite tests the performance tradeoffs between:
 * 1. Baseline: std::vector<std::vector<float>> - simple nested vectors
 * 2. View-based: std::vector<shared_ptr<AnalogTimeSeries>> with view storage
 * 
 * Scenario: Line plot generation (e.g., multi-trial LFP visualization)
 * - 1,000,000 samples in the "full" time series
 * - 1,000 alignment events (trial starts)
 * - For each alignment event, extract a window of samples around the alignment
 * - Populate a mock GPU buffer for rendering
 * 
 * The view-based approach provides abstractions like:
 * - Automatic time frame conversion
 * - Lazy filtering without data copying
 * - Integration with transform pipelines
 * 
 * However, this comes with potential costs:
 * - shared_ptr allocations per view
 * - Indirection through storage wrapper
 * - Reduced cache locality (for non-contiguous operations)
 * 
 * Note: AnalogTimeSeries views ARE contiguous (ViewAnalogDataStorage provides
 * direct span access), so we expect much better performance than digital event
 * views which require index indirection.
 * 
 * Profiling Usage:
 * ----------------
 * # Memory profiling with heaptrack
 * heaptrack ./benchmark_LinePlotViews
 * heaptrack_gui heaptrack.benchmark_LinePlotViews.*.gz
 * 
 * # CPU profiling with perf
 * perf record -g ./benchmark_LinePlotViews --benchmark_filter=FullPipeline
 * perf report
 * 
 * # Cache analysis
 * perf stat -e cache-references,cache-misses,L1-dcache-load-misses ./benchmark_LinePlotViews
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <benchmark/benchmark.h>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

namespace LinePlotBenchmarks {

// ============================================================================
// Configuration
// ============================================================================

struct LineBenchmarkConfig {
    size_t total_samples = 1'000'000;      // Total samples in full series
    size_t alignment_count = 1'000;        // Number of alignment events (trials)
    int64_t window_half_size = 500;        // Half-window size (samples per trial = 2x this)
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
 * @brief Generate synthetic analog signal (random walk)
 */
std::vector<float> generateAnalogSignal(size_t count, std::mt19937& rng) {
    std::normal_distribution<float> dist(0.0f, 0.1f);
    
    std::vector<float> signal;
    signal.reserve(count);
    
    float value = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        value += dist(rng);
        signal.push_back(value);
    }
    
    return signal;
}

/**
 * @brief Generate time indices (0, 1, 2, ...)
 */
std::vector<TimeFrameIndex> generateTimeIndices(size_t count) {
    std::vector<TimeFrameIndex> times;
    times.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        times.emplace_back(static_cast<int64_t>(i));
    }
    return times;
}

/**
 * @brief Generate alignment events spread across time range
 */
std::vector<int64_t> generateAlignmentTimes(
    size_t count, int64_t total_samples, int64_t window_half_size, std::mt19937& rng)
{
    // Ensure alignment events are within valid range for windowing
    int64_t const safe_start = window_half_size;
    int64_t const safe_end = total_samples - window_half_size - 1;
    std::uniform_int_distribution<int64_t> dist(safe_start, safe_end);
    
    std::vector<int64_t> times;
    times.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        times.push_back(dist(rng));
    }
    
    std::ranges::sort(times);
    return times;
}

// ============================================================================
// Baseline Implementation (Raw Vectors)
// ============================================================================

/**
 * @brief Extract window into a new vector using raw indices
 */
std::vector<float> extractWindow(
    std::vector<float> const& full_signal,
    int64_t center,
    int64_t half_window)
{
    size_t const start = static_cast<size_t>(center - half_window);
    size_t const end = static_cast<size_t>(center + half_window);
    
    return {full_signal.begin() + static_cast<ptrdiff_t>(start), 
            full_signal.begin() + static_cast<ptrdiff_t>(end)};
}

/**
 * @brief Baseline approach: Build nested vectors and populate GPU buffer
 */
void populateGPUBufferBaseline(
    std::vector<std::vector<float>> const& windowed_signals,
    std::vector<int64_t> const& alignment_times,
    int64_t window_half_size,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total points for reservation
    size_t total_points = 0;
    for (auto const& window : windowed_signals) {
        total_points += window.size();
    }
    buffer.reserve(total_points);
    
    // Populate buffer
    for (size_t trial_idx = 0; trial_idx < windowed_signals.size(); ++trial_idx) {
        auto const& window = windowed_signals[trial_idx];
        int64_t const center = alignment_times[trial_idx];
        
        for (size_t i = 0; i < window.size(); ++i) {
            // X is relative time, Y is value (for multi-line overlay)
            // Or could use trial_idx for y-offset
            auto const x = static_cast<float>(static_cast<int64_t>(i) - window_half_size);
            buffer.addPoint(x, window[i]);
        }
    }
}

// ============================================================================
// View-Based Implementation (AnalogTimeSeries)
// ============================================================================

/**
 * @brief Create a view of samples in a window around a center point
 * 
 * Uses AnalogTimeSeries::createView with time range filtering
 */
std::shared_ptr<AnalogTimeSeries> createSignalWindowView(
    std::shared_ptr<AnalogTimeSeries const> source,
    int64_t center,
    int64_t half_window)
{
    TimeFrameIndex const start{center - half_window};
    TimeFrameIndex const end{center + half_window - 1};  // exclusive end
    return AnalogTimeSeries::createView(std::move(source), start, end);
}

/**
 * @brief View-based approach: Create views and populate GPU buffer
 */
void populateGPUBufferViews(
    std::vector<std::shared_ptr<AnalogTimeSeries>> const& windowed_views,
    std::vector<int64_t> const& alignment_times,
    int64_t window_half_size,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    // Estimate total points for reservation
    size_t total_points = 0;
    for (auto const& view : windowed_views) {
        total_points += view->getNumSamples();
    }
    buffer.reserve(total_points);
    
    // Populate buffer - using span access (contiguous!)
    for (size_t trial_idx = 0; trial_idx < windowed_views.size(); ++trial_idx) {
        auto const& view = windowed_views[trial_idx];
        auto span = view->getAnalogTimeSeries();
        
        for (size_t i = 0; i < span.size(); ++i) {
            auto const x = static_cast<float>(static_cast<int64_t>(i) - window_half_size);
            buffer.addPoint(x, span[i]);
        }
    }
}

/**
 * @brief View-based approach using iterator (for comparison)
 */
void populateGPUBufferViewsIterator(
    std::vector<std::shared_ptr<AnalogTimeSeries>> const& windowed_views,
    std::vector<int64_t> const& alignment_times,
    int64_t window_half_size,
    MockGPUBuffer& buffer)
{
    buffer.clear();
    
    size_t total_points = 0;
    for (auto const& view : windowed_views) {
        total_points += view->getNumSamples();
    }
    buffer.reserve(total_points);
    
    // Populate buffer using elements() iterator
    for (size_t trial_idx = 0; trial_idx < windowed_views.size(); ++trial_idx) {
        auto const& view = windowed_views[trial_idx];
        int64_t const center = alignment_times[trial_idx];
        
        for (auto const& [time, value] : view->elements()) {
            auto const x = static_cast<float>(time.getValue() - center);
            buffer.addPoint(x, value);
        }
    }
}

// ============================================================================
// Benchmark Fixture
// ============================================================================

class LinePlotBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        config_ = LineBenchmarkConfig{};
        std::mt19937 rng(config_.random_seed);
        
        // Generate data
        full_signal_ = generateAnalogSignal(config_.total_samples, rng);
        time_indices_ = generateTimeIndices(config_.total_samples);
        alignment_times_ = generateAlignmentTimes(
            config_.alignment_count, 
            static_cast<int64_t>(config_.total_samples),
            config_.window_half_size, rng);
        
        // Create AnalogTimeSeries for view-based approach
        full_series_ = std::make_shared<AnalogTimeSeries>(full_signal_, config_.total_samples);
        
        // Pre-size buffer
        buffer_.reserve(config_.alignment_count * config_.window_half_size * 2);
    }
    
    void TearDown(benchmark::State const&) override {
        full_signal_.clear();
        time_indices_.clear();
        alignment_times_.clear();
        windowed_vectors_.clear();
        windowed_views_.clear();
        buffer_.clear();
        full_series_.reset();
    }
    
    void ReportStats(benchmark::State& state, bool include_buffer = true) const {
        state.counters["total_samples"] = static_cast<double>(config_.total_samples);
        state.counters["alignment_count"] = static_cast<double>(config_.alignment_count);
        state.counters["window_size"] = static_cast<double>(config_.window_half_size * 2);
        if (include_buffer) {
            state.counters["buffer_points"] = static_cast<double>(buffer_.size());
        }
    }
    
protected:
    LineBenchmarkConfig config_;
    std::vector<float> full_signal_;
    std::vector<TimeFrameIndex> time_indices_;
    std::vector<int64_t> alignment_times_;
    std::shared_ptr<AnalogTimeSeries> full_series_;
    
    // Storage for benchmark iterations
    std::vector<std::vector<float>> windowed_vectors_;
    std::vector<std::shared_ptr<AnalogTimeSeries>> windowed_views_;
    MockGPUBuffer buffer_;
};

// ============================================================================
// Individual Phase Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Extract windows into nested vectors (baseline)
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, ExtractWindows_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_times_.size());
        
        for (auto const align_time : alignment_times_) {
            windowed_vectors_.push_back(extractWindow(
                full_signal_, 
                align_time, 
                config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_vectors_.data());
    }
    
    ReportStats(state, false);
    
    // Report memory usage estimate
    size_t total_copied_samples = 0;
    for (auto const& vec : windowed_vectors_) {
        total_copied_samples += vec.size();
    }
    state.counters["copied_samples"] = static_cast<double>(total_copied_samples);
    state.counters["bytes_copied"] = static_cast<double>(total_copied_samples * sizeof(float));
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, ExtractWindows_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Create views for windows (view-based)
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, CreateViews_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_times_.size());
        
        for (auto const align_time : alignment_times_) {
            windowed_views_.push_back(createSignalWindowView(
                full_series_,
                align_time,
                config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_views_.data());
    }
    
    ReportStats(state, false);
    state.counters["views_created"] = static_cast<double>(windowed_views_.size());
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, CreateViews_ViewBased)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from nested vectors (baseline)
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, PopulateBuffer_Baseline)(benchmark::State& state) {
    // Pre-create windows
    windowed_vectors_.clear();
    windowed_vectors_.reserve(alignment_times_.size());
    for (auto const align_time : alignment_times_) {
        windowed_vectors_.push_back(extractWindow(
            full_signal_, align_time, config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferBaseline(windowed_vectors_, alignment_times_, 
                                  config_.window_half_size, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
        benchmark::DoNotOptimize(buffer_.y_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, PopulateBuffer_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from views using span (view-based)
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, PopulateBuffer_ViewBased_Span)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_times_.size());
    for (auto const align_time : alignment_times_) {
        windowed_views_.push_back(createSignalWindowView(
            full_series_, align_time, config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferViews(windowed_views_, alignment_times_, 
                              config_.window_half_size, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
        benchmark::DoNotOptimize(buffer_.y_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, PopulateBuffer_ViewBased_Span)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Populate GPU buffer from views using iterator (view-based)
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, PopulateBuffer_ViewBased_Iterator)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_times_.size());
    for (auto const align_time : alignment_times_) {
        windowed_views_.push_back(createSignalWindowView(
            full_series_, align_time, config_.window_half_size));
    }
    
    for (auto _ : state) {
        populateGPUBufferViewsIterator(windowed_views_, alignment_times_,
                                       config_.window_half_size, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
        benchmark::DoNotOptimize(buffer_.y_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, PopulateBuffer_ViewBased_Iterator)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Full Pipeline Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Complete line plot pipeline (baseline)
 * 
 * Full workflow: Extract windows → Populate GPU buffer
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, FullPipeline_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        // Phase 1: Extract windows
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_times_.size());
        for (auto const align_time : alignment_times_) {
            windowed_vectors_.push_back(extractWindow(
                full_signal_, align_time, config_.window_half_size));
        }
        
        // Phase 2: Populate buffer
        populateGPUBufferBaseline(windowed_vectors_, alignment_times_,
                                  config_.window_half_size, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, FullPipeline_Baseline)
    ->Unit(benchmark::kMicrosecond);

/**
 * @brief Benchmark: Complete line plot pipeline (view-based with span)
 * 
 * Full workflow: Create views → Populate GPU buffer (using span access)
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, FullPipeline_ViewBased_Span)(benchmark::State& state) {
    for (auto _ : state) {
        // Phase 1: Create views
        windowed_views_.clear();
        windowed_views_.reserve(alignment_times_.size());
        for (auto const align_time : alignment_times_) {
            windowed_views_.push_back(createSignalWindowView(
                full_series_, align_time, config_.window_half_size));
        }
        
        // Phase 2: Populate buffer
        populateGPUBufferViews(windowed_views_, alignment_times_,
                              config_.window_half_size, buffer_);
        
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    
    ReportStats(state);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, FullPipeline_ViewBased_Span)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Scalability Benchmarks (varying parameters)
// ============================================================================

class LineScalabilityBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        config_.alignment_count = static_cast<size_t>(state.range(0));
        std::mt19937 rng(config_.random_seed);
        
        full_signal_ = generateAnalogSignal(config_.total_samples, rng);
        alignment_times_ = generateAlignmentTimes(
            config_.alignment_count,
            static_cast<int64_t>(config_.total_samples),
            config_.window_half_size, rng);
        full_series_ = std::make_shared<AnalogTimeSeries>(full_signal_, config_.total_samples);
    }

protected:
    LineBenchmarkConfig config_;
    std::vector<float> full_signal_;
    std::vector<int64_t> alignment_times_;
    std::shared_ptr<AnalogTimeSeries> full_series_;
    std::vector<std::vector<float>> windowed_vectors_;
    std::vector<std::shared_ptr<AnalogTimeSeries>> windowed_views_;
    MockGPUBuffer buffer_;
};

BENCHMARK_DEFINE_F(LineScalabilityBenchmark, ScaleAlignments_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_times_.size());
        for (auto const align_time : alignment_times_) {
            windowed_vectors_.push_back(extractWindow(
                full_signal_, align_time, config_.window_half_size));
        }
        populateGPUBufferBaseline(windowed_vectors_, alignment_times_,
                                  config_.window_half_size, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_times_.size());
}

BENCHMARK_DEFINE_F(LineScalabilityBenchmark, ScaleAlignments_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_times_.size());
        for (auto const align_time : alignment_times_) {
            windowed_views_.push_back(createSignalWindowView(
                full_series_, align_time, config_.window_half_size));
        }
        populateGPUBufferViews(windowed_views_, alignment_times_,
                              config_.window_half_size, buffer_);
        benchmark::DoNotOptimize(buffer_.x_coords.data());
    }
    state.counters["alignments"] = static_cast<double>(alignment_times_.size());
}

BENCHMARK_REGISTER_F(LineScalabilityBenchmark, ScaleAlignments_Baseline)
    ->Arg(100)->Arg(500)->Arg(1000)->Arg(2000)->Arg(5000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(LineScalabilityBenchmark, ScaleAlignments_ViewBased)
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
BENCHMARK_DEFINE_F(LinePlotBenchmark, AllocationOnly_Baseline)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_vectors_.clear();
        windowed_vectors_.reserve(alignment_times_.size());
        
        for (auto const align_time : alignment_times_) {
            windowed_vectors_.push_back(extractWindow(
                full_signal_, align_time, config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_vectors_.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_DEFINE_F(LinePlotBenchmark, AllocationOnly_ViewBased)(benchmark::State& state) {
    for (auto _ : state) {
        windowed_views_.clear();
        windowed_views_.reserve(alignment_times_.size());
        
        for (auto const align_time : alignment_times_) {
            windowed_views_.push_back(createSignalWindowView(
                full_series_, align_time, config_.window_half_size));
        }
        
        benchmark::DoNotOptimize(windowed_views_.data());
        benchmark::ClobberMemory();
    }
    
    ReportStats(state, false);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, AllocationOnly_Baseline)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(LinePlotBenchmark, AllocationOnly_ViewBased)
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Iteration-Only Comparison  
// ============================================================================

/**
 * @brief Benchmark: Focus on iteration overhead
 * 
 * Pre-allocate windows/views, then only benchmark iteration.
 * Shows cache locality and span access efficiency.
 */
BENCHMARK_DEFINE_F(LinePlotBenchmark, IterationOnly_Baseline)(benchmark::State& state) {
    // Pre-create windows
    windowed_vectors_.clear();
    windowed_vectors_.reserve(alignment_times_.size());
    for (auto const align_time : alignment_times_) {
        windowed_vectors_.push_back(extractWindow(
            full_signal_, align_time, config_.window_half_size));
    }
    
    float sum = 0.0f;
    for (auto _ : state) {
        sum = 0.0f;
        for (size_t i = 0; i < windowed_vectors_.size(); ++i) {
            for (auto const val : windowed_vectors_[i]) {
                sum += val;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_DEFINE_F(LinePlotBenchmark, IterationOnly_ViewBased_Span)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_times_.size());
    for (auto const align_time : alignment_times_) {
        windowed_views_.push_back(createSignalWindowView(
            full_series_, align_time, config_.window_half_size));
    }
    
    float sum = 0.0f;
    for (auto _ : state) {
        sum = 0.0f;
        for (size_t i = 0; i < windowed_views_.size(); ++i) {
            auto span = windowed_views_[i]->getAnalogTimeSeries();
            for (auto const val : span) {
                sum += val;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_DEFINE_F(LinePlotBenchmark, IterationOnly_ViewBased_Iterator)(benchmark::State& state) {
    // Pre-create views
    windowed_views_.clear();
    windowed_views_.reserve(alignment_times_.size());
    for (auto const align_time : alignment_times_) {
        windowed_views_.push_back(createSignalWindowView(
            full_series_, align_time, config_.window_half_size));
    }
    
    float sum = 0.0f;
    for (auto _ : state) {
        sum = 0.0f;
        for (size_t i = 0; i < windowed_views_.size(); ++i) {
            for (auto const& [time, value] : windowed_views_[i]->elements()) {
                sum += value;
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    ReportStats(state, false);
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_REGISTER_F(LinePlotBenchmark, IterationOnly_Baseline)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(LinePlotBenchmark, IterationOnly_ViewBased_Span)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(LinePlotBenchmark, IterationOnly_ViewBased_Iterator)
    ->Unit(benchmark::kMicrosecond);

}  // namespace LinePlotBenchmarks

BENCHMARK_MAIN();
