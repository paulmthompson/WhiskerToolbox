# Transformation System V2

## Quick Start

This directory contains the next-generation transformation architecture for WhiskerToolbox. It provides:

- **Element-level transforms**: Pure functions operating on single data items
- **Lazy evaluation**: Range-based pipelines with zero intermediate allocations  
- **Composability**: Chain transforms with compile-time optimization
- **Runtime configuration**: Build pipelines via UI/JSON with type-safe execution
- **Provenance tracking**: Track EntityID relationships through transforms

## Status

✅ **Core Infrastructure Complete** ✅

This is a parallel implementation. The existing V1 system (`src/DataManager/transforms/`) continues to work.

**Implemented:**
- Element registry with compile-time type safety
- TypedParamExecutor (eliminates per-element overhead)
- Container automatic lifting and native container transforms
- Transform fusion pipeline with view-based lazy execution
- Ragged outputs, multi-input transforms, time-grouped transforms
- JSON pipeline configuration with reflect-cpp
- **PipelineValueStore** with pre-reductions and parameter bindings
- **RangeReductions** for trial-level analysis
- **GatherResult integration** for trial-aligned workflows
- Provenance tracking (LineageRegistry, EntityResolver)
- DataManager integration (`load_data_from_json_config_v2`)

**Coming Next:**
- UI Integration (Pipeline Builder)
- Migration of legacy transforms

## Directory Structure

```
v2/
├── algorithms/              # Transform implementations
│   ├── MaskArea/           # Mask2D → float
│   ├── SumReduction/       # Time-grouped reduction
│   ├── LineMinPointDist/   # Binary transform
│   ├── ZScoreNormalization/# Z-score with pre-reductions
│   ├── Temporal/           # Time normalization
│   └── RangeReductions/    # Event and value reductions
│
├── core/                   # Public API
│   ├── ElementRegistry.hpp
│   ├── TransformPipeline.hpp
│   ├── PipelineValueStore.hpp
│   ├── PipelineLoader.hpp
│   ├── DataManagerIntegration.hpp
│   └── RangeReductionRegistry.hpp
│
├── extension/              # Transform developer utilities
└── detail/                 # Pipeline internals
```

## Key Concepts

### Element vs Container Transforms

**Element Transform**: Operates on a single data item
```cpp
float calculateArea(Mask2D const& mask, AreaParams const& params);
```

**Lifted Container Transform**: Automatically generated
```cpp
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>(
    "CalculateArea", mask_data, params);
```

**Native Container Transform**: For temporal operations
```cpp
auto events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries>(
    "AnalogEventThreshold", analog_data, params);
```

### Fused Multi-Step Pipelines

```cpp
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<SkeletonParams>("Skeletonize", params1);
pipeline.addStep<MaskAreaParams>("CalculateArea", params2);

// Single pass, no intermediate allocations
auto areas = pipeline.execute(masks);
```

### Pre-Reductions and Parameter Bindings

For transforms requiring global statistics (e.g., Z-Score):

```json
{
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

### DataManager Integration

```cpp
#include "transforms/v2/core/DataManagerIntegration.hpp"

DataManager dm;
// ... populate dm with data ...

auto result = load_data_from_json_config_v2(&dm, json_config);
if (result.success) {
    auto areas = dm.getData<AnalogTimeSeries>("output_key");
}
```

### Multi-Input Pipelines (Binary Transforms)

```json
{
  "steps": [{
    "transform_name": "CalculateLineMinPointDistance",
    "input_key": "my_lines",
    "additional_input_keys": ["my_points"],
    "output_key": "distances"
  }]
}
```

## Usage Examples

### Register and Execute Transform

```cpp
#include "transforms/v2/core/ElementRegistry.hpp"

ElementRegistry& registry = ElementRegistry::instance();
registry.registerTransform<Mask2D, float, MaskAreaParams>(
    "CalculateArea", calculateMaskArea);

// Execute on single element
float area = registry.execute<Mask2D, float, MaskAreaParams>(
    "CalculateArea", mask, params);

// Execute on container (automatic lifting!)
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>(
    "CalculateArea", mask_data, params);
```

### Build Multi-Step Pipeline

```cpp
#include "transforms/v2/core/TransformPipeline.hpp"

TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<MaskAreaParams>("CalculateMaskArea", params);
auto result = pipeline.execute(mask_data);
```

### View-Based Lazy Execution

```cpp
auto view = pipeline.executeAsView(masks);
auto filtered = view 
    | std::views::filter([](auto p) { return p.second > 10.0f; });
```

## Migration from V1

**V1**: ~200+ lines of boilerplate per transform
```cpp
class MaskAreaOperation : public TransformOperation {
    // Manual iteration, container creation, JSON handling...
};
```

**V2**: ~30 lines total
```cpp
float calculateMaskArea(Mask2D const& mask, MaskAreaParams const&) {
    return static_cast<float>(mask.countNonZero());
}

registry.registerTransform<Mask2D, float, MaskAreaParams>(
    "CalculateMaskArea", calculateMaskArea);
```

## Testing

```bash
# Run all V2 transform tests
ctest --preset linux-clang-release -R "TransformsV2"
```

## Documentation

- **Architecture**: See [DESIGN.md](DESIGN.md)
- **Implementation Guide**: See [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)
- **Value Store Pattern**: See [PIPELINE_VALUE_STORE.md](PIPELINE_VALUE_STORE.md)
- **GatherResult Integration**: See [GATHER_PIPELINE_DESIGN.md](GATHER_PIPELINE_DESIGN.md)
- **Next Steps**: See [NEXT_STEPS.md](NEXT_STEPS.md)
