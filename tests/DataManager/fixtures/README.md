# Test Fixture Organization

This directory contains test fixtures for DataManager and transform testing, organized using a modular builder pattern.

## Architecture Overview

The fixture system is organized into three layers:

### 1. Builders (`builders/`)
Lightweight, composable builders for creating test data objects **without** DataManager dependency.
Each builder focuses on a single data type and provides fluent APIs for construction.

**Benefits:**
- No heavy DataManager include in every test file
- Reusable across different test contexts
- Easy to specify exactly what data you need
- Can be used in both v1 and v2 tests

**Organization:**
```
builders/
├── README.md                        # Builder documentation
├── TimeFrameBuilder.hpp             # TimeFrame construction
├── AnalogTimeSeriesBuilder.hpp      # AnalogTimeSeries construction
├── MaskDataBuilder.hpp              # MaskData construction
├── LineDataBuilder.hpp              # LineData construction
├── PointDataBuilder.hpp             # PointData construction
└── DigitalTimeSeriesBuilder.hpp     # Digital series construction
```

### 2. Scenarios (`scenarios/`)
Pre-configured test scenarios using builders. These represent common test patterns
organized by data type and testing domain.

**Organization:**
```
scenarios/
├── analog/
│   ├── threshold_scenarios.hpp      # Event/interval threshold test data
│   ├── filter_scenarios.hpp         # Filter transform test data
│   └── phase_scenarios.hpp          # Phase analysis test data
├── mask/
│   ├── area_scenarios.hpp           # Area calculation test data
│   ├── geometry_scenarios.hpp       # Centroid, component test data
│   └── morphology_scenarios.hpp     # Filtering, hole-filling test data
├── line/
│   ├── geometry_scenarios.hpp       # Angle, curvature test data
│   └── distance_scenarios.hpp       # Point distance test data
└── tableview/
    ├── interval_scenarios.hpp       # Interval-based computation data
    └── sampling_scenarios.hpp       # Sampling and gathering data
```

### 3. Fixtures (root level)
Catch2 test fixtures that combine scenarios with DataManager for integration testing.
These are only needed when tests require the full DataManager context.

**When to use:**
- Testing transforms that interact with DataManager
- Testing pipeline execution
- Integration tests requiring multiple data types

**Organization:**
- Keep existing fixtures for backward compatibility
- New tests should prefer using builders directly
- Fixtures become thin wrappers around builders + DataManager

## Migration Guide

### Old Pattern (Heavy Fixture)
```cpp
// In fixture header - includes DataManager, heavy dependencies
class MyTransformTestFixture {
protected:
    MyTransformTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        // Create complex data inline
        auto signal = std::make_shared<AnalogTimeSeries>(...);
        m_data_manager->setData("signal", signal, TimeKey("time"));
    }
    std::unique_ptr<DataManager> m_data_manager;
};

// In test file
TEST_CASE_METHOD(MyTransformTestFixture, "Test") {
    // Use fixture
}
```

### New Pattern (Builder-based)
```cpp
// In test file - minimal includes
#include "builders/AnalogTimeSeriesBuilder.hpp"
#include "scenarios/analog/threshold_scenarios.hpp"

TEST_CASE("Test") {
    // Option 1: Use pre-configured scenario
    auto signal = analog_scenarios::positive_threshold_signal();
    
    // Option 2: Build custom data
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 3.0f})
        .atTimes({0, 10, 20})
        .build();
    
    // Option 3: Modify scenario
    auto signal = analog_scenarios::positive_threshold_signal()
        .withThreshold(2.0f)
        .build();
}
```

## Design Principles

### 1. Separation of Concerns
- **Builders**: Data construction logic
- **Scenarios**: Domain-specific test patterns
- **Fixtures**: DataManager integration only

### 2. Composability
Builders can be composed and chained:
```cpp
auto mask = MaskDataBuilder()
    .atTime(0, MaskBuilder().box(0, 0, 10, 10))
    .atTime(10, MaskBuilder().circle(50, 50, 20))
    .build();
```

### 3. Discoverability
- Clear naming: `WhatBuilder` for builders, `what_scenarios.hpp` for scenarios
- Organized by data type domain
- Rich documentation and examples

### 4. Minimalism
- No DataManager dependency in builders
- Each builder has focused responsibility
- Scenarios are lightweight and composable

## Usage Examples

### Example 1: Simple Unit Test
```cpp
#include "builders/AnalogTimeSeriesBuilder.hpp"

TEST_CASE("My algorithm") {
    auto input = AnalogTimeSeriesBuilder()
        .withTriangleWave(0, 100, 50)
        .build();
    
    auto result = my_algorithm(input.get());
    REQUIRE(result->size() > 0);
}
```

### Example 2: Using Scenarios
```cpp
#include "scenarios/mask/area_scenarios.hpp"

TEST_CASE("Mask area calculation") {
    auto test_cases = mask_scenarios::area_test_suite();
    
    for (auto& [name, mask_data, expected] : test_cases) {
        auto result = calculate_area(mask_data.get());
        REQUIRE(result == expected);
    }
}
```

### Example 3: DataManager Integration
```cpp
#include "builders/AnalogTimeSeriesBuilder.hpp"
#include "DataManager.hpp"

TEST_CASE("Pipeline execution") {
    DataManager dm;
    
    auto signal = AnalogTimeSeriesBuilder()
        .withSineWave(0, 1000, 10.0)
        .build();
    
    dm.setData("input", signal, TimeKey("time"));
    
    // Test pipeline
    executePipeline(&dm, "input", "output");
    
    REQUIRE(dm.hasData("output"));
}
```

## Future Enhancements

### Planned Features
1. **Random builders**: For fuzzing and stress testing
2. **Validation builders**: For edge cases and error conditions
3. **Comparison utilities**: For asserting equality of complex data
4. **JSON serialization**: Load scenarios from JSON files

### Migration Roadmap
1. **Phase 1 (Current)**: Create builder infrastructure
2. **Phase 2**: Add scenarios for common patterns
3. **Phase 3**: Migrate existing fixtures to use builders internally
4. **Phase 4**: Deprecate monolithic fixtures
5. **Phase 5**: Apply to TableView tests

## Contributing

When adding new test data patterns:

1. **Check if a builder exists** - If not, create one in `builders/`
2. **Check if a scenario fits** - Add to appropriate scenario file
3. **Keep builders simple** - Focus on construction, not validation
4. **Document patterns** - Add examples to this README
5. **Consider reuse** - Will this pattern be useful for v2 transforms?
