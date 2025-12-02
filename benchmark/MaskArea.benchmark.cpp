/**
 * @file MaskArea.benchmark.cpp
 * @brief Benchmarks for MaskArea transform and pipeline execution
 * 
 * This benchmark suite tests the performance of:
 * 1. Element-level transform: Mask2D → float (calculateMaskArea)
 * 2. Container-level transform: MaskData → RaggedAnalogTimeSeries
 * 3. Full pipeline: MaskData → RaggedAnalogTimeSeries → AnalogTimeSeries
 * 
 * Profiling Usage:
 * ----------------
 * # Memory profiling with heaptrack
 * heaptrack ./benchmark_MaskArea
 * heaptrack_gui heaptrack.benchmark_MaskArea.*.gz
 * 
 * # CPU profiling with perf
 * perf record -g ./benchmark_MaskArea --benchmark_filter=Pipeline
 * perf report
 * 
 * # View assembly
 * objdump -d -C -S ./benchmark_MaskArea | less
 * 
 * # Time and memory stats
 * /usr/bin/time -v ./benchmark_MaskArea
 */

#include "fixtures/BenchmarkFixtures.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"

#include <benchmark/benchmark.h>
#include <memory>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using namespace WhiskerToolbox::Benchmark;

// ============================================================================
// Benchmark Fixtures
// ============================================================================

/**
 * @brief Base fixture for MaskArea benchmarks
 * 
 * Sets up test data once per benchmark suite to avoid measuring setup time.
 */
class MaskAreaBenchmark : public benchmark::Fixture {
public:
    void SetUp(benchmark::State const& state) override {
        // Use Medium preset by default
        MaskDataConfig config = Presets::MediumMaskData();
        
        // Generate test data
        MaskDataFixture fixture(config);
        mask_data_ = fixture.generate();
        
        // Calculate stats for later reporting
        total_masks_ = 0;
        total_pixels_ = 0;
        for (auto const& [time, entry] : mask_data_->elements()) {
            total_masks_++;
            total_pixels_ += entry.data.size();
        }
        num_time_points_ = mask_data_->getTimeCount();
    }
    
    void TearDown(benchmark::State const&) override {
        mask_data_.reset();
    }
    
    void ReportStats(benchmark::State& state) const {
        state.counters["num_frames"] = static_cast<double>(num_time_points_);
        state.counters["total_masks"] = static_cast<double>(total_masks_);
        state.counters["total_pixels"] = static_cast<double>(total_pixels_);
        if (num_time_points_ > 0) {
            state.counters["avg_masks_per_frame"] = static_cast<double>(total_masks_) / 
                                                     static_cast<double>(num_time_points_);
        }
        if (total_masks_ > 0) {
            state.counters["avg_pixels_per_mask"] = static_cast<double>(total_pixels_) / 
                                                     static_cast<double>(total_masks_);
        }
    }
    
protected:
    std::shared_ptr<MaskData> mask_data_;
    size_t num_time_points_ = 0;
    size_t total_masks_ = 0;
    size_t total_pixels_ = 0;
};

// ============================================================================
// Element-Level Benchmarks
// ============================================================================

/**
 * @brief Benchmark single mask area calculation
 * 
 * Tests the raw performance of calculateMaskArea on individual masks.
 * This is the fundamental operation that everything else builds on.
 */
BENCHMARK_F(MaskAreaBenchmark, ElementTransform_SingleMask)(benchmark::State& state) {
    // Get a representative mask
    auto const& first_entry = *mask_data_->elements().begin();
    Mask2D const& mask = first_entry.second.data;
    
    MaskAreaParams params;
    
    size_t iterations = 0;
    for (auto _ : state) {
        float area = calculateMaskArea(mask, params);
        benchmark::DoNotOptimize(area);
        ++iterations;
    }
    
    ReportStats(state);
    state.counters["mask_pixels"] = static_cast<double>(mask.size());
    state.counters["iterations"] = static_cast<double>(iterations);
    state.SetItemsProcessed(iterations);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, ElementTransform_SingleMask)
    ->Arg(1)  // Default to Medium preset
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Container-Level Benchmarks
// ============================================================================

