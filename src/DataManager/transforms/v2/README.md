# Transformation System V2

## Quick Start

This directory contains the next-generation transformation architecture for WhiskerToolbox. It provides:

- **Element-level transforms**: Pure functions operating on single data items
- **Lazy evaluation**: Range-based pipelines with zero intermediate allocations  
- **Composability**: Chain transforms together with compile-time optimization
- **Runtime configuration**: Build pipelines via UI/JSON with type-safe execution
- **Provenance tracking**: Track EntityID relationships through transforms

## Status

✅ **Phase 3 Complete: Core Infrastructure & View Pipelines Operational** ✅

This is a parallel implementation. The existing transformation system (`src/DataManager/transforms/`) continues to work. This new system provides:

**Currently Implemented:**
- ✅ Element registry with compile-time type safety
- ✅ TypedParamExecutor system (eliminates per-element parameter casts)
- ✅ Container automatic lifting (element → container transforms)
- ✅ **Native Container Transforms** (direct container-to-container operations)
- ✅ Transform fusion pipeline (minimizes intermediate allocations)
- ✅ **View-based lazy pipelines** (zero intermediate allocations, pull-based)
- ✅ Support for ragged outputs (variable-length per time frame)
- ✅ Multi-input transforms (N inputs → 1 output)
- ✅ Time-grouped transforms (span<Element> per time)
- ✅ Runtime JSON pipeline configuration with reflect-cpp
- ✅ **Provenance Tracking** (LineageRegistry, EntityResolver, LineageRecorder)
- ✅ **DataManager Integration** (`load_data_from_json_config_v2`, V1-compatible JSON format)

**Coming Next:**
- 🔄 UI Integration (Pipeline Builder)
- 🔄 Migration of legacy transforms

## Directory Structure

```
v2/
├── algorithms/                  # ✅ Concrete transform implementations
│   ├── MaskArea/               # Mask2D → float
│   ├── SumReduction/           # float → float (reduction)
│   └── LineMinPointDist/       # (Line2D, Point2D) → float
│
├── core/                        # ✅ Core infrastructure
│   ├── ElementTransform.hpp    # Transform function wrappers
│   ├── ContainerTraits.hpp     # Element ↔ Container type mappings
│   ├── ElementRegistry.hpp     # Main registry with TypedParamExecutor
│   ├── TransformPipeline.hpp   # Multi-step pipeline with fusion
│   ├── ContainerTransform.hpp  # Container lifting utilities
│   ├── ParameterIO.hpp         # reflect-cpp parameter serialization
│   ├── PipelineLoader.hpp      # JSON pipeline loading
│   ├── DataManagerIntegration.hpp # DataManager pipeline executor
│   └── RegisteredTransforms.hpp # Central registration
│
├── DESIGN.md                    # Architecture goals and future plans
├── IMPLEMENTATION_GUIDE.md      # Usage guide
└── README.md                    # This file
```

## Key Concepts

### Element vs Container Transforms

**Element Transform**: Operates on a single data item
```cpp
// Element-level: Mask2D → float
float calculateArea(Mask2D const& mask, AreaParams const& params);
```

**Lifted Container Transform**: Operates on collection (automatically generated)
```cpp
// Container-level: MaskData → AnalogTimeSeries
// Automatically generated from element transform!
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>(
    "CalculateArea", mask_data, params);
```

**Native Container Transform**: Operates directly on collection (manually implemented)
```cpp
// Container-level: AnalogTimeSeries → DigitalEventSeries
// Implemented directly to handle temporal dependencies (e.g. thresholds with lockout)
auto events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries>(
    "AnalogEventThreshold", analog_data, params);
```

### Fused Multi-Step Pipelines

```cpp
// Instead of manual chaining with intermediate allocations:
auto skel = registry.executeContainer<MaskData, MaskData>(
    "Skeletonize", masks, params1);
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>(
    "CalculateArea", skel, params2);

// Use pipeline for fused execution (single pass):
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<SkeletonParams>("Skeletonize", params1);
pipeline.addStep<MaskAreaParams>("CalculateArea", params2);

// Executes both transforms in one pass (no intermediate MaskData)
auto areas = pipeline.execute(masks);

// Dynamic Output (Single Template):
// Useful when output type is not known at compile time
TransformPipeline<MaskData> dynamic_pipeline;
dynamic_pipeline.addStep<SkeletonParams>("Skeletonize", params1);
dynamic_pipeline.addStep<MaskAreaParams>("CalculateArea", params2);
DataTypeVariant result = dynamic_pipeline.execute(masks);

// Benefits:
// - Single iteration over input data
// - No intermediate container allocations
// - Better cache locality
// - Reduced memory usage
```

