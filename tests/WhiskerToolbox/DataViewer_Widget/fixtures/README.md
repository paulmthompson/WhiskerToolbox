# DataViewer Test Fixture Organization

This directory contains test fixtures for DataViewer and CorePlotting testing, organized using a modular builder pattern similar to the DataManager test fixtures.

## Architecture Overview

The fixture system is organized into two layers:

### 1. Builders (`builders/`)
Lightweight, composable builders for creating test data objects **without** DataManager dependency.
Each builder focuses on a single data type and provides fluent APIs for construction.

**Benefits:**
- No heavy DataManager include in every test file
- Reusable across different test contexts
- Easy to specify exactly what data you need
- Separates test data generation from implementation files

**Organization:**
```
builders/
├── DigitalEventBuilder.hpp          # EventData and display options construction
├── (Future) AnalogSeriesBuilder.hpp # Analog time series test data
├── (Future) DigitalIntervalBuilder.hpp
└── (Future) PlottingManagerBuilder.hpp
```

### 2. Scenarios (`scenarios/`)
Pre-configured test scenarios using builders. These represent common test patterns
organized by data type and testing domain.

**Organization:**
```
scenarios/
├── digital_event_scenarios.hpp      # Digital event MVP and rendering tests
├── (Future) analog_scenarios.hpp    # Analog MVP and stacking tests
└── (Future) mixed_stacking_scenarios.hpp
```

## Design Principles

### 1. Separation of Concerns
- **Builders**: Data construction logic (in test fixtures)
- **Scenarios**: Domain-specific test patterns (in test fixtures)
- **Implementation**: Pure production code (no test helpers)

### 2. Composability
Builders can be composed and chained:
```cpp
auto events = DigitalEventBuilder()
    .withRandomEvents(100, 1000.0f)
    .build();

auto options = DigitalEventDisplayOptionsBuilder()
    .withStackedMode()
    .withAllocation(center, height)
    .withIntrinsicProperties(events)
    .build();
```

### 3. Discoverability
- Clear naming: `WhatBuilder` for builders, `what_scenarios.hpp` for scenarios
- Organized by data type domain
- Rich documentation and examples

## Usage Examples

### Example 1: Simple MVP Test
```cpp
#include "fixtures/builders/DigitalEventBuilder.hpp"

TEST_CASE("Digital Event MVP") {
    auto events = DigitalEventBuilder()
        .withRandomEvents(10, 100.0f)
        .build();
    
    // Use events in test
    REQUIRE(events.size() == 10);
}
```

### Example 2: Using Scenarios
```cpp
#include "fixtures/scenarios/digital_event_scenarios.hpp"

TEST_CASE("Event rendering optimization") {
    auto dense_events = digital_event_scenarios::dense_events_for_rendering();
    auto options = digital_event_scenarios::options_with_intrinsics(dense_events);
    
    // Options should have reduced alpha/thickness
    REQUIRE(options.alpha < 0.8f);
    REQUIRE(options.line_thickness < 3);
}
```

### Example 3: Display Options Configuration
```cpp
#include "fixtures/builders/DigitalEventBuilder.hpp"

TEST_CASE("Stacked mode allocation") {
    PlottingManager manager;
    float center, height;
    manager.calculateDigitalEventSeriesAllocation(0, center, height);
    
    auto options = DigitalEventDisplayOptionsBuilder()
        .withStackedMode()
        .withAllocation(center, height)
        .withMarginFactor(0.95f)
        .build();
    
    // Use options in test
}
```

## Migration from Implementation Files

Previously, test data generation functions were embedded in implementation files:
- `generateTestEventData()` in MVP_DigitalEvent.cpp
- `setEventIntrinsicProperties()` in MVP_DigitalEvent.cpp

These have been migrated to:
- `DigitalEventBuilder::withRandomEvents()` - replaces generateTestEventData()
- `DigitalEventDisplayOptionsBuilder::withIntrinsicProperties()` - replaces setEventIntrinsicProperties()

**Old Pattern:**
```cpp
// In MVP_DigitalEvent.hpp (implementation file)
std::vector<EventData> generateTestEventData(size_t num_events, float max_time, unsigned int seed);

// In test
auto events = generateTestEventData(10, 100.0f, 42);
```

**New Pattern:**
```cpp
// In test file only
#include "fixtures/builders/DigitalEventBuilder.hpp"

auto events = DigitalEventBuilder()
    .withRandomEvents(10, 100.0f, 42)
    .build();
```

## Future Enhancements

### Planned Builders
1. **AnalogSeriesBuilder**: For analog time series test data
2. **DigitalIntervalBuilder**: For interval series test data
3. **PlottingManagerBuilder**: For configuring PlottingManager state
4. **ViewStateBuilder**: For coordinate transform tests

### Planned Scenarios
1. **analog_mvp_scenarios.hpp**: Stacking, allocation, and MVP tests
2. **mixed_stacking_scenarios.hpp**: Analog + digital event combinations
3. **interaction_scenarios.hpp**: Zoom, pan, selection test cases

### Migration Roadmap
1. **Phase 1 (Current)**: Digital event builder and scenarios
2. **Phase 2**: Analog time series builders
3. **Phase 3**: Digital interval builders
4. **Phase 4**: Mixed stacking scenarios
5. **Phase 5**: Complete removal of test helpers from implementation files

## Contributing

When adding new test data patterns:

1. **Create builders in `builders/`** - Focus on construction, not validation
2. **Add scenarios in `scenarios/`** - Common patterns for specific test domains
3. **Keep implementation files clean** - No test data generation in production code
4. **Document patterns** - Add examples to this README
5. **Follow DataManager pattern** - Maintain consistency with existing test fixtures
