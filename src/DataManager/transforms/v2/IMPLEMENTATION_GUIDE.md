# Transformation System V2 - Implementation Guide

## Overview

This guide explains how to use and extend the new transformation system. It covers:

1. Creating new transforms
2. Using the registry
3. Building pipelines
4. Runtime configuration
5. Migration from V1
6. Parameter System & Serialization

## Quick Reference

### Register a Transform

```cpp
#include "transforms/v2/core/ElementRegistry.hpp"
#include <rfl.hpp> // reflect-cpp

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
ElementRegistry registry;
TransformMetadata meta;
meta.description = "My transform does...";
meta.category = "Image Processing";

registry.registerTransform<Mask2D, float, MyParams>(
    "MyTransform",
    myTransform,
    meta
);

// 4. Use it!
Mask2D mask = ...;
MyParams params;
float result = registry.execute<Mask2D, float, MyParams>(
    "MyTransform", mask, params);

// Or on containers (automatic!)
MaskData masks = ...;
auto results = registry.executeContainer<MaskData, AnalogTimeSeries, MyParams>(
    "MyTransform", masks, params);

// Or native container transform
AnalogTimeSeries analog = ...;
auto events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, ThresholdParams>(
    "Threshold", analog, params);
```

## Part 1: Creating Transforms

### Anatomy of a Transform

Every transform consists of:

1. **Element function**: Pure computation on single data item
2. **Parameters struct**: Configuration (with reflect-cpp support)
3. **Registration**: One-line call to registry
4. **Metadata**: Documentation for UI

### Element Function Patterns

#### Pattern 1: Simple Stateless Transform

```cpp
// No parameters needed
Line2D skeletonize(Mask2D const& mask) {
    // Computation
    return result;
}

// Register without parameters
registry.registerTransform<Mask2D, Line2D>(
    "Skeletonize",
    skeletonize
);
```

#### Pattern 2: Parameterized Transform

```cpp
struct SkeletonParams {
    std::string method = "zhang-suen";  // Or "lee", "morphological"
    int max_iterations = 100;
    float threshold = 0.5f;
};

Mask2D skeletonize(Mask2D const& mask, SkeletonParams const& params) {
    if (params.method == "zhang-suen") {
        return zhangSuenThin(mask, params.max_iterations);
    } else if (params.method == "lee") {
        return leeThin(mask, params.max_iterations);
    }
    // ...
}

registry.registerTransform<Mask2D, Mask2D, SkeletonParams>(
    "Skeletonize",
    skeletonize
);
```

#### Pattern 3: Context-Aware Transform

For long-running operations that need progress reporting:

```cpp
float expensiveComputation(
    Mask2D const& mask,
    MyParams const& params,
    ComputeContext const& ctx) {
    
    for (int i = 0; i < params.iterations; ++i) {
        // Check cancellation
        if (ctx.shouldCancel()) {
            throw std::runtime_error("Cancelled");
        }
        
        // Do work...
        
        // Report progress
        ctx.reportProgress((i * 100) / params.iterations);
    }
    
    return result;
}
```

#### Pattern 4: Native Container Transform

For operations that need the entire container (e.g., temporal dependencies, global stats):

```cpp
// Operates on the whole container, not just one element
std::shared_ptr<DigitalEventSeries> analogEventThreshold(
    AnalogTimeSeries const& input,
    AnalogEventThresholdParams const& params,
    ComputeContext const& ctx) {
    
    auto result = std::make_shared<DigitalEventSeries>();
    // ... process entire time series with temporal logic ...
    return result;
}

// Register as container transform
registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
    "AnalogEventThreshold",
    analogEventThreshold
);
```

#### Pattern 5: Multi-Pass Transform (Preprocessing)

For element-level transforms that need global statistics (e.g., Z-Score normalization, MinMax scaling). The system allows you to define a `preprocess()` method in your parameters struct that runs before the main transformation.

```cpp
#include "transforms/v2/core/PreProcessingRegistry.hpp"

struct ZScoreParams {
    // User configuration
    bool clamp = false;
    
    // Cached state (use rfl::Skip to exclude from JSON serialization)
    rfl::Skip<std::optional<float>> mean;
    rfl::Skip<std::optional<float>> std;
    rfl::Skip<bool> preprocessed = false;
    
    // Preprocessing method - automatically called by pipeline
    // Must be a template to handle different view types
    template<typename View>
    void preprocess(View view) {
        if (preprocessed.value()) return;
        
        // Compute statistics in first pass
        double sum = 0, sq_sum = 0, count = 0;
        for (auto const& pair : view) {
            // pair.second is the value (or reference wrapper)
            float val = ...; // Extract value
            sum += val;
            count++;
        }
        
        mean = static_cast<float>(sum / count);
        // ... compute std ...
        preprocessed = true;
    }
};

// Transform function uses cached values
float zScore(float val, ZScoreParams const& params) {
    // Params are guaranteed to be preprocessed here
    return (val - params.mean.value().value()) / params.std.value().value();
}

// Register preprocessing capability in anonymous namespace
namespace {
    auto const register_zscore_prep = RegisterPreprocessing<ZScoreParams>();
}
```

