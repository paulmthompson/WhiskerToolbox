/**
 * @file ContactAlignedSpikes.benchmark.cpp
 * @brief Benchmark for spikes aligned to contact onsets using GatherResult
 * 
 * This benchmark measures:
 * 1. Time to create GatherResult of spikes aligned to contact onsets (~5500 contacts)
 * 2. Time to extract normalized time values using RasterMapper (like EventPlotWidget)
 * 3. Identifies bottlenecks in the pipeline
 * 
 * Expected performance target: < 50ms for normalized time extraction
 */

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "WhiskerToolbox/Plots/Common/PlotAlignmentGather.hpp"
#include "WhiskerToolbox/Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"
#include "CorePlotting/Mappers/RasterMapper.hpp"
#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <glm/glm.hpp>

#include <benchmark/benchmark.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

namespace ContactAlignedBenchmarks {

// ============================================================================
// Data Loading
// ============================================================================

/**
 * @brief Load data from JSON config, excluding video
 */
std::shared_ptr<DataManager> loadDataFromJson(std::string const& json_path) {
    auto dm = std::make_shared<DataManager>();
    
    // Load JSON config
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open JSON file: " << json_path << std::endl;
        return nullptr;
    }
    
    nlohmann::json j;
    ifs >> j;
    
    // Filter out video entries
    nlohmann::json filtered_j;
    for (auto const& item : j) {
        if (!item.contains("data_type") || item["data_type"] != "video") {
            filtered_j.push_back(item);
        }
    }
    
    // Get base path
    std::string const base_path = std::filesystem::path(json_path).parent_path().string();
    
    // Load data using the JSON object version (exported in DataManager.hpp)
    std::vector<DataInfo> data_info = load_data_from_json_config(dm.get(), filtered_j, base_path);
    
    if (data_info.empty()) {
        std::cerr << "No data loaded from JSON config" << std::endl;
        return nullptr;
    }
    
    std::cout << "Loaded " << data_info.size() << " data items" << std::endl;
    return dm;
}

// ============================================================================
// Benchmark Fixture
// ============================================================================

class ContactAlignedBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State & state) override {
        // Path to data
        std::string const json_path = "/mnt/c/Users/wanglab/Data/TG/M054/0427_1/data_bench.json";
        
        // Load data
        _data_manager = loadDataFromJson(json_path);
        if (!_data_manager) {
            state.SkipWithError("Failed to load data");
            return;
        }
        
        // Get spikes data
        _spikes = _data_manager->getData<DigitalEventSeries>("spikes");
        if (!_spikes) {
            state.SkipWithError("Failed to get spikes data");
            return;
        }
        
        // Get contact data
        _contacts = _data_manager->getData<DigitalIntervalSeries>("Contact_Events");
        if (!_contacts) {
            state.SkipWithError("Failed to get contact data");
            return;
        }
        
        std::cout << "Spikes count: " << _spikes->size() << std::endl;
        std::cout << "Contact intervals count: " << _contacts->size() << std::endl;
        
        // Get time frame for spikes
        _time_frame = _spikes->getTimeFrame();
        if (!_time_frame) {
            state.SkipWithError("Spikes data has no TimeFrame");
            return;
        }
        
        // Pre-create layout for testing (simplified - single row)
        _layout = CorePlotting::SeriesLayout{
            "trial_0",
            CorePlotting::LayoutTransform{0.0f, 0.1f},
            0
        };
    }
    
    void TearDown(benchmark::State const&) override {
        _data_manager.reset();
        _spikes.reset();
        _contacts.reset();
        _time_frame.reset();
        _gathered.reset();
    }
    
protected:
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<DigitalEventSeries> _spikes;
    std::shared_ptr<DigitalIntervalSeries> _contacts;
    std::shared_ptr<TimeFrame> _time_frame;
    CorePlotting::SeriesLayout _layout;
    
    // Cache for benchmarks that need pre-computed gather result
    std::optional<GatherResult<DigitalEventSeries>> _gathered;
};