### Multi-Pass Preprocessing

Some element-level transforms require global statistics (e.g., Z-Score normalization requires mean and std dev). The system supports a **preprocessing phase** where transforms can make a first pass over the data to compute and cache these statistics.

```cpp
// 1. Define params with cached state (skipped by serialization)
struct ZScoreParams {
    // User config
    bool clamp_outliers = false;
    
    // Cached state (computed during preprocessing)
    rfl::Skip<std::optional<float>> mean;
    rfl::Skip<std::optional<float>> std;
    
    // Preprocess method (automatically called by pipeline)
    template<typename View>
    void preprocess(View view) {
        // Compute mean/std from view...
        mean = computed_mean;
        std = computed_std;
    }
};

// 2. Transform uses cached values
float zScore(float val, ZScoreParams const& params) {
    return (val - params.mean.value()) / params.std.value();
}

// The pipeline automatically detects the preprocess() method and calls it
// before the main execution loop. This enables multi-pass algorithms 
// without materializing intermediate containers.
```

### Runtime Configuration with DataManager Integration

The system supports runtime pipeline configuration via JSON with full DataManager integration:

```cpp
#include "transforms/v2/core/DataManagerIntegration.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Option 1: Use DataManagerPipelineExecutor directly
DataManager dm;
// ... populate dm with data ...

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

// Option 2: Use load_data_from_json_config_v2 (V1-compatible interface)
// JSON file format matches V1 exactly:
// [
//   {
//     "transformations": {
//       "metadata": { "name": "My Pipeline" },
//       "steps": [
//         {
//           "step_id": "1",
//           "transform_name": "CalculateMaskArea",
//           "input_key": "mask_data",
//           "output_key": "areas",
//           "parameters": {}
//         }
//       ]
//     }
//   }
// ]
auto data_info = load_data_from_json_config_v2(&dm, "pipeline.json");
// Results are stored in DataManager, accessible via output_key
auto areas = dm.getData<RaggedAnalogTimeSeries>("areas");
```

## Usage Examples

### Example 1: Register and Execute Element Transform

```cpp
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Register element transform
ElementRegistry registry;
registry.registerTransform<Mask2D, float, MaskAreaParams>(
    "CalculateArea",
    calculateMaskArea
);

// Execute on single element
Mask2D mask = ...;
MaskAreaParams params{};
float area = registry.execute<Mask2D, float, MaskAreaParams>(
    "CalculateArea", mask, params);

// Or execute on container (automatic lifting!)
MaskData mask_data = ...;
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>(
    "CalculateArea", mask_data, params);
```

### Example 2: Build Multi-Step Pipeline

```cpp
#include "transforms/v2/core/TransformPipeline.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Create multi-step pipeline
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;

// Add steps (executes in single fused pass)
MaskAreaParams params1{};
pipeline.addStep<MaskAreaParams>("CalculateMaskArea", params1);

// Execute (all steps fused into single iteration)
auto result = pipeline.execute(mask_data);
```

### Example 3: Ragged Output Transform

```cpp
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Register transform with ragged output
ElementRegistry registry;
registry.registerTransform<Mask2D, std::vector<float>, MaskAreaParams>(
    "CalculateMaskArea",
    calculateMaskArea  // Returns std::vector<float> per mask
);

// Execute on container → RaggedAnalogTimeSeries
MaskData mask_data = ...;
MaskAreaParams params{};
auto ragged_areas = registry.executeContainer<MaskData, RaggedAnalogTimeSeries>(
    "CalculateMaskArea", mask_data, params);

// Each time frame has variable-length data
for (const auto& frame_data : ragged_areas.elements()) {
    // frame_data is std::vector<float> with variable length
    std::cout << "Frame has " << frame_data.size() << " contours\n";
}
```

### Example 4: Multi-Input Transforms

