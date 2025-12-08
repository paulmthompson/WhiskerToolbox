# Test Fixture Builder Examples

This document demonstrates how to use the new builder-based test fixture system.

## Quick Start Examples

### Example 1: Simple Unit Test with Builder

Instead of creating a heavy test fixture that includes DataManager:

```cpp
// OLD APPROACH - Heavy fixture with DataManager
#include "fixtures/AnalogEventThresholdTestFixture.hpp"

TEST_CASE_METHOD(AnalogEventThresholdTestFixture, "My test") {
    auto signal = m_test_signals["positive_no_lockout"];
    // test logic
}
```

Use the lightweight builder directly:

```cpp
// NEW APPROACH - Lightweight builder
#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

TEST_CASE("My test") {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
    
    // test logic - signal is std::shared_ptr<AnalogTimeSeries>
    REQUIRE(signal->size() == 5);
}
```

### Example 2: Using Pre-configured Scenarios

For common test patterns, use scenarios:

```cpp
#include "fixtures/scenarios/analog/threshold_scenarios.hpp"

TEST_CASE("Threshold detection") {
    // Get pre-configured test data
    auto signal = analog_scenarios::positive_threshold_no_lockout();
    
    // Test your algorithm
    auto events = detect_threshold(signal.get(), 1.0f);
    
    // Assert expected behavior (documented in scenario)
    REQUIRE(events.size() == 3);
}
```

### Example 3: Parametric Testing with Builders

Test multiple variations easily:

```cpp
#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

TEST_CASE("Sine wave at different frequencies") {
    for (float freq : {0.01f, 0.05f, 0.1f}) {
        auto signal = AnalogTimeSeriesBuilder()
            .withSineWave(0, 100, freq, 1.0f)
            .build();
        
        auto result = process(signal.get());
        // Assert based on frequency
    }
}
```

### Example 4: Complex Mask Construction

Build complex mask data without fixture overhead:

```cpp
#include "fixtures/builders/MaskDataBuilder.hpp"

TEST_CASE("Mask area calculation") {
    auto mask_data = MaskDataBuilder()
        .withBox(0, 10, 10, 20, 20)      // Box at t=0
        .withCircle(10, 50, 50, 15)       // Circle at t=10
        .withPoint(20, 100, 100)          // Single pixel at t=20
        .withImageSize(800, 600)
        .build();
    
    auto areas = calculate_areas(mask_data.get());
    
    REQUIRE(areas.size() == 3);
    REQUIRE(areas[0] == Catch::Approx(400.0f)); // 20x20 box
}
```

### Example 5: Using Mask Shape Helpers

Leverage predefined shapes:

```cpp
#include "fixtures/builders/MaskDataBuilder.hpp"

TEST_CASE("Connected components") {
    using namespace mask_shapes;
    
    auto mask_data = MaskDataBuilder()
        .atTime(0, box(0, 0, 10, 10))           // Shape helper
        .atTime(0, circle(50, 50, 5))           // Another shape
        .atTime(10, line(0, 0, 100, 100))       // Diagonal line
        .build();
    
    auto components = find_connected_components(mask_data.get());
    // test logic
}
```

### Example 6: Line Geometry Testing

Build line data with geometric shapes:

```cpp
#include "fixtures/builders/LineDataBuilder.hpp"

TEST_CASE("Line angle calculation") {
    using namespace line_shapes;
    
    auto line_data = LineDataBuilder()
        .atTime(0, horizontal(0, 10, 5, 4))     // Horizontal line
        .atTime(10, vertical(5, 0, 10, 4))       // Vertical line
        .atTime(20, diagonal(0, 0, 10, 4))       // 45-degree line
        .build();
    
    auto angles = calculate_angles(line_data.get());
    
    REQUIRE(angles[0] == Catch::Approx(0.0f));     // Horizontal
    REQUIRE(angles[1] == Catch::Approx(90.0f));    // Vertical
    REQUIRE(angles[2] == Catch::Approx(45.0f));    // Diagonal
}
```

### Example 7: Custom Waveforms

Create custom signal patterns:

