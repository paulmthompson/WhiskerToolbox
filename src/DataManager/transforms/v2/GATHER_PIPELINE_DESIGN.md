# GatherResult + TransformPipeline Integration

## Overview

This document describes the integration of `GatherResult` with `TransformPipeline` for runtime-configurable, composable view transformations and reductions for trial-aligned analysis.

## Use Cases

### Primary: Raster Plot with Sorting

```
Input: DigitalEventSeries (spikes), DigitalIntervalSeries (trials)
       
User wants to:
1. Gather spikes by trial intervals
2. Normalize each trial's spike times to alignment (t=0 at trial start)
3. Compute first-spike latency per trial
4. Sort trials by latency
5. Draw raster (no intermediate storage)
```

### Secondary: Multi-Data Conditioning

```
Input: DigitalEventSeries (spikes), AnalogTimeSeries (behavior), 
       DigitalIntervalSeries (trials)

User wants to:
1. Gather both spikes and behavior by trial
2. Compute max behavior value per trial
3. Partition spikes into "high behavior" and "low behavior" groups
4. Draw with different colors
```

## Architecture

### Component Responsibilities

| Component | Responsibility |
|-----------|----------------|
| `ElementRegistry` | Stores element transforms |
| `TimeGroupedTransform` | Per-time-point M→N reductions |
| `RangeReductionRegistry` | Stores whole-range → scalar reductions |
| `TransformPipeline` | Composes transforms, produces view adaptors and reducers |
| `GatherResult` | Provides trial structure, consumes pipeline adaptors |
| `PipelineValueStore` | Stores per-trial computed values for parameter binding |

### Two Types of Reductions

| Aspect | TimeGroupedTransform | RangeReduction |
|--------|---------------------|----------------|
| **Scope** | Elements at ONE time point | Elements across ALL time points |
| **Temporal Structure** | Preserved | Collapsed |
| **Signature** | `span<In> → vector<Out>` | `range<Element> → Scalar` |
| **Use Case** | Ragged → non-ragged containers | Trial → sort key |
| **Example** | Sum 3 mask areas at t=100 → [6] at t=100 | Count all spikes in trial → 50 |

## API Reference

### GatherResult Methods

```cpp
template<typename T>
class GatherResult {
public:
    using element_type = WhiskerToolbox::Gather::element_type_of_t<T>;
    
    /// Build value store for a specific trial
    [[nodiscard]] PipelineValueStore buildTrialStore(size_type trial_idx) const;
    // Populates: "alignment_time", "trial_index", "trial_duration", "end_time"
    
    /// Project values across all trials using a store-based factory
    template<typename Value>
    auto projectV2(ValueProjectionFactoryV2<element_type, Value> const& factory) const;
    
    /// Apply reducer to all trials, returns vector of scalars
    template<typename Scalar>
    std::vector<Scalar> reduce(ReducerFactory<element_type, Scalar> const& factory) const;
    
    /// Sort trial indices by reduction result
    template<typename Scalar>
    std::vector<size_type> sortIndicesBy(
        ReducerFactory<element_type, Scalar> const& factory,
        bool ascending = true) const;
    
    /// Create reordered result
    [[nodiscard]] GatherResult reorder(std::vector<size_type> const& indices) const;
    
    /// Map reordered position to original trial index
    [[nodiscard]] size_type originalIndex(size_type reordered_idx) const;
};
```

### Value Projection Types

```cpp
// Store-based projection factory (preferred)
template<typename InElement, typename Value>
using ValueProjectionFactoryV2 = 
    std::function<ValueProjectionFn<InElement, Value>(PipelineValueStore const&)>;

// Bind a pipeline to produce a store-based factory
template<typename InElement, typename Value>
ValueProjectionFactoryV2<InElement, Value> bindValueProjectionV2(TransformPipeline const& pipeline);
```

### Range Reductions

#### Event Reductions