```cpp
#include "transforms/v2/core/ElementRegistry.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Define a multi-input transform (e.g., intersection)
struct IntersectionParams {};

Mask2D intersectMasks(
    std::tuple<Mask2D, Mask2D> const& inputs,
    IntersectionParams const& params) {
    auto const& [mask1, mask2] = inputs;
    // Compute intersection...
    return result;
}

// Register as multi-input transform
registry.registerMultiInputTransform<
    std::tuple<Mask2D, Mask2D>,
    Mask2D,
    IntersectionParams>(
    "IntersectMasks",
    intersectMasks
);
```

### Example 5: Runtime Multi-Input Pipeline (Binary Transforms via JSON)

For multi-input (binary, ternary, etc.) transforms at runtime, use `additional_input_keys` to specify secondary inputs:

```cpp
#include "transforms/v2/core/DataManagerIntegration.hpp"

using namespace WhiskerToolbox::Transforms::V2;

DataManager dm;
// ... populate dm with LineData at "my_lines" and PointData at "my_points" ...

DataManagerPipelineExecutor executor(&dm);
executor.loadFromJson(R"({
    "steps": [
        {
            "step_id": "1",
            "transform_name": "CalculateLineMinPointDistance",
            "input_key": "my_lines",
            "additional_input_keys": ["my_points"],
            "output_key": "distances",
            "parameters": {}
        },
        {
            "step_id": "2",
            "transform_name": "SumReduction",
            "input_key": "distances",
            "output_key": "total_distances",
            "parameters": {}
        },
        {
            "step_id": "3",
            "transform_name": "AnalogEventThreshold",
            "input_key": "total_distances",
            "output_key": "threshold_events",
            "parameters": {
                "threshold": 5.0,
                "direction": "above"
            }
        }
    ]
})");

auto result = executor.execute();
if (result.success) {
    // "distances" contains RaggedAnalogTimeSeries from multi-input transform
    // "total_distances" contains AnalogTimeSeries (sum of distances)
    // "threshold_events" contains DigitalEventSeries
    auto events = dm.getData<DigitalEventSeries>("threshold_events");
}
```

**Key Points for Multi-Input Pipelines:**
- `input_key` specifies the primary input (first element of tuple)
- `additional_input_keys` is an array of secondary inputs (rest of tuple)
- All inputs are zip-iterated at runtime using `FlatZipView`
- Binary transforms (2 inputs) use tuple: `(primary, secondary)`
- Steps can be chained after multi-input transforms using standard `input_key`

**JSON Schema for Multi-Input Step:**
```json
{
    "step_id": "string (unique identifier)",
    "transform_name": "string (registered transform name)",
    "input_key": "string (primary input DataManager key)",
    "additional_input_keys": ["string", "..."],  // Optional: secondary inputs
    "output_key": "string (output DataManager key)",
    "parameters": { }
}
```

### Example 6: Pipeline Fusion with Multi-Input Transforms

The executor automatically analyzes pipelines and groups fusible steps into segments:

```cpp
DataManagerPipelineExecutor executor(&dm);
executor.loadFromJson(config);

// Analyze fusion opportunities
auto segments = executor.buildSegments();

for (auto const& segment : segments) {
    std::cout << "Segment: steps " << segment.start_step 
              << " to " << segment.end_step << "\n";
    std::cout << "  Multi-input: " << segment.is_multi_input << "\n";
    std::cout << "  Requires materialization: " 
              << segment.requires_materialization << "\n";
}

// Fusion rules:
// - Element-wise transforms with chained outputs are fused
// - Multi-input transforms start new segments
// - Container transforms (like AnalogEventThreshold) require materialization
```

## Building

This module is part of the DataManager library and built with CMake:

```bash
# V2 transforms are automatically included when building DataManager
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release

# Run tests
ctest --preset linux-clang-release -R "TransformsV2"
```

## Migration Guide

### Porting a Transform from V1 to V2

**Old system** (`transforms/Masks/Mask_Area/`):
```cpp
class MaskAreaOperation : public TransformOperation {
    DataTypeVariant execute(DataTypeVariant const& input, ...) override {
        auto mask_data = std::get<std::shared_ptr<MaskData>>(input);
        auto result = std::make_shared<AnalogTimeSeries>();
        
        // Manual iteration and container creation
        for (auto const& [time, masks] : mask_data->getData()) {
            for (auto const& mask : masks) {
                float area = calculateArea(mask);
                result->addValue(time, area);
            }
        }
        
        return result;
    }
};
```