// ============================================================================
// Individual Phase Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Create GatherResult aligned to contact onsets
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, CreateGatherResult)(benchmark::State& state) {
    for (auto _ : state) {
        // Create GatherResult aligned to contact interval starts
        auto gathered = Neuralyzer::Plots::gatherWithIntervalAlignment<DigitalEventSeries>(
            _spikes,
            _contacts,
            Neuralyzer::Transforms::V2::AlignmentPoint::Start
        );
        
        benchmark::DoNotOptimize(gathered.source());
        benchmark::ClobberMemory();
        
        // Store for other benchmarks
        _gathered = gathered;
    }
    
    state.counters["num_contacts"] = static_cast<double>(_contacts->size());
    state.counters["num_spikes"] = static_cast<double>(_spikes->size());
    
    if (_gathered.has_value()) {
        // Count total spikes in all trials
        size_t total_trial_spikes = 0;
        for (auto const& trial : _gathered.value()) {
            total_trial_spikes += trial->size();
        }
        state.counters["total_trial_spikes"] = static_cast<double>(total_trial_spikes);
    }
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, CreateGatherResult)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: Create views individually (without GatherResult)
 * 
 * Measures the cost of creating views one at a time to compare with
 * GatherResult's batch creation approach.
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, CreateViewsIndividually)(benchmark::State& state) {
    // Extract contact intervals
    std::vector<Interval> contact_intervals;
    contact_intervals.reserve(_contacts->size());
    for (auto const& interval : _contacts->view()) {
        contact_intervals.push_back(interval.interval);
    }
    
    std::vector<std::shared_ptr<DigitalEventSeries>> views;
    views.reserve(contact_intervals.size());
    
    for (auto _ : state) {
        views.clear();
        views.reserve(contact_intervals.size());
        
        // Create views individually using full interval bounds
        for (auto const& contact_interval : contact_intervals) {
            auto view = DigitalEventSeries::createView(
                _spikes,
                TimeFrameIndex(contact_interval.start),
                TimeFrameIndex(contact_interval.end)
            );
            views.push_back(std::move(view));
        }
        
        benchmark::DoNotOptimize(views.data());
        benchmark::ClobberMemory();
    }
    
    state.counters["num_contacts"] = static_cast<double>(contact_intervals.size());
    state.counters["num_spikes"] = static_cast<double>(_spikes->size());
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, CreateViewsIndividually)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: Create GatherResult using basic gather() (no adapter)
 * 
 * This uses the direct gather() function without the IntervalWithAlignmentAdapter,
 * which should be faster than gatherWithIntervalAlignment.
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, CreateGatherResult_Basic)(benchmark::State& state) {
    for (auto _ : state) {
        // Use basic gather() without adapter - aligns to interval starts
        auto gathered = gather(_spikes, _contacts);
        
        benchmark::DoNotOptimize(gathered.source());
        benchmark::ClobberMemory();
    }
    
    state.counters["num_contacts"] = static_cast<double>(_contacts->size());
    state.counters["num_spikes"] = static_cast<double>(_spikes->size());
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, CreateGatherResult_Basic)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: Extract normalized time values using RasterMapper
 * 
 * This mimics what EventPlotWidget does in rebuildScene()
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, ExtractNormalizedTimes_RasterMapper)(benchmark::State& state) {
    // Pre-create gather result
    auto gathered = Neuralyzer::Plots::gatherWithIntervalAlignment<DigitalEventSeries>(
        _spikes,
        _contacts,
        Neuralyzer::Transforms::V2::AlignmentPoint::Start
    );
    
    if (gathered.empty()) {
        state.SkipWithError("GatherResult is empty");
        return;
    }
    
    // Window size for filtering (in time units, not indices)
    // Using a reasonable window: ±500ms = ±500 time units if 1 unit = 1ms
    int const window_before = 500;
    int const window_after = 500;
    
    // Storage for normalized times
    std::vector<std::vector<float>> normalized_times_per_trial;
    normalized_times_per_trial.reserve(gathered.size());
    
    for (auto _ : state) {
        normalized_times_per_trial.clear();
        normalized_times_per_trial.reserve(gathered.size());
        
        // Process each trial (contact)
        for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
            auto const& trial_view = gathered[trial_idx];
            if (!trial_view) continue;
            
            // Get alignment time (contact onset) — already absolute time
            auto ref_abs_time = static_cast<int>(gathered.alignmentTimeAt(trial_idx));
            
            // Use RasterMapper to get normalized times (like EventPlotWidget)
            auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
                *trial_view,
                _layout,
                *_time_frame,
                ref_abs_time,
                window_before,
                window_after
            );
            
            // Materialize normalized times
            std::vector<float> normalized_times;
            for (auto const& elem : mapped) {
                normalized_times.push_back(elem.x);
            }
            
            normalized_times_per_trial.push_back(std::move(normalized_times));
        }
        
        benchmark::DoNotOptimize(normalized_times_per_trial.data());
    }
    
    state.counters["num_contacts"] = static_cast<double>(gathered.size());
    
    // Count total normalized events
    size_t total_normalized = 0;
    for (auto const& trial_times : normalized_times_per_trial) {
        total_normalized += trial_times.size();
    }
    state.counters["total_normalized_events"] = static_cast<double>(total_normalized);
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, ExtractNormalizedTimes_RasterMapper)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: Full pipeline (GatherResult + Normalized Times)
 * 
 * Measures the complete workflow from contact intervals to normalized times
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, FullPipeline)(benchmark::State& state) {
    int const window_before = 500;
    int const window_after = 500;
    
    std::vector<std::vector<float>> normalized_times_per_trial;
    
    for (auto _ : state) {
        // Phase 1: Create GatherResult
        auto gathered = Neuralyzer::Plots::gatherWithIntervalAlignment<DigitalEventSeries>(
            _spikes,
            _contacts,
            Neuralyzer::Transforms::V2::AlignmentPoint::Start
        );
        
        if (gathered.empty()) {
            state.SkipWithError("GatherResult is empty");
            return;
        }
        
        // Phase 2: Extract normalized times
        normalized_times_per_trial.clear();
        normalized_times_per_trial.reserve(gathered.size());
        
        for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
            auto const& trial_view = gathered[trial_idx];
            if (!trial_view) continue;
            
            auto ref_abs_time = static_cast<int>(gathered.alignmentTimeAt(trial_idx));
            
            auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
                *trial_view,
                _layout,
                *_time_frame,
                ref_abs_time,
                window_before,
                window_after
            );
            
            std::vector<float> normalized_times;
            for (auto const& elem : mapped) {
                normalized_times.push_back(elem.x);
            }
            
            normalized_times_per_trial.push_back(std::move(normalized_times));
        }
        
        benchmark::DoNotOptimize(normalized_times_per_trial.data());
    }
    
    state.counters["num_contacts"] = static_cast<double>(_contacts->size());
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, FullPipeline)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: Iteration only (pre-created GatherResult)
 * 
 * Measures just the cost of iterating through GatherResult and extracting times
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, IterationOnly)(benchmark::State& state) {
    // Pre-create gather result
    auto gathered = Neuralyzer::Plots::gatherWithIntervalAlignment<DigitalEventSeries>(
        _spikes,
        _contacts,
        Neuralyzer::Transforms::V2::AlignmentPoint::Start
    );
    
    if (gathered.empty()) {
        state.SkipWithError("GatherResult is empty");
        return;
    }
    
    int const window_before = 500;
    int const window_after = 500;
    
    int64_t sum = 0;  // Checksum to prevent optimization
    
    for (auto _ : state) {
        sum = 0;
        
        for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
            auto const& trial_view = gathered[trial_idx];
            if (!trial_view) continue;
            
            // Just iterate and sum times (no normalization)
            for (auto const& event : trial_view->view()) {
                sum += event.time().getValue();
            }
        }
        
        benchmark::DoNotOptimize(sum);
    }
    
    state.counters["num_contacts"] = static_cast<double>(gathered.size());
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, IterationOnly)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: TimeFrame conversion cost
 * 
 * Measures the cost of converting TimeFrameIndex to absolute time
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, TimeFrameConversion)(benchmark::State& state) {
    // Pre-create gather result
    auto gathered = Neuralyzer::Plots::gatherWithIntervalAlignment<DigitalEventSeries>(
        _spikes,
        _contacts,
        Neuralyzer::Transforms::V2::AlignmentPoint::Start
    );
    
    if (gathered.empty()) {
        state.SkipWithError("GatherResult is empty");
        return;
    }
    
    int sum = 0;
    
    for (auto _ : state) {
        sum = 0;
        
        for (size_t trial_idx = 0; trial_idx < gathered.size(); ++trial_idx) {
            auto const& trial_view = gathered[trial_idx];
            if (!trial_view) continue;
            
            int const ref_abs_time = static_cast<int>(gathered.alignmentTimeAt(trial_idx));
            
            for (auto const& event : trial_view->view()) {
                int abs_time = _time_frame->getTimeAtIndex(event.time());
                sum += abs_time - ref_abs_time;  // Relative time
            }
        }
        
        benchmark::DoNotOptimize(sum);
    }
    
    state.counters["num_contacts"] = static_cast<double>(gathered.size());
    state.counters["checksum"] = static_cast<double>(sum);
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, TimeFrameConversion)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// EventPlotWidget rebuildScene() Benchmarks
// ============================================================================