| Reduction | Output | Description |
|-----------|--------|-------------|
| `EventCount` | `int` | Number of events |
| `FirstPositiveLatency` | `float` | Time of first event with t > 0 |
| `LastNegativeLatency` | `float` | Time of last event with t < 0 |
| `EventCountInWindow` | `int` | Events within time window (parameterized) |
| `MeanInterEventInterval` | `float` | Mean interval between events |
| `EventTimeSpan` | `float` | Duration from first to last event |

#### Value Reductions

| Reduction | Output | Description |
|-----------|--------|-------------|
| `MaxValue` | `float` | Maximum value |
| `MinValue` | `float` | Minimum value |
| `MeanValue` | `float` | Mean value |
| `StdValue` | `float` | Standard deviation |
| `TimeOfMax` | `float` | Time at which max occurs |
| `TimeOfMin` | `float` | Time at which min occurs |
| `TimeOfThresholdCross` | `float` | First threshold crossing (parameterized) |
| `SumValue` | `float` | Sum of all values |
| `AreaUnderCurve` | `float` | Trapezoidal integration |

## Usage Example: Raster Plot

```cpp
// 1. Gather spikes by trials
auto gather = gatherDigitalEventsByIntervals(spikes, trials, time_frame);

// 2. Create normalization pipeline with store-based binding
TransformPipeline pipeline;
pipeline.addStep<NormalizeTimeParamsV2>("NormalizeTimeValue", {});
// NormalizeTimeParamsV2 has `alignment_time` field bound from store

// 3. Create projection factory that uses store
auto projection_factory = bindValueProjectionV2<TimeFrameIndex, float>(pipeline);

// 4. Project, reduce, and sort
auto reducer_factory = [](PipelineValueStore const& store) {
    return [](auto const& events) { return firstPositiveLatency(events); };
};

auto sorted_indices = gather.sortIndicesBy(reducer_factory, /*ascending=*/true);
auto sorted_gather = gather.reorder(sorted_indices);

// 5. Draw raster
for (size_t i = 0; i < sorted_gather.size(); ++i) {
    auto store = sorted_gather.buildTrialStore(i);
    auto projection = projection_factory(store);
    
    for (auto const& event : (*sorted_gather[i])->view()) {
        float norm_time = projection(event.time());
        draw_point(norm_time, i, event.id());
    }
}
```

## JSON Pipeline Configuration

```json
{
  "name": "Trial Raster Analysis",
  "steps": [{
    "transform": "NormalizeTimeValue",
    "params": {},
    "param_bindings": {
      "alignment_time": "alignment_time"
    }
  }],
  "range_reduction": {
    "reduction_name": "FirstPositiveLatency",
    "description": "Sort key for trials"
  }
}
```

## Key Design Principles

### Value Projection Pattern

Instead of creating composite types like `NormalizedEvent`, extract fundamental types and process them:

```cpp
// Good: Extract fundamental type, process, retain identity
for (auto const& event : trial_view) {
    TimeFrameIndex time = event.time();       // Extract fundamental type
    float norm_time = projection(time);        // Process in pipeline
    EntityId id = event.id();                  // Retain identity from source
    draw_point(norm_time, trial_row, id);
}
```

Benefits:
- `TimeFrameIndex` is in `ElementVariant` (no type explosion)
- Transforms work on fundamental types
- Caller explicitly manages identity tracking

### Store-Based Context

All trial context flows through `PipelineValueStore`:

```cpp
PipelineValueStore store = gather.buildTrialStore(i);
// Contains:
// - "alignment_time": int64_t (trial start)
// - "trial_index": int (trial number)
// - "trial_duration": int64_t (end - start)
// - "end_time": int64_t (trial end)
```

This replaces the old `TrialContext` injection pattern with generic key-value binding.

## File Locations

- **RangeReductionRegistry**: `core/RangeReductionRegistry.hpp`
- **Event Reductions**: `algorithms/RangeReductions/EventRangeReductions.hpp`
- **Value Reductions**: `algorithms/RangeReductions/ValueRangeReductions.hpp`
- **Value Projection Types**: `extension/ValueProjectionTypes.hpp`
- **Tests**: `tests/DataManager/TransformsV2/test_range_reductions.test.cpp`