### Parameter Best Practices

#### Use reflect-cpp for Serialization

```cpp
#include <rfl.hpp>

struct MyParams {
    float threshold = 0.5f;
    int window_size = 5;
    std::string mode = "auto";
    bool verbose = false;
};

// Enable serialization (automatic JSON conversion!)
// No manual toJson/fromJson needed!
```

#### Provide Good Defaults

```cpp
struct FilterParams {
    int kernel_size = 3;          // Good default
    float sigma = 1.0f;           // Reasonable value
    std::string boundary = "reflect";  // Safe choice
    
    // Document valid options in comments
    // boundary: "reflect", "constant", "wrap"
};
```

#### Validate Using reflect-cpp

reflect-cpp provides built-in validation via `rfl::Validator`:

```cpp
#include <rfl.hpp>

struct FilterParams {
    // Constrain value to be between 1 and 100
    rfl::Validator<int, rfl::Minimum<1>, rfl::Maximum<100>> kernel_size = 3;
    
    // Positive float
    rfl::Validator<float, rfl::Minimum<0.0f>> sigma = 1.0f;
    
    // String from predefined set
    rfl::Validator<std::string, rfl::OneOf<"reflect", "constant", "wrap">> 
        boundary = "reflect";
};

// Validation happens automatically during deserialization!
// If invalid JSON is provided, rfl::Error is thrown with helpful message
```

Available validators:
- `rfl::Minimum<N>` - minimum value
- `rfl::Maximum<N>` - maximum value  
- `rfl::Size<Min, Max>` - string/container size constraints
- `rfl::OneOf<values...>` - must be one of specified values
- `rfl::AllOf<validators...>` - combine multiple validators
- `rfl::AnyOf<validators...>` - at least one validator must pass
- Custom validators - define your own!

### Metadata for UI Generation

Good metadata helps users discover and use your transform:

```cpp
TransformMetadata meta;
meta.description = "Skeletonize binary mask using Zhang-Suen algorithm";
meta.category = "Image Processing / Morphology";
meta.author = "WhiskerToolbox Team";
meta.version = "2.0";

// Performance hints
meta.is_expensive = true;  // Enables parallelization
meta.is_deterministic = true;  // Same input → same output
meta.supports_cancellation = true;  // Uses ComputeContext

// Type names for UI (auto-filled if not provided)
meta.input_type_name = "Mask2D";
meta.output_type_name = "Line2D";

registry.registerTransform<Mask2D, Line2D, SkeletonParams>(
    "Skeletonize",
    skeletonize,
    meta
);
```

## Part 2: Using the Registry

### Querying Available Transforms

```cpp
ElementRegistry registry;
// ... register transforms ...

// What transforms can I apply to Mask2D?
auto mask_transforms = registry.getTransformsForInputType(typeid(Mask2D));
for (auto const& name : mask_transforms) {
    auto* meta = registry.getMetadata(name);
    std::cout << name << ": " << meta->description << "\n";
}

// What transforms produce Line2D?
auto line_producers = registry.getTransformsForOutputType(typeid(Line2D));

// What transforms take Mask2D and produce Line2D?
auto mask_to_line = registry.getCompatibleTransforms<Mask2D, Line2D>();
```

### Executing Transforms

#### Single Element

```cpp
Mask2D mask = loadMask();
SkeletonParams params{.method = "zhang-suen", .max_iterations = 50};

auto skeleton = registry.execute<Mask2D, Mask2D, SkeletonParams>(
    "Skeletonize",
    mask,
    params
);
```

#### Container (Automatic Lifting)

```cpp
MaskData masks = loadMasks();
SkeletonParams params;

// This applies skeletonize to EACH mask in the container
auto skeletons = registry.executeContainer<MaskData, MaskData, SkeletonParams>(
    "Skeletonize",
    masks,
    params
);
```

#### Native Container Transform

```cpp
AnalogTimeSeries analog = loadAnalogData();
AnalogEventThresholdParams params;

// Executes directly on the container (no lifting)
auto events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
    "AnalogEventThreshold",
    analog,
    params
);
```

#### With Progress Reporting

