# CorePlotting Layout Engine

The Layout Engine is responsible for calculating spatial positioning for multiple data series in plotting visualizations. It provides a clean separation between **what to display** (series data) and **where to display it** (layout positions).

## Core Abstraction: LayoutTransform

At the heart of the layout system is `LayoutTransform` — a simple affine transform:

```
output = input × gain + offset
```

This single abstraction handles all positioning needs:
- **Data normalization**: Scale raw values to a standard range
- **Vertical stacking**: Position series at different Y offsets
- **User adjustments**: Manual gain/offset tweaks

```cpp
struct LayoutTransform {
    float offset{0.0f};  // Translation (applied after scaling)
    float gain{1.0f};    // Scale factor
    
    float apply(float value) const { return value * gain + offset; }
    float inverse(float transformed) const { return (transformed - offset) / gain; }
    
    // Compose transforms: result applies this AFTER other
    LayoutTransform compose(LayoutTransform const& other) const;
    
    // Convert to 4x4 Model matrix for rendering
    glm::mat4 toModelMatrixY() const;
    glm::mat4 toModelMatrixX() const;
};
```

### Transform Composition

Transforms can be composed to layer multiple effects:

```cpp
// 1. Normalization: z-score data (centers on mean, scales by std_dev)
auto normalize = NormalizationHelpers::forZScore(mean, std_dev);

// 2. Layout positioning: from LayoutEngine (e.g., y_center = 0.5, half_height = 0.25)
auto layout = LayoutTransform{0.5f, 0.25f};

// 3. User adjustment: manual 2x vertical zoom
auto user = LayoutTransform{0.0f, 2.0f};

// Compose: data → normalized → positioned → user-adjusted
auto final_transform = user.compose(layout.compose(normalize));
```

## NormalizationHelpers

Factory functions that create `LayoutTransform` objects from data statistics. These bridge data-level metrics (mean, std_dev, min, max) to the geometric transform abstraction.

```cpp
namespace NormalizationHelpers {
    // Z-score: output = (value - mean) / std_dev
    // Centers data at 0, ±1 = ±1 standard deviation
    LayoutTransform forZScore(float mean, float std_dev);
    
    // Peak-to-peak: map [data_min, data_max] → [target_min, target_max]
    // Default maps to [-1, 1]
    LayoutTransform forPeakToPeak(float data_min, float data_max,
                                   float target_min = -1.0f, float target_max = 1.0f);
    
    // Std dev range: ±N standard deviations from mean map to ±1
    // Common for neural data (e.g., 3 std_devs → full display range)
    LayoutTransform forStdDevRange(float mean, float std_dev, float num_std_devs = 3.0f);
    
    // Unit range: map [0, 1] → [target_min, target_max]
    LayoutTransform forUnitRange(float target_min = -1.0f, float target_max = 1.0f);
    
    // Percentile: robust normalization ignoring outliers
    LayoutTransform forPercentileRange(float low_value, float high_value,
                                        float target_min = -1.0f, float target_max = 1.0f);
    
    // Centered: output = (value - center) * gain
    LayoutTransform forCentered(float center, float gain = 1.0f);
    
    // Manual: direct specification
    LayoutTransform manual(float gain, float offset);
}
```

### Normalization Examples

```cpp
// Neural signal: z-score normalization
float mean = computeMean(signal);
float std_dev = computeStdDev(signal);
auto norm = NormalizationHelpers::forZScore(mean, std_dev);
// Now signal values are centered at 0, with ±1 = ±1 std_dev

// Image intensity: peak-to-peak to [0, 1]
auto norm = NormalizationHelpers::forPeakToPeak(min_intensity, max_intensity, 0.0f, 1.0f);

// Robust normalization: 5th to 95th percentile → [-1, 1]
auto norm = NormalizationHelpers::forPercentileRange(percentile_5, percentile_95);

// Neural display: ±3 std_devs fills the view
auto norm = NormalizationHelpers::forStdDevRange(mean, std_dev, 3.0f);
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Layout Engine                                │
│                                                                     │
│   Input: LayoutRequest                                              │
│     ├─ Series metadata (type, ID, stackable)                       │
│     └─ Viewport bounds (NDC)                                        │
│                                                                     │
│   Processing: ILayoutStrategy                                       │
│     ├─ StackedLayoutStrategy (vertical stacking)                   │
│     ├─ RowLayoutStrategy (raster plots)                             │
│     └─ SpatialLayoutStrategy (2D spatial data)                      │
│                                                                     │
│   Output: LayoutResponse                                            │
│     └─ SeriesLayout[] (y_transform, x_transform per series)        │
└─────────────────────────────────────────────────────────────────────┘
```

