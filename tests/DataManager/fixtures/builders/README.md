# Test Fixture Builders

This directory contains lightweight, composable builders for creating test data objects.

## Design Philosophy

### Lightweight Construction
Builders focus solely on data construction without heavy dependencies. They:
- **Do not** include DataManager.hpp
- **Do not** perform complex initialization
- **Do** provide fluent APIs for data specification
- **Do** support method chaining

### Type-Focused Organization
Each builder handles one primary data type:
- `TimeFrameBuilder.hpp` - TimeFrame construction
- `AnalogTimeSeriesBuilder.hpp` - AnalogTimeSeries construction  
- `MaskDataBuilder.hpp` - MaskData construction
- `LineDataBuilder.hpp` - LineData construction
- `PointDataBuilder.hpp` - PointData construction
- `DigitalTimeSeriesBuilder.hpp` - Digital series construction

### Composability
Builders can be nested and composed:
```cpp
auto mask_data = MaskDataBuilder()
    .atTime(0, Mask2DBuilder().box(0, 0, 10, 10).build())
    .atTime(10, Mask2DBuilder().circle(50, 50, 20).build())
    .build();
```

## Common Patterns

### Pattern 1: Simple Value Specification
```cpp
auto signal = AnalogTimeSeriesBuilder()
    .withValues({1.0f, 2.0f, 3.0f})
    .atTimes({0, 10, 20})
    .build();
```

### Pattern 2: Parametric Waveforms
```cpp
auto signal = AnalogTimeSeriesBuilder()
    .withSineWave(start_time, end_time, frequency)
    .build();
```

### Pattern 3: Builder Helpers
```cpp
auto mask = MaskDataBuilder()
    .atTime(0, box_mask(0, 0, 10, 10))
    .atTime(10, circle_mask(50, 50, 20))
    .build();
```

## Usage Guidelines

### When to Use Builders
✅ Unit tests for pure algorithms  
✅ Creating specific test scenarios  
✅ Avoiding DataManager dependency  
✅ Building data programmatically  

### When NOT to Use Builders
❌ When you need DataManager integration (use fixtures)  
❌ When loading from files (use IO loaders)  
❌ When a pre-built scenario exists (use scenarios/)  

## Builder Reference

See individual builder headers for detailed API documentation.