**New system** (`algorithms/MaskArea/MaskArea.hpp`):
```cpp
// Just the element-level computation!
float calculateMaskArea(Mask2D const& mask, MaskAreaParams const& params) {
    return mask.countNonZero();  // Or whatever computation
}

// Registration (one line):
registry.registerTransform<Mask2D, float, MaskAreaParams>(
    "CalculateArea", calculateMaskArea);

// Container handling is automatic!
```

### Benefits of Migration

1. **Less code**: No manual container iteration
2. **Better performance**: Lazy evaluation + compiler optimization
3. **Composability**: Easily chain transforms
4. **Testability**: Element functions are pure and simple to test
5. **Maintainability**: Separation of concerns

## Testing

Tests are located in `tests/DataManager/TransformsV2/`:

```bash
# Run all V2 transform tests
ctest --preset linux-clang-release -R "TransformsV2"

# Run specific test
./out/build/Clang/Release/tests/DataManager/TransformsV2/test_element_transforms
```

Key test files:
- `ElementTransform.test.cpp` - Core transform infrastructure
- `RangePipeline.test.cpp` - Pipeline composition
- `LazyEvaluation.test.cpp` - Correctness of lazy evaluation
- `Performance.benchmark.cpp` - Performance comparison with V1

## Documentation

- **Architecture**: See `DESIGN.md` for complete system design
- **API Reference**: Doxygen comments in header files
- **Examples**: Working code in `algorithms/` directory
- **Migration**: Step-by-step guide in this README

## Roadmap

### Phase 1: Core Infrastructure ✅ COMPLETE
- [x] Element transform concepts and wrappers
- [x] Container trait system  
- [x] Design documentation
- [x] TypedParamExecutor system (eliminates per-element overhead)

### Phase 2: Registry and Execution ✅ COMPLETE
- [x] Element registry implementation
- [x] TypedParamExecutor with cached executors
- [x] Multi-step pipeline with fusion
- [x] Container automatic lifting
- [x] Ragged output support
- [x] Multi-input transform support
- [x] Time-grouped transform support
- [x] Basic tests (MaskArea example)
- [x] Example transforms (MaskArea, SumReduction)

### Phase 3: Runtime Configuration ✅ COMPLETE
- [x] JSON schema for pipeline descriptors
- [x] reflect-cpp parameter serialization
- [x] Pipeline factory (runtime type dispatch)
- [x] Type-erased executor interface
- [ ] UI integration for pipeline builder (In Progress)

### Phase 4: Advanced Features (Future)
- [x] View-based lazy pipelines (zero intermediate allocations)
- [ ] C++20 ranges for true lazy evaluation (deferred)
- [ ] Lazy storage strategy (delay materialization)
- [ ] Provenance tracking (EntityID relationships)
- [ ] Parallel execution support (thread pool)
- [ ] Performance benchmarking vs V1

### Phase 5: Migration
- [ ] Port 10 transforms to V2
- [ ] Side-by-side UI support
- [ ] User testing and feedback
- [ ] Full migration of transform library

## Contributing

When adding new transforms to V2:

1. **Create element-level function**: Pure, testable computation
2. **Define parameters**: Use reflect-cpp for serialization
3. **Register transform**: One line in registry
4. **Add tests**: Unit tests for element function
5. **Document**: Doxygen comments + example usage

Example template:
```cpp
// MyTransform.hpp
struct MyTransformParams {
    float threshold = 0.5f;
    int iterations = 10;
};

REFLECTCPP_STRUCT(MyTransformParams, threshold, iterations);

OutputType myTransform(InputType const& input, 
                      MyTransformParams const& params) {
    // Your computation here
}

// In registration code:
registry.registerTransform<InputType, OutputType, MyTransformParams>(
    "MyTransform", myTransform);
```

## Questions?

- Architecture questions: See `DESIGN.md`
- Implementation questions: Check header comments
- Migration questions: See examples in `algorithms/`
- Performance questions: Run benchmarks in `tests/`

## License

Same as parent WhiskerToolbox project.
