# Transform V2 Parameter System

This directory contains the parameter definitions for Transform V2 system, using [reflect-cpp](https://github.com/getml/reflect-cpp) for automatic JSON serialization/deserialization with validation.

## Overview

All transform parameters use reflect-cpp to provide:
- **Automatic JSON parsing** - No manual serialization code needed
- **Type validation** - Compile-time and runtime type safety
- **Value validation** - Built-in validators for ranges, constraints
- **Optional fields** - Clean handling of default values
- **Clear error messages** - Validation failures report exactly what went wrong

## Parameter Structure

Each parameter struct follows this pattern (based on V1 binary loader patterns):

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

## Available Parameters

### MaskAreaParams

Calculate area of masks with scaling and filtering.

**Fields:**
- `scale_factor` (float, >0): Multiply area by this value (default: 1.0)
- `min_area` (float, ≥0): Minimum area threshold (default: 0.0)
- `exclude_holes` (bool): Exclude holes from area calculation (default: false)

**Example JSON:**
```json
{
  "scale_factor": 2.5,
  "min_area": 10.0,
  "exclude_holes": true
}
```

### SumReductionParams

Sum multiple values into a single value.

**Fields:**
- `ignore_nan` (bool): Ignore NaN values when summing (default: false)
- `default_value` (float): Value to return if input is empty (default: 0.0)

**Example JSON:**
```json
{
  "ignore_nan": true,
  "default_value": -100.0
}
```

## Usage

### Load Parameters from JSON

```cpp
#include "transforms/v2/examples/ParameterIO.hpp"

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

// From file
auto result2 = loadParametersFromFile<MaskAreaParams>("config.json");
```

### Save Parameters to JSON

```cpp
MaskAreaParams params;
params.scale_factor = 3.0f;
params.exclude_holes = true;

// To string
std::string json = saveParametersToJson(params);

// To file (pretty-printed)
bool success = saveParametersToFile(params, "output.json");
```

### Dynamic Loading by Transform Name

```cpp
std::string transform_name = "CalculateMaskArea";
std::string json = R"({"scale_factor": 2.0})";

auto variant = loadParameterVariant(transform_name, json);
if (variant) {
    // Use std::visit to handle the variant
    std::visit([](auto const& params) {
        // Process params...
    }, variant.value());
}
```

## Validation

reflect-cpp provides automatic validation:

### Built-in Validators

- `rfl::Minimum<N>` - Value must be ≥ N
- `rfl::Maximum<N>` - Value must be ≤ N
- `rfl::ExclusiveMinimum<true>` - Value must be > N (not equal)
- `rfl::ExclusiveMaximum<true>` - Value must be < N (not equal)

### Custom Validators

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

## Testing

### Unit Tests

Located in `tests/DataManager/TransformsV2/test_parameter_io.test.cpp`

Run with:
```bash
ctest --preset linux-clang-release -R test_parameter_io
```

### Fuzz Tests

Located in `tests/fuzz/unit/fuzz_v2_parameter_io.cpp`

Fuzz tests verify:
- No crashes on malformed JSON
- Validation rules are enforced
- All valid parameter combinations work
- Round-trip serialization preserves values

Run with:
```bash
./out/build/Clang/Release/tests/fuzz/unit/fuzz_v2_parameter_io
```

### Corpus

Seed corpus in `tests/fuzz/corpus/v2_parameters/` contains:
- Valid parameter examples (defaults, custom, edge cases)
- Invalid examples (negative values, wrong types)
- Malformed JSON (syntax errors)

## Adding New Parameters

1. **Define the parameter struct** with reflect-cpp validators:

```cpp
struct MyTransformParams {
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> threshold;
    std::optional<int> iterations;
    
    float getThreshold() const { 
        return threshold.has_value() ? threshold.value().value() : 0.5f; 
    }
    
    int getIterations() const { 
        return iterations.value_or(10); 
    }
};
```

2. **Add to ParameterVariant** in `ParameterIO.hpp`:

```cpp
using ParameterVariant = std::variant<
    MaskAreaParams,
    SumReductionParams,
    MyTransformParams  // Add here
>;
```

3. **Add to loadParameterVariant** dispatcher:

```cpp
if (transform_name == "MyTransform") {
    auto result = loadParametersFromJson<MyTransformParams>(json_str);
    if (result) return result.value();
}
```

4. **Add tests** to `test_parameter_io.test.cpp`

5. **Add fuzz tests** to `fuzz_v2_parameter_io.cpp`

6. **Add corpus examples** to `tests/fuzz/corpus/v2_parameters/`

## Benefits Over V1 Manual Parsing

- ✅ **Zero boilerplate** - No manual JSON parsing code
- ✅ **Type safety** - Compile-time type checking
- ✅ **Validation** - Built-in range and constraint validation
- ✅ **Error messages** - Clear validation failure reporting
- ✅ **Maintainability** - Parameters defined once, serialization automatic
- ✅ **Testability** - Easy to fuzz and unit test

## See Also

- [reflect-cpp documentation](https://github.com/getml/reflect-cpp)
- V1 binary loader example: `src/DataManager/AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp`
- Transform V2 core: `src/DataManager/transforms/v2/core/`
