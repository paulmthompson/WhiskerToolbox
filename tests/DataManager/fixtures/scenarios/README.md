# Test Scenarios

This directory contains pre-configured test scenarios organized by data type and testing domain.

## Purpose

Scenarios provide reusable, well-documented test data patterns that:
- **Capture common test cases** - threshold crossing, filtering, geometry calculations
- **Share data between v1 and v2 tests** - ensure algorithmic consistency
- **Document expected behavior** - scenarios encode test intent
- **Reduce code duplication** - single definition, multiple uses

## Organization

Scenarios are organized by data type:

```
scenarios/
├── analog/           # AnalogTimeSeries test patterns
│   ├── threshold_scenarios.hpp
│   ├── filter_scenarios.hpp
│   └── phase_scenarios.hpp
├── mask/             # MaskData test patterns
│   ├── area_scenarios.hpp
│   ├── geometry_scenarios.hpp
│   └── morphology_scenarios.hpp
├── line/             # LineData test patterns
│   ├── geometry_scenarios.hpp
│   └── distance_scenarios.hpp
└── tableview/        # TableView computation patterns
    ├── interval_scenarios.hpp
    └── sampling_scenarios.hpp
```

## Usage Pattern

### 1. Direct Use
```cpp
#include "scenarios/analog/threshold_scenarios.hpp"

TEST_CASE("Threshold detection") {
    auto signal = analog_scenarios::positive_threshold_no_lockout();
    auto result = detect_threshold(signal.get(), 1.0f);
    // Assert on result
}
```

### 2. Parametric Variation
```cpp
#include "scenarios/analog/threshold_scenarios.hpp"

TEST_CASE("Threshold with different values") {
    for (float threshold : {0.5f, 1.0f, 2.0f}) {
        auto signal = analog_scenarios::positive_threshold_no_lockout();
        auto result = detect_threshold(signal.get(), threshold);
        // Assert based on threshold
    }
}
```

### 3. Scenario Suites
```cpp
#include "scenarios/mask/area_scenarios.hpp"

TEST_CASE("Area calculation scenarios") {
    auto scenarios = mask_scenarios::area_calculation_suite();
    
    for (auto& [name, mask_data, expected_area] : scenarios) {
        SECTION(name) {
            auto result = calculate_area(mask_data.get());
            REQUIRE(result == expected_area);
        }
    }
}
```

## Design Guidelines

### Naming Convention
- Function names describe the **data pattern**, not the **expected result**
- Example: `positive_threshold_signal()` not `should_detect_three_events()`
- Expected results are documented in comments or returned separately

### Self-Documenting
Each scenario should include:
```cpp
/**
 * @brief Signal with positive threshold crossings and no lockout
 * 
 * Data: {0.5, 1.5, 0.8, 2.5, 1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=1.0:
 *   - Crosses at t=200 (0.5 -> 1.5)
 *   - Crosses at t=400 (0.8 -> 2.5)
 *   - Crosses at t=500 (2.5 -> 1.2 -> above threshold)
 */
inline auto positive_threshold_no_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}
```

### Composability
Scenarios should be composable:
```cpp
// Base scenario
auto base_signal = threshold_scenarios::step_function();

// Variations
auto noisy_signal = add_noise(base_signal, 0.1f);
auto scaled_signal = scale(base_signal, 2.0f);
```

## Migration from Fixtures

When migrating from existing test fixtures:

1. **Identify test data patterns** - What data does the fixture create?
2. **Extract to scenarios** - Create functions that return the data
3. **Document expectations** - What should happen with this data?
4. **Group related scenarios** - Put similar patterns in same file
5. **Update tests** - Use scenarios instead of fixtures

Example:
```cpp
// Before: in fixture
class MyTestFixture {
    void populateData() {
        auto signal = std::make_shared<AnalogTimeSeries>(...);
        m_data_manager->setData("signal", signal, ...);
    }
};

// After: in scenario
namespace my_scenarios {
    inline auto my_test_signal() {
        return AnalogTimeSeriesBuilder()
            .withValues(...)
            .atTimes(...)
            .build();
    }
}

// In test
TEST_CASE("My test") {
    auto signal = my_scenarios::my_test_signal();
    // test logic
}
```

## Contributing

When adding new scenarios:

1. **Check for existing patterns** - Don't duplicate
2. **Use builders** - Don't construct manually
3. **Document thoroughly** - Include expected behavior
4. **Keep focused** - One concept per scenario
5. **Name descriptively** - Describe the data, not the test