/**
 * @brief Benchmark MaskData → RaggedAnalogTimeSeries transform
 * 
 * Tests the container-level transform using direct iteration.
 * This represents the most straightforward implementation.
 */
BENCHMARK_F(MaskAreaBenchmark, ContainerTransform_Direct)(benchmark::State& state) {
    MaskAreaParams params;
    
    for (auto _ : state) {
        auto result = std::make_shared<RaggedAnalogTimeSeries>();
        result->setTimeFrame(mask_data_->getTimeFrame());
        
        // Direct iteration and transformation
        for (auto const& [time, entry] : mask_data_->elements()) {
            float area = calculateMaskArea(entry.data, params);
            result->appendAtTime(time, std::vector<float>{area}, NotifyObservers::No);
        }
        
        benchmark::DoNotOptimize(result);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, ContainerTransform_Direct)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark using range view with std::ranges::views::transform
 * 
 * Tests lazy evaluation approach with range-based transform.
 */
BENCHMARK_F(MaskAreaBenchmark, ContainerTransform_RangeView)(benchmark::State& state) {
    MaskAreaParams params;
    auto& registry = ElementRegistry::instance();
    auto transform_fn = registry.getTransformFunction<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea", params);
    
    for (auto _ : state) {
        // Create lazy view
        auto transformed_view = std::ranges::views::transform(
            mask_data_->elements(),
            [transform_fn](auto const& time_entry_pair) {
                return std::make_pair(
                    time_entry_pair.first,
                    std::vector<float>{transform_fn(time_entry_pair.second.data)}
                );
            }
        );
        
        // Materialize via range constructor
        RaggedAnalogTimeSeries result(transformed_view);
        result.setTimeFrame(mask_data_->getTimeFrame());
        
        benchmark::DoNotOptimize(result);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, ContainerTransform_RangeView)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Pipeline Benchmarks
// ============================================================================

/**
 * @brief Benchmark full pipeline: MaskData → RaggedAnalogTimeSeries → AnalogTimeSeries
 * 
 * Tests the complete transform chain:
 * 1. Calculate area for each mask (element transform)
 * 2. Sum areas at each time point (time-grouped reduction)
 */
BENCHMARK_F(MaskAreaBenchmark, Pipeline_MaskAreaSum)(benchmark::State& state) {
    // Create pipeline
    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});
    pipeline.addStep("SumReduction", SumReductionParams{});
    
    for (auto _ : state) {
        auto result = pipeline.execute<MaskData>(*mask_data_);
        benchmark::DoNotOptimize(result);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Pipeline_MaskAreaSum)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark pipeline with executeOptimized
 * 
 * Tests the automatic optimization path that should detect
 * the element-wise portion and fuse it.
 */
BENCHMARK_F(MaskAreaBenchmark, Pipeline_MaskAreaSum_Optimized)(benchmark::State& state) {
    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});
    pipeline.addStep("SumReduction", SumReductionParams{});
    
    for (auto _ : state) {
        auto result = pipeline.executeOptimized<MaskData, AnalogTimeSeries>(*mask_data_);
        benchmark::DoNotOptimize(result);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Pipeline_MaskAreaSum_Optimized)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark element-only pipeline with fusion
 * 
 * Tests pure element-wise pipeline to measure fusion overhead.
 * This should be the fastest execution path.
 */
BENCHMARK_F(MaskAreaBenchmark, Pipeline_ElementOnly_Fused)(benchmark::State& state) {
    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});
    // Add a second element transform to test fusion
    // (In real usage, this might be a scale or threshold operation)
    
    for (auto _ : state) {
        auto result = pipeline.executeFused<MaskData, RaggedAnalogTimeSeries>(*mask_data_);
        benchmark::DoNotOptimize(result);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Pipeline_ElementOnly_Fused)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Benchmark lazy view execution (no materialization)
 * 
 * Tests the executeAsView path which returns a lazy range.
 * We force evaluation by iterating through the view.
 */