/**
 * @brief Benchmark: Full rebuildScene() pipeline (EventPlotWidget workflow)
 * 
 * This mimics the exact workflow in EventPlotOpenGLWidget::rebuildScene():
 * 1. Gather trial-aligned data
 * 2. Build layout request
 * 3. Compute layout
 * 4. Map events with RasterMapper
 * 5. Build scene
 * 6. (uploadScene not included - requires OpenGL context)
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, RebuildScene_FullPipeline)(benchmark::State& state) {
    // Setup alignment data (mimicking EventPlotState)
    PlotAlignmentData alignment_data;
    alignment_data.alignment_event_key = "Contact_Events";
    alignment_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    alignment_data.window_size = 1000.0;  // ±500 window
    
    // Setup view state (mimicking EventPlotViewState)
    double x_min = -500.0;
    double x_max = 500.0;
    
    for (auto _ : state) {
        // Phase 1: Gather trial-aligned data (gatherTrialData)
        auto gathered = Neuralyzer::Plots::createAlignedGatherResult<DigitalEventSeries>(
            _data_manager,
            "spikes",
            alignment_data
        );
        
        if (gathered.empty()) {
            state.SkipWithError("GatherResult is empty");
            return;
        }
        
        // Phase 2: Build layout request
        size_t num_trials = gathered.size();
        CorePlotting::LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        
        for (size_t i = 0; i < num_trials; ++i) {
            std::string key = "trial_" + std::to_string(i);
            layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
        }
        
        // Phase 3: Compute layout using RowLayoutStrategy
        CorePlotting::RowLayoutStrategy layout_strategy;
        CorePlotting::LayoutResponse layout_response = layout_strategy.compute(layout_request);
        
        // Phase 4: Build scene with SceneBuilder
        // BoundingBox constructor: (min_x, min_y, max_x, max_y)
        BoundingBox bounds{
            static_cast<float>(x_min),  // min_x
            -1.0f,                       // min_y
            static_cast<float>(x_max),  // max_x
            1.0f                         // max_y
        };
        
        CorePlotting::SceneBuilder builder;
        builder.setBounds(bounds);
        
        // Get time frame from source series
        auto time_frame = _spikes->getTimeFrame();
        if (!time_frame) {
            state.SkipWithError("No time frame");
            return;
        }
        
        // Phase 5: Map each trial's events and add to builder
        CorePlotting::GlyphStyle style;
        style.size = 3.0f;
        style.color = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
        
        for (size_t trial = 0; trial < num_trials; ++trial) {
            auto const& trial_view = gathered[trial];
            if (!trial_view) continue;
            
            std::string key = "trial_" + std::to_string(trial);
            auto const* trial_layout = layout_response.findLayout(key);
            if (!trial_layout) continue;
            
            // Get alignment time
            auto ref_abs_time = static_cast<int>(gathered.alignmentTimeAt(trial));
            
            // Use RasterMapper to generate mapped elements
            auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
                *trial_view,
                *trial_layout,
                *time_frame,
                ref_abs_time,
                static_cast<int>(-x_min),
                static_cast<int>(x_max)
            );
            
            // Convert range to vector for builder (materialization)
            std::vector<CorePlotting::MappedElement> elements;
            for (auto const& elem : mapped) {
                elements.push_back(elem);
            }
            
            builder.addGlyphs(key, std::move(elements), style);
        }
        
        // Phase 6: Build scene
        auto scene = builder.build();
        
        benchmark::DoNotOptimize(scene.glyph_batches.size());
    }
    
    state.counters["num_contacts"] = static_cast<double>(_contacts->size());
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, RebuildScene_FullPipeline)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: Layout computation phase
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, RebuildScene_LayoutComputation)(benchmark::State& state) {
    // Pre-create gather result
    PlotAlignmentData alignment_data;
    alignment_data.alignment_event_key = "Contact_Events";
    alignment_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    
    auto gathered = Neuralyzer::Plots::createAlignedGatherResult<DigitalEventSeries>(
        _data_manager,
        "spikes",
        alignment_data
    );
    
    if (gathered.empty()) {
        state.SkipWithError("GatherResult is empty");
        return;
    }
    
    size_t num_trials = gathered.size();
    
    for (auto _ : state) {
        // Build layout request
        CorePlotting::LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        
        for (size_t i = 0; i < num_trials; ++i) {
            std::string key = "trial_" + std::to_string(i);
            layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
        }
        
        // Compute layout
        CorePlotting::RowLayoutStrategy layout_strategy;
        auto layout_response = layout_strategy.compute(layout_request);
        
        benchmark::DoNotOptimize(layout_response.layouts.size());
    }
    
    state.counters["num_contacts"] = static_cast<double>(num_trials);
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, RebuildScene_LayoutComputation)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: RasterMapper mapping and materialization phase
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, RebuildScene_RasterMapping)(benchmark::State& state) {
    // Pre-create gather result and layout
    PlotAlignmentData alignment_data;
    alignment_data.alignment_event_key = "Contact_Events";
    alignment_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    
    auto gathered = Neuralyzer::Plots::createAlignedGatherResult<DigitalEventSeries>(
        _data_manager,
        "spikes",
        alignment_data
    );
    
    if (gathered.empty()) {
        state.SkipWithError("GatherResult is empty");
        return;
    }
    
    // Pre-compute layout
    size_t num_trials = gathered.size();
    CorePlotting::LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;
    
    for (size_t i = 0; i < num_trials; ++i) {
        std::string key = "trial_" + std::to_string(i);
        layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
    }
    
    CorePlotting::RowLayoutStrategy layout_strategy;
    auto layout_response = layout_strategy.compute(layout_request);
    
    auto time_frame = _spikes->getTimeFrame();
    if (!time_frame) {
        state.SkipWithError("No time frame");
        return;
    }
    
    double x_min = -500.0;
    double x_max = 500.0;
    
    size_t total_elements = 0;
    
    for (auto _ : state) {
        total_elements = 0;
        
        for (size_t trial = 0; trial < num_trials; ++trial) {
            auto const& trial_view = gathered[trial];
            if (!trial_view) continue;
            
            std::string key = "trial_" + std::to_string(trial);
            auto const* trial_layout = layout_response.findLayout(key);
            if (!trial_layout) continue;
            
            auto ref_abs_time = static_cast<int>(gathered.alignmentTimeAt(trial));
            
            // Map events
            auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
                *trial_view,
                *trial_layout,
                *time_frame,
                ref_abs_time,
                static_cast<int>(-x_min),
                static_cast<int>(x_max)
            );
            
            // Materialize to vector
            std::vector<CorePlotting::MappedElement> elements;
            for (auto const& elem : mapped) {
                elements.push_back(elem);
            }
            
            total_elements += elements.size();
        }
        
        benchmark::DoNotOptimize(total_elements);
    }
    
    state.counters["num_contacts"] = static_cast<double>(num_trials);
    state.counters["total_elements"] = static_cast<double>(total_elements);
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, RebuildScene_RasterMapping)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark: SceneBuilder building phase
 */