```cpp
#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

TEST_CASE("Custom waveform processing") {
    auto signal = AnalogTimeSeriesBuilder()
        .withFunction(0, 100, [](int t) {
            return std::exp(-static_cast<float>(t) / 10.0f);
        })
        .build();
    
    // Test with exponential decay
    auto result = process(signal.get());
    // assertions
}
```

### Example 8: Digital Event Series

Build event and interval data:

```cpp
#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"

TEST_CASE("Event detection") {
    auto events = DigitalEventSeriesBuilder()
        .withEvents({10, 20, 30, 40, 50})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 15)
        .withInterval(25, 35)
        .withInterval(45, 55)
        .build();
    
    auto overlaps = find_overlaps(events.get(), intervals.get());
    // test logic
}
```

### Example 9: TimeFrame Construction

Build time frames for tests:

```cpp
#include "fixtures/builders/TimeFrameBuilder.hpp"

TEST_CASE("Time synchronization") {
    auto time_high_res = TimeFrameBuilder()
        .withRange(0, 1000, 1)  // Every frame
        .build();
    
    auto time_low_res = TimeFrameBuilder()
        .withRange(0, 1000, 10) // Every 10th frame
        .build();
    
    // Test time interpolation
    auto synced = synchronize(time_high_res.get(), time_low_res.get());
    // assertions
}
```

### Example 10: Combining with DataManager (when needed)

When you do need DataManager integration:

```cpp
#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include "fixtures/builders/MaskDataBuilder.hpp"
#include "DataManager.hpp"

TEST_CASE("Pipeline execution") {
    DataManager dm;
    
    // Use builders to populate DataManager
    auto signal = AnalogTimeSeriesBuilder()
        .withSineWave(0, 100, 0.1f)
        .build();
    
    auto masks = MaskDataBuilder()
        .withBox(0, 10, 10, 50, 50)
        .build();
    
    dm.setData("signal", signal, TimeKey("time"));
    dm.setData("masks", masks, TimeKey("time"));
    
    // Test pipeline
    execute_pipeline(&dm, "my_pipeline");
    
    REQUIRE(dm.hasData("output"));
}
```

## Migration Examples

### Migrating from Existing Fixtures

Before:
```cpp
// In AnalogEventThresholdTestFixture.hpp
class AnalogEventThresholdTestFixture {
protected:
    AnalogEventThresholdTestFixture() {
        m_data_manager = std::make_unique<DataManager>();
        createSignal("positive_no_lockout", 
                     {0.5f, 1.5f, 0.8f, 2.5f, 1.2f}, 
                     {100, 200, 300, 400, 500});
    }
    // ... more setup code
    std::unique_ptr<DataManager> m_data_manager;
};

// In test file
TEST_CASE_METHOD(AnalogEventThresholdTestFixture, "Test") {
    auto signal = m_test_signals["positive_no_lockout"];
    // test
}
```

After:
```cpp
// In scenarios/analog/threshold_scenarios.hpp
namespace analog_scenarios {
    inline auto positive_threshold_no_lockout() {
        return AnalogTimeSeriesBuilder()
            .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
            .atTimes({100, 200, 300, 400, 500})
            .build();
    }
}

// In test file - much simpler!
#include "scenarios/analog/threshold_scenarios.hpp"

TEST_CASE("Test") {
    auto signal = analog_scenarios::positive_threshold_no_lockout();
    // test
}
```

## Best Practices

1. **Use builders directly for simple tests** - No need for fixtures
2. **Use scenarios for common patterns** - Avoid duplication
3. **Only use fixtures when DataManager is required** - Keep it minimal
4. **Document scenarios thoroughly** - They're shared between tests
5. **Prefer builders over raw construction** - More maintainable
6. **Chain builder methods** - Fluent API for readability
7. **Use shape/waveform helpers** - Don't repeat geometric construction

## Performance Benefits

The builder approach is more efficient than fixtures because:

- **No DataManager overhead** - When you don't need it
- **No global state** - Each test creates only what it needs
- **Faster compilation** - Fewer includes, less template instantiation
- **Better test isolation** - No shared state between tests
- **Parallelizable** - Tests don't depend on fixture setup order
