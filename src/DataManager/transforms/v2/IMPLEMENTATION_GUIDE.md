# Transformation System V2 - Implementation Guide

## Overview

This guide explains how to use and extend the V2 transformation system. It covers:

1. Creating new transforms
2. Using the registry
3. Building pipelines
4. Runtime configuration (JSON)
5. DataManager integration
6. Parameter serialization

## Quick Reference

### Register a Transform

```cpp
#include "transforms/v2/core/ElementRegistry.hpp"

// 1. Define parameters (using reflect-cpp)
struct MyParams {
    std::optional<float> threshold;
    std::optional<int> iterations;
    
    float getThreshold() const { return threshold.value_or(0.5f); }
    int getIterations() const { return iterations.value_or(10); }
};

// 2. Implement element-level function
float myTransform(Mask2D const& input, MyParams const& params) {
    // Your computation here
    return result;
}

// 3. Register
ElementRegistry& registry = ElementRegistry::instance();
TransformMetadata meta;
meta.description = "My transform does...";
meta.category = "Image Processing";

registry.registerTransform<Mask2D, float, MyParams>("MyTransform", myTransform, meta);

// 4. Use it!
Mask2D mask = ...;
MyParams params;
float result = registry.execute<Mask2D, float, MyParams>("MyTransform", mask, params);

// Or on containers (automatic!)
MaskData masks = ...;
auto results = registry.executeContainer<MaskData, AnalogTimeSeries, MyParams>(
    "MyTransform", masks, params);
```

## Part 1: Creating Transforms

### Element Function Patterns

#### Pattern 1: Simple Stateless Transform

```cpp
Line2D skeletonize(Mask2D const& mask) {
    // Computation
    return result;
}

registry.registerTransform<Mask2D, Line2D>("Skeletonize", skeletonize);
```

#### Pattern 2: Parameterized Transform

```cpp
struct SkeletonParams {
    std::string method = "zhang-suen";
    int max_iterations = 100;
};

Mask2D skeletonize(Mask2D const& mask, SkeletonParams const& params) {
    if (params.method == "zhang-suen") {
        return zhangSuenThin(mask, params.max_iterations);
    }
    // ...
}

registry.registerTransform<Mask2D, Mask2D, SkeletonParams>("Skeletonize", skeletonize);
```

#### Pattern 3: Native Container Transform

For operations that need the entire container (temporal dependencies, global stats):

```cpp
std::shared_ptr<DigitalEventSeries> analogEventThreshold(
    AnalogTimeSeries const& input,
    AnalogEventThresholdParams const& params,
    ComputeContext const& ctx) {
    
    auto result = std::make_shared<DigitalEventSeries>();
    // Process entire time series with temporal logic
    return result;
}

registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, 
    AnalogEventThresholdParams>("AnalogEventThreshold", analogEventThreshold);
```

#### Pattern 4: Multi-Input Transform

```cpp
float lineMinPointDist(
    std::tuple<Line2D, Point2D<float>> const& inputs,
    LineMinPointDistParams const& params) {
    auto const& [line, point] = inputs;
    // Compute minimum distance
    return result;
}

registry.registerMultiInputTransform<
    std::tuple<Line2D, Point2D<float>>,
    float,
    LineMinPointDistParams>("LineMinPointDist", lineMinPointDist);
```

#### Pattern 5: Transform with Pre-Reductions (Z-Score Pattern)

For transforms requiring global statistics, use pre-reductions with parameter bindings:

```cpp
// Parameters with bindable fields (no preprocessing methods needed)
struct ZScoreNormalizationParamsV2 {
    float mean = 0.0f;           // Bound from "computed_mean"
    float std_dev = 1.0f;        // Bound from "computed_std"
    bool clamp_outliers = false;
    float outlier_threshold = 3.0f;
};

float zScoreNormalizationV2(float value, ZScoreNormalizationParamsV2 const& params) {
    float z = (value - params.mean) / (params.std_dev + 1e-8f);
    if (params.clamp_outliers) {
        z = std::clamp(z, -params.outlier_threshold, params.outlier_threshold);
    }
    return z;
}
```

Pipeline JSON that uses pre-reductions:

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

### Parameter Best Practices

#### Use reflect-cpp for Serialization

```cpp
struct MyParams {
    float threshold = 0.5f;
    int window_size = 5;
    std::string mode = "auto";
};
// No manual toJson/fromJson needed - reflect-cpp handles it automatically
```

#### Validate Using reflect-cpp

```cpp
#include <rfl.hpp>

struct FilterParams {
    rfl::Validator<int, rfl::Minimum<1>, rfl::Maximum<100>> kernel_size = 3;
    rfl::Validator<float, rfl::Minimum<0.0f>> sigma = 1.0f;
};
// Validation happens automatically during JSON deserialization
```

### Metadata for UI Generation

```cpp
TransformMetadata meta;
meta.description = "Skeletonize binary mask using Zhang-Suen algorithm";
meta.category = "Image Processing / Morphology";
meta.is_expensive = true;      // Enables parallelization hints
meta.is_deterministic = true;  // Same input → same output

registry.registerTransform<Mask2D, Line2D, SkeletonParams>(
    "Skeletonize", skeletonize, meta);
```

## Part 2: Using the Registry

### Querying Available Transforms

```cpp
ElementRegistry& registry = ElementRegistry::instance();

// What transforms can I apply to Mask2D?
auto mask_transforms = registry.getTransformsForInputType(typeid(Mask2D));

// What transforms produce Line2D?
auto line_producers = registry.getTransformsForOutputType(typeid(Line2D));
```

### Executing Transforms