BENCHMARK_DEFINE_F(ContactAlignedBenchmark, RebuildScene_SceneBuilding)(benchmark::State& state) {
    // Pre-create gather result, layout, and mapped elements
    PlotAlignmentData alignment_data;
    alignment_data.alignment_event_key = "Contact_Events";
    alignment_data.interval_alignment_type = IntervalAlignmentType::Beginning;
    
    auto gathered = Neuralyzer::Plots::createAlignedGatherResult<DigitalEventSeries>(
        _data_manager,
        "spikes",
        alignment_data
    );
    
    if (gathered.empty()) {
        state.SkipWithError("GatherResult is empty");
        return;
    }
    
    // Pre-compute layout
    size_t num_trials = gathered.size();
    CorePlotting::LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;
    
    for (size_t i = 0; i < num_trials; ++i) {
        std::string key = "trial_" + std::to_string(i);
        layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
    }
    
    CorePlotting::RowLayoutStrategy layout_strategy;
    auto layout_response = layout_strategy.compute(layout_request);
    
    auto time_frame = _spikes->getTimeFrame();
    if (!time_frame) {
        state.SkipWithError("No time frame");
        return;
    }
    
    double x_min = -500.0;
    double x_max = 500.0;
    
    // Pre-materialize all elements
    std::vector<std::vector<CorePlotting::MappedElement>> all_elements;
    all_elements.reserve(num_trials);
    
    for (size_t trial = 0; trial < num_trials; ++trial) {
        auto const& trial_view = gathered[trial];
        if (!trial_view) continue;
        
        std::string key = "trial_" + std::to_string(trial);
        auto const* trial_layout = layout_response.findLayout(key);
        if (!trial_layout) continue;
        
        auto ref_abs_time = static_cast<int>(gathered.alignmentTimeAt(trial));
        
        auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
            *trial_view,
            *trial_layout,
            *time_frame,
            ref_abs_time,
            static_cast<int>(-x_min),
            static_cast<int>(x_max)
        );
        
        std::vector<CorePlotting::MappedElement> elements;
        for (auto const& elem : mapped) {
            elements.push_back(elem);
        }
        
        all_elements.push_back(std::move(elements));
    }
    
    for (auto _ : state) {
        // BoundingBox constructor: (min_x, min_y, max_x, max_y)
        BoundingBox bounds{
            static_cast<float>(x_min),  // min_x
            -1.0f,                       // min_y
            static_cast<float>(x_max),  // max_x
            1.0f                         // max_y
        };
        
        CorePlotting::SceneBuilder builder;
        builder.setBounds(bounds);
        
        CorePlotting::GlyphStyle style;
        style.size = 3.0f;
        style.color = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);
        
        // Add all glyphs
        for (size_t trial = 0; trial < all_elements.size(); ++trial) {
            std::string key = "trial_" + std::to_string(trial);
            builder.addGlyphs(key, all_elements[trial], style);
        }
        
        // Build scene
        auto scene = builder.build();
        
        benchmark::DoNotOptimize(scene.glyph_batches.size());
    }
    
    state.counters["num_contacts"] = static_cast<double>(num_trials);
    state.counters["total_elements"] = static_cast<double>(
        std::accumulate(all_elements.begin(), all_elements.end(), size_t(0),
            [](size_t sum, auto const& vec) { return sum + vec.size(); }));
}

BENCHMARK_REGISTER_F(ContactAlignedBenchmark, RebuildScene_SceneBuilding)
    ->Unit(benchmark::kMillisecond);

} // namespace ContactAlignedBenchmarks

BENCHMARK_MAIN();