```cpp
ComputeContext ctx;
ctx.progress = [](int p) {
    std::cout << "Progress: " << p << "%\n";
};
ctx.is_cancelled = []() {
    return user_clicked_cancel;
};

auto result = registry.execute<Mask2D, float, MyParams>(
    "ExpensiveTransform",
    mask,
    params,
    ctx
);
```

## Part 3: Building Pipelines

### Sequential Transform Chains

#### Manual Chaining

```cpp
// Apply transforms one at a time
Mask2D mask = loadMask();

auto skeleton = registry.execute<Mask2D, Mask2D, SkeletonParams>(
    "Skeletonize", mask, params1);

auto line = registry.execute<Mask2D, Line2D, MaskToLineParams>(
    "MaskToLine", skeleton, params2);

float angle = registry.execute<Line2D, float, AngleParams>(
    "LineAngle", line, params3);
```

#### Fused Pipeline Execution

```cpp
// Typed Pipeline (Compile-time output type)
TransformPipeline<MaskData, AnalogTimeSeries> pipeline;
pipeline.addStep<SkeletonParams>("Skeletonize", params1);
pipeline.addStep<MaskToLineParams>("MaskToLine", params2);
pipeline.addStep<AngleParams>("LineAngle", params3);

auto angles = pipeline.execute(masks);

// Dynamic Pipeline (Runtime output type)
TransformPipeline<MaskData> dynamic_pipeline;
dynamic_pipeline.addStep<SkeletonParams>("Skeletonize", params1);
dynamic_pipeline.addStep<MaskToLineParams>("MaskToLine", params2);
dynamic_pipeline.addStep<AngleParams>("LineAngle", params3);

// Returns DataTypeVariant
DataTypeVariant result = dynamic_pipeline.execute(masks);
```

### Lazy Evaluation (Coming in Phase 2)

```cpp
// Instead of materializing each step:
MaskData skeletons = skeletonize_all(masks);  // Materializes 1GB
LineData lines = mask_to_line_all(skeletons); // Materializes 100MB
AnalogTimeSeries angles = angle_all(lines);   // Materializes 8KB

// Use lazy pipeline:
auto pipeline = createRangeView(masks)
    | transform(skeletonize)
    | transform(maskToLine)
    | transform(lineAngle);

// Only materialize final result
auto angles = pipeline.materialize();
// Peak memory: ~1MB (processes one mask at a time)
```

## Part 4: Runtime Configuration

### JSON Pipeline Format

```json
{
  "name": "Whisker Angle Analysis",
  "description": "Extract whisker angles from segmentation masks",
  "version": "1.0",
  "author": "Lab Name",
  
  "input_type": "MaskData",
  "output_type": "AnalogTimeSeries",
  
  "steps": [
    {
      "name": "Skeletonize",
      "enabled": true,
      "parameters": {
        "method": "zhang-suen",
        "max_iterations": 100
      }
    },
    {
      "name": "MaskToLine",
      "enabled": true,
      "parameters": {
        "method": "centerline",
        "smoothing": 5.0
      }
    },
    {
      "name": "LineAngle",
      "enabled": true,
      "parameters": {
        "reference_point": "base",
        "angle_units": "degrees"
      }
    }
  ]
}
```

### Loading and Executing

```cpp
PipelineFactory factory(registry);

// Load from JSON file
auto pipeline = factory.loadFromJson("my_analysis.json");

// Execute
MaskData masks = loadMasks();
auto result = pipeline->execute(masks, [](int p) {
    updateProgressBar(p);
});

// Result is DataTypeVariant
auto angles = std::get<std::shared_ptr<AnalogTimeSeries>>(result);
```

## Part 5: Migration from V1

### Before (V1 System)

```cpp
// transforms/Masks/Mask_Area/mask_area.cpp

class MaskAreaOperation : public TransformOperation {
public:
    std::string getName() const override {
        return "Calculate Area";
    }
    
    std::type_index getTargetInputTypeIndex() const override {
        return typeid(std::shared_ptr<MaskData>);
    }
    
    bool canApply(DataTypeVariant const& variant) const override {
        return std::holds_alternative<std::shared_ptr<MaskData>>(variant);
    }
    
    DataTypeVariant execute(
        DataTypeVariant const& variant,
        TransformParametersBase const* params) override {
        
        auto mask_data = std::get<std::shared_ptr<MaskData>>(variant);
        auto result = std::make_shared<AnalogTimeSeries>();
        
        // Manual iteration
        for (auto const& [time, masks] : mask_data->getData()) {
            for (auto const& mask : masks) {
                float area = calculateArea(mask);
                result->addValue(time, area);
            }
        }
        
        return DataTypeVariant(result);
    }
    
private:
    float calculateArea(Mask2D const& mask) {
        // Actual computation buried in class
        int count = 0;
        for (auto const& row : mask) {
            for (auto val : row) {
                if (val != 0) ++count;
            }
        }
        return static_cast<float>(count);
    }
};

// Also need JSON parameter handling, UI widget, etc.
// ~200+ lines of boilerplate!
```