**Important**: User zoom/pan state is NOT part of LayoutRequest. The LayoutEngine
computes *relative* series positions (Model matrices). Zoom and pan are handled by
`ViewState` (CoordinateTransform/ViewState.hpp), which is the **single source of truth**
for camera state and produces the View and Projection matrices.

This separation means:
- **Layout recalculation needed**: When series are added/removed or viewport bounds change
- **No layout recalculation needed**: When user zooms or pans (only ViewState changes)

## Key Components

### LayoutEngine

Main coordinator that delegates to pluggable layout strategies. Stateless and pure functional.

```cpp
auto engine = LayoutEngine(std::make_unique<StackedLayoutStrategy>());
LayoutResponse response = engine.compute(request);
```

### LayoutRequest

Input specification containing:
- **series**: Vector of `SeriesInfo` (ID, type, stackability)
- **viewport_y_min/max**: Y-axis bounds in NDC (typically -1 to +1)

**Note**: User zoom/pan is intentionally NOT in LayoutRequest. See ViewState.

### SeriesLayout

Output containing:
- **y_transform**: `LayoutTransform` for Y-axis positioning and scaling
- **x_transform**: `LayoutTransform` for X-axis (usually identity for time-series)
- **series_id**: Identifier for lookup
- **series_index**: Position in request order

```cpp
struct SeriesLayout {
    std::string series_id;
    LayoutTransform y_transform;  // Data normalization + vertical positioning
    LayoutTransform x_transform;  // Usually identity for time-series
    int series_index{0};
    
    // Convenience methods
    glm::mat4 computeModelMatrix() const;
    float transformY(float data_value) const;
    float inverseTransformY(float world_y) const;
};
```

### ILayoutStrategy

Interface for layout algorithms. Implementations:

#### StackedLayoutStrategy
DataViewer-style vertical stacking. Series behavior:
- **Stackable** (analog, stacked events): Divide viewport equally among stackable series
- **Full-canvas** (intervals): Span entire viewport

#### RowLayoutStrategy  
Raster plot-style horizontal rows. All series treated equally:
- Each series gets one row
- Equal row heights
- Top-to-bottom ordering

#### SpatialLayoutStrategy
2D spatial layout for overlay-style plots (whiskers, masks, points on images):
- Transforms both X and Y coordinates (not just vertical stacking)
- Modes: **Fit** (uniform scale, preserves aspect), **Fill** (non-uniform), **Identity** (1:1)
- Supports padding around data bounds

## Usage Examples

### DataViewer Layout (Stacked)

```cpp
LayoutRequest request;
request.series = {
    {"ecg", SeriesType::Analog, true},      // Stackable
    {"emg", SeriesType::Analog, true},      // Stackable
    {"stim", SeriesType::DigitalInterval, false}  // Full-canvas
};
request.viewport_y_min = -1.0f;
request.viewport_y_max = 1.0f;

LayoutEngine engine(std::make_unique<StackedLayoutStrategy>());
LayoutResponse response = engine.compute(request);

// Each stackable series gets a y_transform that positions it in its allocated region
// Full-canvas series get a y_transform spanning the entire viewport
```

### Complete Widget Pattern (Normalization + Layout)

