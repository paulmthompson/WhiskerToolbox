# Transformation System V2 Architecture

## Overview

This document describes the V2 transformation architecture for WhiskerToolbox. The system provides a unified, composable approach to data transformations with compile-time type safety and runtime configurability.

## Implementation Status

### ✅ Complete
- **ElementRegistry**: Compile-time typed transform registry with TypedParamExecutor
- **TransformPipeline**: Multi-step pipeline with execution fusion and view-based lazy evaluation
- **Container Transforms**: Automatic lifting (element→container) and native container-to-container transforms
- **ContainerTraits**: Type mapping system (Mask2D ↔ MaskData, etc.)
- **Advanced Features**: Ragged outputs, multi-input transforms (binary via tuple), time-grouped transforms
- **reflect-cpp Integration**: Parameter serialization with automatic validation
- **JSON Pipeline Loading**: PipelineDescriptor, PipelineLoader with full validation
- **PipelineValueStore**: Generic key-value store for computed values with parameter bindings
- **Pre-Reductions**: Statistical computations (Mean, StdDev, Min, Max) that populate value store
- **GatherResult Integration**: Trial-aligned analysis with `buildTrialStore()` and value projections
- **RangeReductions**: Library of reductions for trial-level analysis (EventCount, FirstPositiveLatency, etc.)
- **Provenance Tracking**: `LineageRegistry`, `EntityResolver`, `LineageRecorder`
- **DataManager Integration**: `DataManagerPipelineExecutor` and `load_data_from_json_config_v2()`

### 🔄 Planned
- **UI Integration**: Pipeline Builder Widget
- **Parallel Execution**: Multi-threaded transform execution

## Core Architecture

### Type Traits Layer

Fundamental type properties are defined at `src/DataManager/TypeTraits/`. Each container type defines its own `DataTraits` nested struct:

```cpp
class MaskData : public RaggedTimeSeries<Mask2D> {
public:
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<MaskData, Mask2D> {
        static constexpr bool is_ragged = true;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = true;
    };
};

// Usage:
static_assert(is_ragged_v<MaskData>);
using ElementType = element_type_t<MaskData>;  // Mask2D
```

### Element-Level Transforms

Pure functions operating on single data elements:

```cpp
float calculateMaskArea(Mask2D const& mask, MaskAreaParams const& params);
```

Properties: stateless, type-safe, composable, testable in isolation.

### Container Transforms

**Automatic Lifting**: Element transforms automatically work on containers:

```cpp
registry.registerTransform<Mask2D, float, MaskAreaParams>("CalculateMaskArea", calculateMaskArea);

// Automatically available at container level:
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>("CalculateMaskArea", masks, params);
```

**Native Container Transforms**: For operations requiring full context (temporal dependencies, global stats):

```cpp
registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, ThresholdParams>(
    "AnalogEventThreshold", analogEventThreshold);
```

### Pipeline Execution Modes

**Fused Execution (Eager)**: Single pass, materializes final result immediately.

```cpp
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<SkeletonParams>("Skeletonize", p1);
pipeline.addStep<MaskAreaParams>("CalculateArea", p2);
auto areas = pipeline.execute(masks);  // Single pass, no intermediate allocations
```

**View-Based Execution (Lazy)**: Zero intermediate allocations, pull-based.

```cpp
auto view = pipeline.executeAsView(masks);
auto filtered = view | std::views::filter([](auto p) { return p.second > 10.0f; });
```

**Dynamic Output**: Runtime-determined output type via `DataTypeVariant`.

```cpp
TransformPipeline<MaskData> pipeline;
DataTypeVariant result = pipeline.execute(masks);
```

### Pre-Reductions and Parameter Bindings

For transforms requiring global statistics (e.g., Z-Score normalization), use pre-reductions to compute values and bind them to parameters:

