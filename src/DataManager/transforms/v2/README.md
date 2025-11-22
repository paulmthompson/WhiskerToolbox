# Transformation System V2

## Quick Start

This directory contains the next-generation transformation architecture for WhiskerToolbox. It provides:

- **Element-level transforms**: Pure functions operating on single data items
- **Lazy evaluation**: Range-based pipelines with zero intermediate allocations  
- **Composability**: Chain transforms together with compile-time optimization
- **Runtime configuration**: Build pipelines via UI/JSON with type-safe execution
- **Provenance tracking**: Track EntityID relationships through transforms

## Status

🚧 **Under Active Development** 🚧

This is a parallel implementation. The existing transformation system (`src/DataManager/transforms/`) continues to work. This new system will gradually replace it as transforms are migrated.

## Directory Structure

```
v2/
├── DESIGN.md                    # Complete architecture documentation
├── README.md                    # This file
├── CMakeLists.txt              # Build configuration
│
├── core/                        # Core infrastructure
│   ├── ElementTransform.hpp    # Element-level transform concepts & wrappers
│   ├── ContainerTraits.hpp     # Element ↔ Container type mappings
│   ├── ContainerTraits.cpp     # Implementation (typeid usage)
│   ├── ElementRegistry.hpp     # Compile-time typed registry
│   ├── ElementRegistry.cpp
│   ├── TransformPipeline.hpp   # Lazy range-based pipeline builder
│   └── TransformPipeline.cpp
│
├── runtime/                     # Runtime configuration support
│   ├── PipelineDescriptor.hpp  # Runtime pipeline configuration
│   ├── PipelineFactory.hpp     # Type-erased factory
│   ├── PipelineFactory.cpp
│   └── PipelineExecutor.hpp    # Type-erased execution interface
│
├── storage/                     # Storage strategy abstraction
│   ├── StorageStrategy.hpp     # Base storage interfaces
│   ├── LazyStorage.hpp         # Lazy-evaluated storage
│   └── LazyStorage.cpp
│
├── parameters/                  # Parameter handling via reflect-cpp
│   ├── TransformParameters.hpp # Parameter base classes
│   ├── ParameterSerialization.hpp
│   └── ParameterValidation.hpp
│
├── provenance/                  # EntityID relationship tracking
│   ├── ProvenanceTracker.hpp
│   └── ProvenanceTracker.cpp
│
└── examples/                    # Example transform implementations
    ├── MaskAreaTransform.hpp   # Mask2D → float (area calculation)
    ├── MaskToLineTransform.hpp # Mask2D → Line2D (centerline fitting)
    ├── LineAngleTransform.hpp  # Line2D → float (angle calculation)
    └── ComposedExample.cpp     # Multi-step pipeline example
```

## Key Concepts

### Element vs Container Transforms

**Element Transform**: Operates on a single data item
```cpp
// Element-level: Mask2D → float
float calculateArea(Mask2D const& mask, AreaParams const& params);
```

**Container Transform**: Operates on collection
```cpp
// Container-level: MaskData → AnalogTimeSeries
// Automatically generated from element transform!
auto areas = registry.executeContainer<MaskData, AnalogTimeSeries>(
    "CalculateArea", mask_data, params);
```

### Lazy Evaluation

```cpp
// Old approach (eager, materializes each step):
auto skel = skeletonize(masks);      // → MaskData (1GB allocated)
auto lines = maskToLine(skel);       // → LineData (100MB allocated)  
auto angles = lineAngle(lines);      // → AnalogTimeSeries (8KB)
// Peak memory: ~1.1GB

// New approach (lazy, fused pipeline):
auto pipeline = createRangeView(masks)
    | transform(skeletonize)
    | transform(maskToLine)
    | transform(lineAngle);

auto angles = pipeline.materialize();
// Peak memory: ~1MB (only 1 mask in memory at a time)
// Compiler fuses the loop!
```

### Runtime Configuration

```cpp
// User builds pipeline via UI, saves as JSON:
{
  "name": "Whisker Analysis",
  "steps": [
    {"transform": "Skeletonize", "params": {...}},
    {"transform": "MaskToLine", "params": {...}},
    {"transform": "LineAngle", "params": {...}}
  ]
}

// Factory creates compile-time optimized pipeline:
auto pipeline = factory.loadFromJson("analysis.json");

// Execution is fully typed and optimized:
auto result = pipeline->execute(mask_data);
// ↑ This runs as fast as hand-written fused loop!
```

## Usage Examples

### Example 1: Register and Execute Element Transform

```cpp
#include "core/ElementRegistry.hpp"
#include "examples/MaskAreaTransform.hpp"

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

### Example 2: Build Lazy Pipeline

```cpp
#include "core/TransformPipeline.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Create pipeline using standard ranges
// (Container types provide range views via std::ranges::view_interface)
auto pipeline = PipelineBuilder<MaskData, AnalogTimeSeries>()
    .addStep<MaskAreaParams>("CalculateArea", params1)
    .addStep<NormalizeParams>("Normalize", params2)
    .build();

// Execute lazily
auto result = pipeline.execute(mask_data);
```

### Example 3: Ragged Output Transform

```cpp
#include "core/ElementRegistry.hpp"
#include "examples/MaskAreaTransform.hpp"

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

### Example 4: Runtime Configuration

```cpp
#include "runtime/PipelineFactory.hpp"

using namespace WhiskerToolbox::Transforms::V2;

// Load pipeline from JSON
PipelineFactory factory(registry);
auto pipeline = factory.loadFromJson("my_analysis.json");

// Execute with progress reporting
auto result = pipeline->execute(mask_data, [](int progress) {
    std::cout << "Progress: " << progress << "%\n";
});
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

**New system** (`examples/MaskAreaTransform.hpp`):
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
- **Examples**: Working code in `examples/` directory
- **Migration**: Step-by-step guide in this README

## Roadmap

### Phase 1: Core Infrastructure ✅
- [x] Element transform concepts and wrappers
- [x] Container trait system
- [x] Range adaptor views
- [x] Design documentation

### Phase 2: Registry and Execution (In Progress)
- [ ] Element registry implementation
- [ ] Range-based pipeline builder
- [ ] Materialization strategies
- [ ] Basic tests

### Phase 3: Runtime Configuration
- [ ] Pipeline descriptor (JSON schema)
- [ ] Pipeline factory (type dispatch)
- [ ] Type-erased executor
- [ ] UI integration

### Phase 4: Advanced Features
- [ ] Lazy storage strategy
- [ ] Provenance tracking
- [ ] Parallel execution support
- [ ] Performance optimization

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
- Migration questions: See examples in `examples/`
- Performance questions: Run benchmarks in `tests/`

## License

Same as parent WhiskerToolbox project.