```cpp
// Widget code composing normalization with layout
void buildAnalogSeries(std::string const& key, AnalogTimeSeries const& data,
                       SeriesLayout const& layout) {
    // 1. Compute data statistics
    auto [mean, std_dev] = data.computeStatistics();
    
    // 2. Create normalization transform (data → normalized [-1, 1])
    auto normalize = NormalizationHelpers::forStdDevRange(mean, std_dev, 3.0f);
    
    // 3. Compose with layout transform (normalized → world position)
    auto final_transform = layout.y_transform.compose(normalize);
    
    // 4. Apply to data points
    for (auto const& sample : data) {
        float world_y = final_transform.apply(sample.value);
        // ... add to scene
    }
}
```

### Raster Plot Layout (Rows)

```cpp
LayoutRequest request;
request.series = {
    {"trial1", SeriesType::DigitalEvent, true},
    {"trial2", SeriesType::DigitalEvent, true},
    {"trial3", SeriesType::DigitalEvent, true}
};
request.viewport_y_min = -1.0f;
request.viewport_y_max = 1.0f;

LayoutEngine engine(std::make_unique<RowLayoutStrategy>());
LayoutResponse response = engine.compute(request);

// Each trial gets equal height, with y_transform positioning it in its row
```

## Design Principles

### 1. Pure Functions
Layout calculations are stateless. Same input → same output. No side effects.

### 2. Composable Transforms
Normalization, layout, and user adjustments are separate concerns that compose cleanly via `LayoutTransform::compose()`.

### 3. Strategy Pattern
Layout algorithms are pluggable. Add new strategies without modifying LayoutEngine.

### 4. No Data Storage
LayoutEngine doesn't store series data. Series metadata (type, ID) is sufficient for positioning. Data statistics for normalization are computed separately.

### 5. Separation of Concerns
- **NormalizationHelpers**: Data metrics → LayoutTransform (knows about mean, std_dev, etc.)
- **LayoutEngine**: Series metadata → LayoutTransform (knows about viewport, stacking)
- **Widgets**: Compose the two and apply to rendering

## File Structure

```
Layout/
├── LayoutTransform.hpp        # Core transform abstraction (offset + gain)
├── NormalizationHelpers.hpp   # Factory functions for data-based normalization
├── SeriesLayout.hpp           # Per-series layout result
├── LayoutEngine.hpp           # Strategy pattern coordinator
├── LayoutEngine.cpp
├── StackedLayoutStrategy.hpp  # DataViewer-style vertical stacking
├── StackedLayoutStrategy.cpp
├── RowLayoutStrategy.hpp      # Raster plot-style rows
├── RowLayoutStrategy.cpp
├── SpatialLayoutStrategy.hpp  # 2D spatial fitting
├── SpatialLayoutStrategy.cpp
└── README.md                  # This file
```

## Integration with CorePlotting

The Layout Engine outputs `SeriesLayout` which feeds into:

1. **Model Matrix Construction**
   - `SeriesLayout::computeModelMatrix()` creates 4x4 transform
   - Used directly by renderers for GPU transforms

2. **Coordinate Mapping**
   - `SeriesLayout::transformY()` / `inverseTransformY()` for hit testing
   - Enables world ↔ data coordinate conversion

3. **Mappers**
   - Layout transforms are composed with mapper coordinate extraction
   - Single transform from raw data → final screen position

## Testing

Comprehensive unit tests in [LayoutEngine.test.cpp](../../../tests/CorePlotting/LayoutEngine.test.cpp):
- All layout strategies covered
- Edge cases (empty requests, single series)
- Mixed stackable/non-stackable series
- Custom viewport bounds
- Transform composition

Run tests:
```bash
ctest --preset linux-clang-release -R test_coreplotting
```

## Future Extensions

### Potential Strategies
- **GridLayoutStrategy**: Tile series in 2D grid
- **ProportionalLayoutStrategy**: Variable heights based on data range

### Potential Features
- Spacing/padding between series
- Minimum/maximum height constraints
- Group-based layout (related series grouped together)

## See Also
- [CorePlotting DESIGN.md](../DESIGN.md) - Overall architecture
- [CorePlotting ROADMAP.md](../ROADMAP.md) - Implementation progress