```json
{
  "name": "ZScoreNormalization",
  "pre_reductions": [
    {"reduction": "MeanValue", "output_key": "computed_mean"},
    {"reduction": "StdValue", "output_key": "computed_std"}
  ],
  "steps": [{
    "transform": "ZScoreNormalizeV2",
    "params": {"clamp_outliers": true},
    "param_bindings": {"mean": "computed_mean", "std_dev": "computed_std"}
  }]
}
```

The `PipelineValueStore` holds computed values, and bindings inject them into transform parameters.

### Range Reductions

For trial-level analysis, range reductions collapse entire ranges to scalars:

| Reduction | Input | Output | Description |
|-----------|-------|--------|-------------|
| `EventCount` | `EventWithId` | `int` | Number of events |
| `FirstPositiveLatency` | `EventWithId` | `float` | Time of first event with t > 0 |
| `MeanValue` | `TimeValuePoint` | `float` | Mean value |
| `StdValue` | `TimeValuePoint` | `float` | Standard deviation |

### GatherResult Integration

For trial-aligned analysis, `GatherResult` provides:

```cpp
// Build value store for a trial
PipelineValueStore store = gather.buildTrialStore(trial_idx);
// Contains: "alignment_time", "trial_index", "trial_duration", "end_time"

// Project values across trials
auto factory = bindValueProjectionWithContext<TimeFrameIndex, float>(pipeline);
auto projections = gather.project(factory);

// Reduce and sort trials
auto latencies = gather.reduce(reducer_factory);
auto sorted_indices = gather.sortIndicesBy(reducer_factory, /*ascending=*/true);
auto reordered = gather.reorder(sorted_indices);
```

### Provenance Tracking

Track EntityID relationships through transforms:

```cpp
auto resolver = data_manager.getEntityResolver();
auto source_entities = resolver.resolveToRoot("mask_areas", time_index);
```

## Directory Structure

```
v2/
├── algorithms/              # Concrete transform implementations
│   ├── MaskArea/           # Mask2D → float
│   ├── SumReduction/       # Time-grouped reduction
│   ├── LineMinPointDist/   # Binary transform (Line2D, Point2D) → float
│   ├── ZScoreNormalization/# Z-score with pre-reductions
│   ├── Temporal/           # Time normalization transforms
│   └── RangeReductions/    # Event and value range reductions
│
├── core/                   # Public API
│   ├── ElementTransform.hpp
│   ├── ElementRegistry.hpp
│   ├── TransformPipeline.hpp
│   ├── PipelineValueStore.hpp
│   ├── PipelineLoader.hpp
│   ├── DataManagerIntegration.hpp
│   └── RangeReductionRegistry.hpp
│
├── extension/              # Transform developer utilities
│   ├── ParameterBinding.hpp
│   └── ValueProjectionTypes.hpp
│
└── detail/                 # Pipeline execution internals
    ├── PipelineStep.hpp
    └── ReductionStep.hpp
```

## Key Design Decisions

### Why Ranges Instead of Virtual Functions?

Ranges (compile-time dispatch) provide:
- Zero overhead when type is known
- Compiler can fuse multiple transforms
- Cache-friendly execution (element processed through full pipeline)
- Type erasure only at boundaries for runtime configuration

### When to Materialize vs Keep Lazy?

**Materialize**: User saves to DataManager, transform requires multiple passes, data needed by multiple consumers.

**Keep Lazy**: Intermediate pipeline steps, single-pass consumption, memory constraints, interactive visualization.

## Performance Characteristics

- Parameter cast: Once per pipeline (not per element)
- Type dispatch: Once per pipeline (not per element)
- Per-element overhead: ~2 virtual calls + 2 variant visits (input/output only)
- Fusion: Multiple transforms execute in single pass (no intermediate containers)
- View Mode: Pull-based execution, zero intermediate allocations

## Next Steps

1. **UI Integration**: Pipeline Builder widget, auto-generated parameter forms
2. **Transform Migration**: Port more V1 transforms to V2
3. **Parallel Execution**: Thread pool for expensive transforms

## References

- [reflect-cpp](https://github.com/getml/reflect-cpp)
- [C++20 Ranges](https://en.cppreference.com/w/cpp/ranges)

