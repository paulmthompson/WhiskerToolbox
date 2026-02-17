# Pipeline Value Store

## Overview

The Pipeline Value Store is a generic key-value store that enables parameter binding from computed values. It replaces the old preprocessing and context injection systems with a unified pattern.

## Core Components

### PipelineValueStore

Runtime key-value store for pipeline intermediate values:

```cpp
#include "transforms/v2/core/PipelineValueStore.hpp"

PipelineValueStore store;

// Type-safe setters
store.set("mean", 0.5f);
store.set("count", 100);
store.set("alignment_time", static_cast<int64_t>(12345));
store.set("label", std::string("trial_1"));

// Typed getters
auto mean = store.getFloat("mean");        // std::optional<float>
auto count = store.getInt("count");        // std::optional<int64_t>

// JSON representation (for binding)
auto json = store.getJson("mean");         // std::optional<std::string>
```

### Pre-Reductions

Pre-reductions compute values before transform execution and populate the value store:

```cpp
TransformPipeline<AnalogTimeSeries, AnalogTimeSeries> pipeline;

// Add pre-reductions
pipeline.addPreReduction({"MeanValue", "computed_mean", {}});
pipeline.addPreReduction({"StdValue", "computed_std", {}});

// Execute - pre-reductions run first, then transforms use bound values
auto result = pipeline.execute(input);
```

### Parameter Bindings

Bindings map parameter fields to value store keys:

```cpp
// In code
pipeline.addStep<ZScoreParams>(
    "ZScoreNormalize",
    ZScoreParams{.clamp = true},
    {{"mean", "computed_mean"}, {"std_dev", "computed_std"}}  // bindings
);

// In JSON
{
  "steps": [{
    "transform": "ZScoreNormalize",
    "params": {"clamp": true},
    "param_bindings": {
      "mean": "computed_mean",
      "std_dev": "computed_std"
    }
  }]
}
```

## GatherResult Integration

For trial-aligned analysis, `GatherResult::buildTrialStore()` creates a value store with trial context:

```cpp
GatherResult<DigitalEventSeries> gather = ...;

for (size_t i = 0; i < gather.size(); ++i) {
    PipelineValueStore store = gather.buildTrialStore(i);
    // store contains:
    // - "alignment_time": int64_t (trial start)
    // - "trial_index": int (trial number)
    // - "trial_duration": int64_t (end - start)
    // - "end_time": int64_t (trial end)
    
    auto projection = projection_factory(store);
    // Use projection with trial-specific alignment...
}
```

## JSON Pipeline Format

Complete pipeline with pre-reductions and bindings:

```json
{
  "name": "ZScoreNormalization",
  "pre_reductions": [
    {"reduction": "MeanValue", "output_key": "computed_mean"},
    {"reduction": "StdValue", "output_key": "computed_std"}
  ],
  "steps": [{
    "transform": "ZScoreNormalizeV2",
    "params": {"clamp_outliers": true, "outlier_threshold": 3.0},
    "param_bindings": {
      "mean": "computed_mean",
      "std_dev": "computed_std"
    }
  }]
}
```

## File Locations

| Component | Location |
|-----------|----------|
| PipelineValueStore | `core/PipelineValueStore.hpp` |
| Parameter Binding | `extension/ParameterBinding.hpp` |
| Pipeline Step | `detail/PipelineStep.hpp` |
| Reduction Step | `detail/ReductionStep.hpp` |
| Tests | `tests/DataManager/TransformsV2/test_pipeline_value_store.test.cpp` |

## Migration from Old System

| Old Pattern | New Pattern |
|-------------|-------------|
| `preprocess()` method in params | Pre-reductions with value store |
| `setContext(TrialContext)` | `buildTrialStore()` + parameter bindings |
| `ContextInjectorRegistry` | (Removed) Use bindings instead |
| `PreprocessingRegistry` | (Removed) Use pre-reductions instead |