```cpp
// Single element
auto skeleton = registry.execute<Mask2D, Mask2D, SkeletonParams>(
    "Skeletonize", mask, params);

// Container (automatic lifting)
auto skeletons = registry.executeContainer<MaskData, MaskData, SkeletonParams>(
    "Skeletonize", masks, params);

// Native container transform
auto events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, 
    ThresholdParams>("AnalogEventThreshold", analog, params);
```

## Part 3: Building Pipelines

### Fused Pipeline Execution

```cpp
// Typed Pipeline (compile-time output type)
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<SkeletonParams>("Skeletonize", params1);
pipeline.addStep<MaskAreaParams>("CalculateMaskArea", params2);

auto angles = pipeline.execute(masks);

// Dynamic Pipeline (runtime output type)
TransformPipeline<MaskData> dynamic_pipeline;
dynamic_pipeline.addStep<SkeletonParams>("Skeletonize", params1);
dynamic_pipeline.addStep<MaskAreaParams>("CalculateMaskArea", params2);

DataTypeVariant result = dynamic_pipeline.execute(masks);
```

### Pipelines with Pre-Reductions

```cpp
TransformPipeline<AnalogTimeSeries, AnalogTimeSeries> pipeline;

// Add pre-reductions that populate PipelineValueStore
pipeline.addPreReduction({"MeanValue", "computed_mean", {}});
pipeline.addPreReduction({"StdValue", "computed_std", {}});

// Add step with parameter bindings
pipeline.addStep<ZScoreNormalizationParamsV2>(
    "ZScoreNormalizeV2", 
    ZScoreNormalizationParamsV2{.clamp_outliers = true},
    {{"mean", "computed_mean"}, {"std_dev", "computed_std"}}  // bindings
);

auto normalized = pipeline.execute(analog_data);
```

## Part 4: Runtime Configuration (JSON)

### JSON Pipeline Format

```json
{
  "name": "Whisker Angle Analysis",
  "description": "Extract whisker angles from segmentation masks",
  
  "pre_reductions": [
    {"reduction": "MeanValue", "output_key": "mean_area"}
  ],
  
  "steps": [
    {
      "transform": "Skeletonize",
      "params": {"method": "zhang-suen", "max_iterations": 100}
    },
    {
      "transform": "MaskToLine",
      "params": {"method": "centerline"},
      "param_bindings": {"normalize_to": "mean_area"}
    },
    {
      "transform": "LineAngle",
      "params": {"reference_point": "base", "angle_units": "degrees"}
    }
  ],
  
  "range_reduction": {
    "reduction_name": "MeanValue",
    "description": "Average angle across time"
  }
}
```

### Loading and Executing

```cpp
#include "transforms/v2/core/PipelineLoader.hpp"

auto pipeline = loadPipelineFromJson(json_string);
auto result = pipeline.execute(input_data);
```

## Part 5: DataManager Integration

### Using DataManagerPipelineExecutor

```cpp
#include "transforms/v2/core/DataManagerIntegration.hpp"

DataManager dm;
// ... populate dm with input data ...

DataManagerPipelineExecutor executor(&dm);
executor.loadFromJson(R"({
    "steps": [{
        "step_id": "1",
        "transform_name": "CalculateMaskArea",
        "input_key": "mask_data",
        "output_key": "areas",
        "parameters": {}
    }]
})");

auto result = executor.execute();
if (result.success) {
    auto areas = dm.getData<AnalogTimeSeries>("areas");
}
```

### V1-Compatible Interface

```cpp
// Use load_data_from_json_config_v2 for V1-compatible JSON format
auto result = load_data_from_json_config_v2(&dm, json_config);
```

### Multi-Input Pipelines

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

## Part 6: Migration from V1

### Before (V1 System)

```cpp
class MaskAreaOperation : public TransformOperation {
    DataTypeVariant execute(DataTypeVariant const& input, ...) override {
        auto mask_data = std::get<std::shared_ptr<MaskData>>(input);
        auto result = std::make_shared<AnalogTimeSeries>();
        
        // Manual iteration
        for (auto const& [time, masks] : mask_data->getData()) {
            for (auto const& mask : masks) {
                float area = calculateArea(mask);
                result->addValue(time, area);
            }
        }
        return result;
    }
};
// ~200+ lines of boilerplate
```

### After (V2 System)

```cpp
struct MaskAreaParams {};

float calculateMaskArea(Mask2D const& mask, MaskAreaParams const&) {
    return static_cast<float>(mask.countNonZero());
}

registry.registerTransform<Mask2D, float, MaskAreaParams>(
    "CalculateMaskArea", calculateMaskArea);
// ~30 lines total
```

## Testing Transforms

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("MaskArea - Empty mask", "[transforms][v2]") {
    Mask2D empty_mask(10, std::vector<uint8_t>(10, 0));
    MaskAreaParams params;
    
    float area = calculateMaskArea(empty_mask, params);
    
    REQUIRE(area == 0.0f);
}

TEST_CASE("Registry - Execute on container", "[transforms][v2][integration]") {
    ElementRegistry& registry = ElementRegistry::instance();
    
    MaskData masks;
    // ... populate with test masks ...
    
    MaskAreaParams params;
    auto areas = registry.executeContainer<MaskData, AnalogTimeSeries, MaskAreaParams>(
        "CalculateMaskArea", masks, params);
    
    REQUIRE(areas->getNumSamples() == masks.getTotalElementCount());
}
```

## Next Steps

1. **Read DESIGN.md**: Understand overall architecture
2. **Study algorithms/**: See complete working code
3. **Try migrating one transform**: Start with something simple
4. **Add tests**: Verify correctness