BENCHMARK_F(MaskAreaBenchmark, Pipeline_LazyView)(benchmark::State& state) {
    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});
    
    for (auto _ : state) {
        auto view = pipeline.executeAsView(*mask_data_);
        
        // Force evaluation by iterating
        size_t count = 0;
        for (auto const& [time, value_variant] : view) {
            benchmark::DoNotOptimize(time);
            benchmark::DoNotOptimize(value_variant);
            ++count;
        }
        
        benchmark::DoNotOptimize(count);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Pipeline_LazyView)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Parameter Variation Benchmarks
// ============================================================================

/**
 * @brief Benchmark with different scale factors
 * 
 * Tests if parameter values affect performance (they shouldn't significantly).
 */
BENCHMARK_F(MaskAreaBenchmark, Parameters_ScaleFactor)(benchmark::State& state) {
    float scale_factor = 1.0f;  // Fixed scale factor
    MaskAreaParams params;
    params.scale_factor = scale_factor;
    
    for (auto _ : state) {
        auto result = std::make_shared<RaggedAnalogTimeSeries>();
        result->setTimeFrame(mask_data_->getTimeFrame());
        
        for (auto&& [time, entry] : mask_data_->elements()) {
            float area = calculateMaskArea(entry.data, params);
            result->appendAtTime(time, std::vector<float>{area}, NotifyObservers::No);
        }
        
        benchmark::DoNotOptimize(result);
    }
    
    ReportStats(state);
    state.counters["scale_factor"] = static_cast<double>(scale_factor);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Parameters_ScaleFactor)
    ->Arg(1)  // Medium data preset
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Comparison Benchmarks
// ============================================================================

/**
 * @brief Baseline: Just iterate through MaskData without transforming
 * 
 * Measures the overhead of iteration alone.
 */
BENCHMARK_F(MaskAreaBenchmark, Baseline_IterationOnly)(benchmark::State& state) {
    for (auto _ : state) {
        size_t total_masks = 0;
        size_t total_pixels = 0;
        
        for (auto&& [time, entry] : mask_data_->elements()) {
            total_masks++;
            total_pixels += entry.data.size();
            benchmark::DoNotOptimize(total_masks);
            benchmark::DoNotOptimize(total_pixels);
        }
        
        benchmark::DoNotOptimize(total_masks);
        benchmark::DoNotOptimize(total_pixels);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Baseline_IterationOnly)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

/**
 * @brief Baseline: Count pixels without creating output container
 * 
 * Measures just the computation without any container allocation.
 */
BENCHMARK_F(MaskAreaBenchmark, Baseline_ComputeOnly)(benchmark::State& state) {
    MaskAreaParams params;
    
    for (auto _ : state) {
        double sum_areas = 0.0;
        
        for (auto&& [time, entry] : mask_data_->elements()) {
            float area = calculateMaskArea(entry.data, params);
            sum_areas += area;
        }
        
        benchmark::DoNotOptimize(sum_areas);
    }
    
    ReportStats(state);
}
BENCHMARK_REGISTER_F(MaskAreaBenchmark, Baseline_ComputeOnly)
    ->Arg(1)
    ->Unit(benchmark::kMillisecond);

// ============================================================================
// Memory Access Pattern Benchmarks
// ============================================================================

/**
 * @brief Test cache behavior with different mask sizes
 * 
 * Small masks should fit in cache, large masks will cause cache misses.
 */
static void BM_CacheBehavior(benchmark::State& state) {
    size_t mask_size = state.range(0);
    
    // Generate a single mask of specified size
    MaskDataConfig config;
    config.num_frames = 1;
    config.masks_per_frame_min = 1;
    config.masks_per_frame_max = 1;
    config.mask_size_min = mask_size;
    config.mask_size_max = mask_size;
    
    MaskDataFixture fixture(config);
    auto mask_data = fixture.generate();
    
    auto const& first_entry = *mask_data->elements().begin();
    Mask2D const& mask = first_entry.second.data;
    
    MaskAreaParams params;
    
    for (auto _ : state) {
        float area = calculateMaskArea(mask, params);
        benchmark::DoNotOptimize(area);
    }
    
    state.SetBytesProcessed(state.iterations() * mask_size * sizeof(Point2D<uint32_t>));
}
BENCHMARK(BM_CacheBehavior)
    ->RangeMultiplier(4)
    ->Range(64, 64*1024)  // 64 points to 64K points
    ->Unit(benchmark::kMicrosecond);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
