# CorePlotting Layout Engine

The Layout Engine is responsible for calculating spatial positioning for multiple data series in plotting visualizations. It provides a clean separation between **what to display** (series data) and **where to display it** (layout positions).

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Layout Engine                         │
│                                                         │
│   Input: LayoutRequest                                  │
│     ├─ Series metadata (type, ID, stackable)           │
│     ├─ Viewport bounds                                  │
│     └─ Global settings (zoom, pan)                      │
│                                                         │
│   Processing: ILayoutStrategy                           │
│     ├─ StackedLayoutStrategy (vertical stacking)       │
│     └─ RowLayoutStrategy (raster plots)                 │
│                                                         │
│   Output: LayoutResponse                                │
│     └─ SeriesLayout[] (y_center, height per series)    │
└─────────────────────────────────────────────────────────┘
```

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
- **global_zoom/scale/pan**: User-controlled transformations

### SeriesLayout
Output containing:
- **result**: `SeriesLayoutResult` (allocated_y_center, allocated_height)
- **series_id**: Identifier for lookup
- **series_index**: Position in request order

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

// ecg and emg each get 1.0 height (viewport divided by 2 stackable series)
// stim gets 2.0 height (full viewport)
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

// Each trial gets 2.0 / 3 = 0.667 height
// Centers at -0.667, 0.0, 0.667
```

## Design Principles

### 1. Pure Functions
Layout calculations are stateless. Same input → same output. No side effects.

### 2. Strategy Pattern
Layout algorithms are pluggable. Add new strategies without modifying LayoutEngine.

### 3. No Data Storage
LayoutEngine doesn't store series data. Series metadata (type, ID) is sufficient for positioning.

### 4. Viewport-Agnostic
Calculations use normalized coordinates. Works with any viewport bounds.

## Integration with CorePlotting

The Layout Engine outputs `SeriesLayoutResult` which feeds into:

1. **Model Matrix Construction** (`SeriesMatrices.cpp`)
   - Uses `allocated_y_center` for vertical positioning
   - Uses `allocated_height` for scaling

2. **SVG Export** (`SVGExporter.cpp`)
   - Same layout logic for consistent rendering

3. **Spatial Indexing** (`QuadTree`)
   - Y-coordinates match Model matrix for synchronized hit testing

## Testing

Comprehensive unit tests in [LayoutEngine.test.cpp](/tests/CorePlotting/LayoutEngine.test.cpp):
- 82 assertions covering all strategies
- Edge cases (empty requests, single series)
- Mixed stackable/non-stackable series
- Custom viewport bounds

Run tests:
```bash
ctest --preset linux-clang-release -R test_coreplotting
```

## Future Extensions

### Potential Strategies
- **GridLayoutStrategy**: Tile series in 2D grid
- **ProportionalLayoutStrategy**: Variable heights based on data range
- **OverlayLayoutStrategy**: All series at same position (z-ordering)

### Potential Features
- Spacing/padding between series
- Minimum/maximum height constraints
- Group-based layout (related series grouped together)

## See Also
- [CorePlotting DESIGN.md](../DESIGN.md) - Overall architecture
- [CorePlotting ROADMAP.md](../ROADMAP.md) - Implementation progress
- [SeriesMatrices.hpp](../CoordinateTransform/SeriesMatrices.hpp) - MVP matrix construction