### After (V2 System)

```cpp
// transforms/v2/algorithms/MaskArea/MaskArea.hpp

struct MaskAreaParams {};  // No parameters needed

float calculateMaskArea(Mask2D const& mask, MaskAreaParams const&) {
    int count = 0;
    for (auto const& row : mask) {
        for (auto val : row) {
            if (val != 0) ++count;
        }
    }
    return static_cast<float>(count);
}

// Registration
void registerMaskAreaTransform(ElementRegistry& registry) {
    TransformMetadata meta;
    meta.description = "Calculate mask area (pixel count)";
    meta.category = "Image Processing";
    
    registry.registerTransform<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea",
        calculateMaskArea,
        meta
    );
}

// That's it! ~30 lines total.
// Container iteration: automatic
// JSON serialization: automatic (reflect-cpp)
// UI generation: automatic (from metadata)
```

### Migration Steps

1. **Extract computation**: Pull pure computation logic out of operation class
2. **Define parameters**: Create params struct with reflect-cpp
3. **Register**: One line in registry
4. **Test element function**: Much easier to unit test!
5. **Remove old code**: Delete operation class, parameter handling, etc.

### Side-by-Side Operation

During migration, both systems work together:

```cpp
// V1 transforms still registered in TransformRegistry
TransformRegistry old_registry;

// V2 transforms in ElementRegistry
ElementRegistry new_registry;

// UI can show both:
auto v1_transforms = old_registry.getOperationNames(...);
auto v2_transforms = new_registry.getAllTransformNames();

// User can choose which to use
```

## Common Patterns

### Pattern: Reduction Operations

Many transforms reduce collections to scalars:

```cpp
// Count
int countMasks(std::vector<Mask2D> const& masks, CountParams const&);

// Sum
float sumAreas(std::vector<Mask2D> const& masks, SumParams const&);

// Mean
float meanCentroidX(std::vector<Mask2D> const& masks, MeanParams const&);
```

### Pattern: Filtering Operations

```cpp
Mask2D threshold(Mask2D const& mask, ThresholdParams const& params) {
    Mask2D result = mask;
    for (auto& row : result) {
        for (auto& val : row) {
            val = (val > params.threshold) ? 255 : 0;
        }
    }
    return result;
}
```

### Pattern: Geometric Transformations

```cpp
Line2D rotate(Line2D const& line, RotateParams const& params) {
    float angle = params.angle_degrees * M_PI / 180.0f;
    Line2D result;
    for (auto const& point : line) {
        float x = point.x * cos(angle) - point.y * sin(angle);
        float y = point.x * sin(angle) + point.y * cos(angle);
        result.push_back({x, y});
    }
    return result;
}
```

### Pattern: Multi-Step Composition

```cpp
// Register individual steps
registry.registerTransform<Mask2D, Mask2D, SkeletonParams>(...);
registry.registerTransform<Mask2D, Line2D, FitParams>(...);
registry.registerTransform<Line2D, float, AngleParams>(...);

// Compose at element level
float extractAngle(Mask2D const& mask, ComposedParams const& params) {
    auto skel = skeletonize(mask, params.skeleton_params);
    auto line = fitLine(skel, params.fit_params);
    return calculateAngle(line, params.angle_params);
}

// Or let user build pipeline via UI!
```

## Testing Transforms

### Unit Test Template

```cpp
#include <catch2/catch_test_macros.hpp>
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"

TEST_CASE("MaskArea - Empty mask", "[transforms][v2]") {
    Mask2D empty_mask(10, std::vector<uint8_t>(10, 0));
    MaskAreaParams params;
    
    float area = calculateMaskArea(empty_mask, params);
    
    REQUIRE(area == 0.0f);
}

TEST_CASE("MaskArea - Full mask", "[transforms][v2]") {
    Mask2D full_mask(10, std::vector<uint8_t>(10, 255));
    MaskAreaParams params;
    
    float area = calculateMaskArea(full_mask, params);
    
    REQUIRE(area == 100.0f);
}

TEST_CASE("MaskArea - Partial mask", "[transforms][v2]") {
    Mask2D mask(10, std::vector<uint8_t>(10, 0));
    // Set some pixels
    mask[5][5] = 255;
    mask[5][6] = 255;
    
    MaskAreaParams params;
    float area = calculateMaskArea(mask, params);
    
    REQUIRE(area == 2.0f);
}
```

### Integration Test Template

```cpp
TEST_CASE("Registry - Execute on container", "[transforms][v2][integration]") {
    ElementRegistry registry;
    registerMaskAreaTransform(registry);
    
    // Create test data
    MaskData masks;
    // ... populate with test masks ...
    
    MaskAreaParams params;
    auto areas = registry.executeContainer<MaskData, AnalogTimeSeries, MaskAreaParams>(
        "CalculateMaskArea",
        masks,
        params
    );
    
    REQUIRE(areas->getNumSamples() == masks.getTotalElementCount());
    // Check specific values...
}
```

## Performance Considerations

### When to Use V2

**Good fits**:
- Element-wise operations
- Filtering, thresholding
- Geometric transformations
- Feature extraction (per-element)

**May need special handling**:
- Operations requiring multiple passes
- Global statistics (mean, std dev across ALL data)
- Operations with dependencies between elements

### Optimization Tips

1. **Avoid copies**: Take `const&`, return by value (RVO)
2. **Preallocate**: Reserve space when output size is known
3. **Use spans**: For views into existing data
4. **Parallel when appropriate**: Mark `is_expensive = true` in metadata
5. **Profile**: Measure before optimizing

## Next Steps

1. **Read DESIGN.md**: Understand overall architecture
2. **Study algorithms/**: See complete working code
3. **Try migrating one transform**: Start with something simple
4. **Add tests**: Verify correctness
5. **Integrate with UI**: See it in action

## Questions?

- **How do I...?** Check algorithms/ directory
- **Why isn't...?** See DESIGN.md for architecture decisions

## Part 6: Parameter System & Serialization

This system uses [reflect-cpp](https://github.com/getml/reflect-cpp) for automatic JSON serialization/deserialization with validation.

### Overview

All transform parameters use reflect-cpp to provide:
- **Automatic JSON parsing** - No manual serialization code needed
- **Type validation** - Compile-time and runtime type safety
- **Value validation** - Built-in validators for ranges, constraints
- **Optional fields** - Clean handling of default values
- **Clear error messages** - Validation failures report exactly what went wrong

### Parameter Structure

Each parameter struct follows this pattern:

```cpp
struct TransformParams {
    // Optional field with validator
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> some_value;
    
    // Optional boolean field
    std::optional<bool> some_flag;
    
    // Helper method with default
    float getSomeValue() const { 
        return some_value.has_value() ? some_value.value().value() : 1.0f; 
    }
    
    bool getSomeFlag() const { 
        return some_flag.value_or(false); 
    }
};
```

### Key Patterns

1. **Optional fields** - Use `std::optional<T>` for all parameters
2. **Validators** - Wrap validated fields in `rfl::Validator<T, Constraints...>`
3. **Helper methods** - Provide `getXxx()` methods that return defaults
4. **Clear naming** - Use descriptive field names matching JSON keys

### Usage Examples

#### Load Parameters from JSON

```cpp
#include "transforms/v2/core/ParameterIO.hpp"

using namespace WhiskerToolbox::Transforms::V2::Examples;

// From string
std::string json = R"({"scale_factor": 2.5})";
auto result = loadParametersFromJson<MaskAreaParams>(json);

if (result) {
    auto params = result.value();
    float scale = params.getScaleFactor(); // 2.5
} else {
    std::cerr << "Error: " << result.error().what() << std::endl;
}
```

#### Save Parameters to JSON

```cpp
MaskAreaParams params;
params.scale_factor = 3.0f;
params.exclude_holes = true;

// To string
std::string json = saveParametersToJson(params);

// To file (pretty-printed)
bool success = saveParametersToFile(params, "output.json");
```

### Validation

reflect-cpp provides automatic validation:

#### Built-in Validators

- `rfl::Minimum<N>` - Value must be ≥ N
- `rfl::Maximum<N>` - Value must be ≤ N
- `rfl::ExclusiveMinimum<true>` - Value must be > N (not equal)
- `rfl::ExclusiveMaximum<true>` - Value must be < N (not equal)

#### Custom Validators

```cpp
struct ValidDataType {
    static constexpr auto valid_types = std::array{"type1", "type2"};
    
    static rfl::Result<std::string> validate(const std::string& value) {
        for (const auto& type : valid_types) {
            if (value == type) return value;
        }
        return rfl::Error("Invalid type: " + value);
    }
};

std::optional<rfl::Validator<std::string, ValidDataType>> data_type;
```
