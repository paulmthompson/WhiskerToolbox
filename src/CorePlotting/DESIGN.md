# CorePlotting Library Design Document

## Overview

CorePlotting is the foundational, Qt-independent C++ library for the WhiskerToolbox plotting stack. It provides unified abstractions for spatial visualization of time-series data, separating **data & layout** from **rendering** and **interaction**.

The ultimate goal is to de-complect the current parallel implementations (DataViewer, Analysis Dashboard, SVG Exporter) into a layered architecture:

1.  **CorePlotting** (This Library): Data structures, Layout engines, Coordinate transforms (MVP), Spatial Indexing. (No Qt, No OpenGL).
2.  **PlottingOpenGL**: Rendering strategies that consume CorePlotting descriptions and issue OpenGL commands.
3.  **QtPlotting** (Future): Qt widgets, Input handling, Tooltips, bridging user events to CorePlotting queries.

---

## Architectural Evolution: Position Mappers (December 2025)

### The Problem: Coordinate Mapping is Visualization-Specific

The initial design baked DataViewer-specific coordinate logic into `SceneBuilder`:

```cpp
// PROBLEM: This is DataViewer-specific, not general
SceneBuilder::addEventSeries(series, layout, time_frame) {
    float x = time_frame.getTimeAtIndex(event.time);  // X = absolute time
    float y = layout.allocated_y_center;               // Y = stacked position
}
```

This doesn't work for other visualizations:

| Visualization | X Coordinate Source | Y Coordinate Source |
|---------------|---------------------|---------------------|
| **DataViewer Events** | `TimeFrame[event.time]` | `layout.y_center` (constant) |
| **DataViewer Analog** | `TimeFrame[sample.index]` | `layout.apply(sample.value)` |
| **SpatialOverlay Points** | `point.x` | `point.y` |
| **Raster Plot** | `relative_time(event, center)` | `row.y_center` |
| **Scatter Plot** | `series_a.value[i]` | `series_b.value[i]` |
| **Line Event Plot** | `relative_time(sample, trigger)` | `layout.apply(sample.value)` |

### The Solution: Separate Extraction, Layout, and Scene Building

The architecture now follows a **three-layer model**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Visualization-Specific Layer                            │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────────┐   │
│  │ TimeSeriesMapper │  │  SpatialMapper   │  │   ScatterMapper          │   │
│  │ (DataViewer)     │  │ (SpatialOverlay) │  │   (Future)               │   │
│  │                  │  │                  │  │                          │   │
│  │ Knows: TimeFrame │  │ Knows: Direct    │  │ Knows: Two series +      │   │
│  │ + LayoutEngine   │  │ X/Y from data    │  │ TimeFrameIndex sync      │   │
│  └────────┬─────────┘  └────────┬─────────┘  └────────────┬─────────────┘   │
│           │                     │                         │                 │
│           ▼                     ▼                         ▼                 │
│     positions[]           positions[]               positions[]             │
│     entity_ids[]          entity_ids[]              entity_ids[]            │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        CorePlotting (General)                               │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         SceneBuilder                                 │   │
│  │  • addGlyphs(key, positions[], entity_ids[])                        │   │
│  │  • addRectangles(key, bounds[], entity_ids[])                       │   │
│  │  • addPolyLine(vertices[], entity_ids[])                            │   │
│  │  • setBounds() / setViewState() / build()                           │   │
│  │                                                                      │   │
│  │  Does NOT know about TimeFrame, specific data types, or extractors  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                       RenderableScene                                │   │
│  │  • glyph_batches[], rectangle_batches[], polyline_batches[]         │   │
│  │  • spatial_index (QuadTree<EntityId>)                               │   │
│  │  • view_matrix, projection_matrix                                   │   │
│  │  • batch_key_maps (embedded)                                        │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### The General Model

Every data element's position follows this formula:

```
Position = LayoutTransform( X_Extractor(data, index), Y_Extractor(data, index) )
```

Where:
- **Extractor**: Function that pulls a raw coordinate from a data element
- **LayoutTransform**: Maps raw coordinates → world coordinates

### Axis Bindings

Each axis can bind to different data sources:

```cpp
struct AxisBinding {
    enum class Source {
        TimeFrameAbsolute,   // TimeFrameIndex → absolute time via TimeFrame
        TimeFrameRelative,   // TimeFrameIndex → time relative to reference
        DataFieldX,          // Extract X field from spatial data (Point, Line)
        DataFieldY,          // Extract Y field from spatial data
        DataValue,           // Extract primary value (AnalogTimeSeries sample)
        LayoutOnly           // Position comes entirely from layout (no data extraction)
    };
    
    Source source;
    std::shared_ptr<TimeFrame> time_frame;           // For TimeFrame sources
    std::optional<int64_t> reference_time;           // For relative time
    std::optional<float> normalization_factor;       // For normalized coordinates
};
```

### Layout Transforms

Layout transforms convert raw extracted values to world coordinates:

```cpp
struct LayoutTransform {
    float offset{0.0f};      // Added after scaling
    float scale{1.0f};       // Applied to raw value
    float allocated_min{0.0f};
    float allocated_max{0.0f};
    
    [[nodiscard]] float apply(float raw_value) const {
        return raw_value * scale + offset;
    }
};

struct SeriesLayout {
    std::string key;
    LayoutTransform x_transform;  // Usually identity for time-series
    LayoutTransform y_transform;  // Vertical positioning and scaling
};
```

### Position Mappers: Range-Based Design

Mappers return **range views**, not vectors. This enables:
1. **Zero-copy**: Data flows directly from source → GPU buffer + QuadTree
2. **Single traversal**: x, y, and entity_id extracted together
3. **Data source controls iteration**: Each data type knows how to efficiently access its own storage

#### The MappedElement Concept

```cpp
// Common element type yielded by all mappers
struct MappedElement {
    float x;
    float y;
    EntityId entity_id;
};

// For rectangles (intervals)
struct MappedRectElement {
    float x, y, width, height;
    EntityId entity_id;
};

// For polylines (analog series, lines)
struct MappedVertex {
    float x;
    float y;
    // Note: EntityId typically per-line, not per-vertex
};
```

#### Range-Based Mapper API

```cpp
// TimeSeriesMapper - returns ranges, not vectors
namespace TimeSeriesMapper {
    // Returns range of MappedElement (x=time, y=layout.y_center)
    auto mapEvents(
        DigitalEventSeries const& series,
        TimeFrame const& time_frame,
        SeriesLayout const& layout) -> /* range of MappedElement */;
    
    // Returns range of MappedRectElement
    auto mapIntervals(
        DigitalIntervalSeries const& series,
        TimeFrame const& time_frame,
        SeriesLayout const& layout) -> /* range of MappedRectElement */;
    
    // Returns range of MappedVertex + line metadata
    auto mapAnalog(
        AnalogTimeSeries const& series,
        TimeFrame const& time_frame,
        SeriesLayout const& layout,
        TimeFrameIndex start, TimeFrameIndex end) -> /* range of MappedVertex */;
}

// SpatialMapper - direct x/y from data
namespace SpatialMapper {
    // Returns range yielding {point.x, point.y, point.entity_id}
    auto mapPoints(
        PointData const& points,
        TimeFrameIndex at_time,
        SeriesLayout const& layout) -> /* range of MappedElement */;
    
    // Returns range of vertices from line data
    auto mapLines(
        LineData const& lines,
        TimeFrameIndex at_time,
        SeriesLayout const& layout) -> /* range of MappedVertex */;
}

// RasterMapper - relative time positioning
namespace RasterMapper {
    // Returns range with x = relative_time(event, reference)
    auto mapEventsRelative(
        DigitalEventSeries const& series,
        TimeFrame const& time_frame,
        int64_t reference_time,
        float time_window,
        SeriesLayout const& layout) -> /* range of MappedElement */;
}
```

#### Ideal: Data Objects Provide Iteration

The cleanest design has data objects provide their own mapping views:

```cpp
// PointData provides efficient iteration with layout applied
for (auto [x, y, entity_id] : points.viewAtTime(current_frame, layout)) {
    // Single traversal, no intermediate storage
}

// DigitalEventSeries with TimeFrame conversion + layout
for (auto [x, y, entity_id] : events.viewMapped(time_frame, layout)) {
    // x = time_frame.getTimeAtIndex(event.time)
    // y = layout.y_transform.apply(0)  // events have no intrinsic Y
}

// AnalogTimeSeries with TimeFrame + layout
for (auto [x, y] : analog.viewMapped(time_frame, layout, start, end)) {
    // x = time_frame.getTimeAtIndex(sample.index)
    // y = layout.y_transform.apply(sample.value)
}
```

This "short-circuits" the Mapper layer entirely for simple cases—the data object itself knows how to yield mapped coordinates efficiently.

### Layout Engines by Plot Type

Different plot types need different layout strategies:

```cpp
// Stacked time-series (DataViewer)
class StackedTimeSeriesLayout {
    LayoutResponse compute(std::vector<SeriesRequest> const& series,
                           float viewport_y_min, float viewport_y_max);
};

// Spatial plots (SpatialOverlay) - simple viewport fitting
class SpatialLayout {
    SeriesLayout compute(BoundingBox const& data_bounds,
                         BoundingBox const& viewport_bounds,
                         float padding = 0.1f);
};

// Row-based layout (Raster plots)
class RowLayout {
    LayoutResponse compute(int num_rows,
                           float viewport_y_min, float viewport_y_max);
};
```

### Example Usage Patterns

**DataViewer (time-series):**
```cpp
// 1. Compute layout
auto layout = StackedTimeSeriesLayout().compute(series_requests, -1.0f, 1.0f);

// 2. Build scene - Mapper returns range, SceneBuilder consumes directly
RenderableScene scene = SceneBuilder()
    .setBounds(data_bounds)
    .addGlyphs("events", 
               TimeSeriesMapper::mapEvents(events, *master_tf, *layout.findLayout("events")))
    .addPolyLine("analog",
                 TimeSeriesMapper::mapAnalog(analog, *master_tf, *layout.findLayout("analog"), start, end),
                 analog.getEntityId())
    .build();

// Or with data object providing mapped view directly:
RenderableScene scene = SceneBuilder()
    .setBounds(data_bounds)
    .addGlyphs("events", events.viewMapped(*master_tf, *layout.findLayout("events")))
    .build();
```

**SpatialOverlay (spatial data):**
```cpp
// 1. Simple spatial layout (fits bounds to viewport)
auto layout = SpatialLayout().compute(data_bounds, viewport_bounds);

// 2. Build scene - PointData yields {x, y, entity_id} directly
RenderableScene scene = SceneBuilder()
    .setBounds(viewport_bounds)
    .addGlyphs("whiskers", points.viewAtTime(current_frame, layout))
    .build();

// Single traversal: PointData → GPU buffer + QuadTree
// No intermediate vector<glm::vec2> or vector<EntityId>
```

**Raster Plot:**
```cpp
// 1. Row layout for trials
auto layout = RowLayout().compute(trials.size(), -1.0f, 1.0f);

// 2. Build scene with range-based API
SceneBuilder builder;
builder.setBounds(plot_bounds);

for (size_t i = 0; i < trials.size(); ++i) {
    // Mapper returns range, consumed directly by addGlyphs
    builder.addGlyphs(
        "trial_" + std::to_string(i),
        RasterMapper::mapEventsRelative(
            *trials[i].events, *trials[i].time_frame,
            trials[i].align_time, time_window, layout.layouts[i]));
}

auto scene = builder.build();
```

### Key Design Principles

1. **SceneBuilder consumes ranges, never vectors** — no intermediate allocations
2. **Single traversal for x, y, entity_id** — data flows directly to GPU buffer + QuadTree
3. **Data objects can provide mapped views** — `points.viewAtTime(frame, layout)` short-circuits the Mapper
4. **Each plot type has coordinate extraction logic** — encapsulated in Mappers or data object methods
5. **Layout engines are plot-type specific** — but share a common output format (`SeriesLayout`)
6. **Data type coupling is acceptable** — CorePlotting knows about DataManager types; this is intentional
7. **QuadTree uses the same positions as rendering** — both populated from same range traversal

### Directory Structure Update

```
src/CorePlotting/
├── Mappers/                       # Visualization-specific coordinate mapping
│   ├── MappedElement.hpp          # Common element types (MappedElement, MappedRectElement, MappedVertex)
│   ├── TimeSeriesMapper.hpp       # DataViewer: events→time, intervals→time, analog→time
│   ├── SpatialMapper.hpp          # SpatialOverlay: points→x/y, lines→x/y
│   ├── RasterMapper.hpp           # Raster/PSTH: events→relative time
│   └── MapperConcepts.hpp         # C++20 concepts for range requirements
│
├── Layout/                        # Plot-type-specific layout strategies
│   ├── StackedTimeSeriesLayout.hpp
│   ├── SpatialLayout.hpp
│   ├── RowLayout.hpp
│   └── SeriesLayout.hpp           # Common layout result type
│
├── SceneGraph/                    # Generic scene assembly (range-based API)
│   ├── SceneBuilder.hpp           # Fluent builder consuming ranges
│   ├── RenderableScene.hpp
│   └── RenderablePrimitives.hpp
│
└── ... (existing directories)
```

**Note on Data Object Integration:**

For optimal efficiency, data objects in DataManager can provide mapped views directly:

```cpp
// In DataManager/PointData/PointData.hpp
class PointData {
    // Returns range yielding {x, y, entity_id} with layout applied
    auto viewAtTime(TimeFrameIndex time, SeriesLayout const& layout) const;
};

// In DataManager/DigitalTimeSeries/Digital_Event_Series.hpp  
class DigitalEventSeries {
    // Returns range yielding {x=time, y=layout.y_center, entity_id}
    auto viewMapped(TimeFrame const& tf, SeriesLayout const& layout) const;
};
```

This keeps Mappers thin (or eliminates them for simple cases) while ensuring single-traversal efficiency.

---

## Motivation

WhiskerToolbox currently has three parallel implementations for event-like visualization:

| Component | Location | Data Format | Interaction | EntityID |
|-----------|----------|-------------|-------------|----------|
| DataViewer FullCanvas Events | `WhiskerToolbox/DataViewer` | `DigitalEventSeries` | Click | ✅ |
| DataViewer Stacked Events | `WhiskerToolbox/DataViewer` | `DigitalEventSeries` | Scroll/pan | ✅ |
| EventPlotWidget (Raster) | `WhiskerToolbox/Analysis_Dashboard` | `vector<vector<float>>` | Hover, pan, zoom | ❌ |
| SVG Exporter | `WhiskerToolbox/DataViewer` | `DigitalEventSeries` | None | N/A |

**Goal**: Unify these into a shared abstraction where:
1.  **Single Source of Truth for Layout**: The position of an event or point is calculated ONCE in `CorePlotting`.
2.  **Unified MVP Logic**: OpenGL and SVG use the exact same matrix calculation functions.
3.  **Universal Interaction**: Hover/Click detection works via `EntityId` lookup across all plot types.

## Current Architecture Pain Points

### DataViewer Issues

1. **XAxis isolation**: The current `XAxis` class manages time range independently, without knowledge of `TimeFrame` objects. It should integrate with `ViewState` and respect the bounds of the underlying `TimeFrame` to prevent scrolling/zooming beyond valid data ranges.

2. ~~**Duplicate series storage**~~: ✅ **RESOLVED** — Eliminated `PlottingManager` series storage. Series data now lives only in `OpenGLWidget`. `LayoutCalculator` (renamed from `PlottingManager`) is now a pure layout calculator with no data storage.

3. ~~**DisplayOptions conflation**~~: ✅ **RESOLVED** — Split `DisplayOptions` into three separate structs:
   - `SeriesStyle`: Rendering properties (color, alpha, thickness)
   - `SeriesLayoutResult`: Layout output (allocated_y_center, allocated_height)
   - `SeriesDataCache`: Cached calculations (std_dev, mean)

4. **Per-draw-call vertex generation**: Each render frame, `OpenGLWidget` rebuilds vertex arrays inline. This should be separated into data preparation (CorePlotting) and rendering (OpenGL).

### Matrix Architecture Comparison

The two main plotting widgets use different MVP strategies:

| Widget | Model Matrix | View Matrix | Projection Matrix |
|--------|--------------|-------------|-------------------|
| **SpatialOverlayOpenGLWidget** | Identity (shared) | Pan/Zoom (shared) | Ortho from ViewState (shared) |
| **DataViewer OpenGLWidget** | Per-series (scaling, positioning) | Global pan (shared) | Time range ortho (shared) |

**Key insight**: For spatial data (points, masks, lines), all data shares one coordinate system, so a single MVP suffices. For time-series data with stacked layouts, each series needs its own **Model matrix** to position it vertically, while **View** (pan) and **Projection** (time range → NDC) remain shared.

This is the correct pattern:
- **Model**: Per-series (positions series in world space, handles series-specific scaling)
- **View**: Shared (global camera pan/zoom)
- **Projection**: Shared (maps world coordinates to NDC)

## Dependencies

```
CorePlotting
    ├── DataManager (DigitalEventSeries, PointData, EntityId, TimeFrameIndex)
    ├── SpatialIndex (QuadTree)
    ├── Entity (EntityId, EntityRegistry)
    ├── TimeFrame (TimeFrameIndex, TimeFrame)
    └── glm (matrix math)
```

**Note**: CorePlotting does NOT depend on Qt or OpenGL. It provides pure data structures and algorithms that Qt-based widgets consume.

## Key Architectural Concepts

### Relationship: PlottingManager → LayoutEngine → SceneGraph

The current `PlottingManager` in DataViewer conflates three responsibilities:

1. **Series Registry**: Storing which series are loaded (`analog_series_map`, etc.)
2. **Layout Calculation**: Computing vertical positions (`calculateAnalogSeriesAllocation()`)
3. **Global State**: Managing zoom, pan offsets, viewport bounds

In the new architecture, these responsibilities are separated:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CorePlotting                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐     ┌──────────────────┐     ┌─────────────────────┐  │
│  │   ViewState     │     │  LayoutEngine    │     │   RenderableScene   │  │
│  │  (TimeRange +   │────▶│  (allocates Y    │────▶│   (batched prims    │  │
│  │   Camera)       │     │   positions)     │     │   + MVP matrices)   │  │
│  └─────────────────┘     └──────────────────┘     └─────────────────────┘  │
│         │                        │                          │               │
│         │                        │                          │               │
│         ▼                        ▼                          ▼               │
│  ┌─────────────────┐     ┌──────────────────┐     ┌─────────────────────┐  │
│  │  TimeRange      │     │  SeriesLayout    │     │  Consumed by:       │  │
│  │  (bounds-aware  │     │  (per-series     │     │  - OpenGL Renderer  │  │
│  │   scrolling)    │     │   Model params)  │     │  - SVG Exporter     │  │
│  └─────────────────┘     └──────────────────┘     │  - Hit Testing      │  │
│                                                    └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

**SceneGraph vs LayoutEngine:**
- **LayoutEngine**: Pure calculation of "where things go" (Y positions, heights). Outputs `SeriesLayout` structs.
- **SceneGraph/RenderableScene**: The complete description of "what to draw" — geometry batches + transformation matrices. The LayoutEngine's output feeds into the Model matrices stored in the SceneGraph.

The `RenderableScene` is the **output** of the layout process, not a replacement for layout calculation.

### TimeRange: Bounds-Aware X-Axis

The current `XAxis` class is isolated and doesn't interact with `TimeFrame` objects. The new `TimeRange` integrates with `ViewState` and respects data bounds:

```cpp
// CorePlotting/CoordinateTransform/TimeRange.hpp

/**
 * @brief Bounds-aware time range for X-axis display
 * 
 * Integrates with TimeFrame to enforce valid data bounds.
 * Prevents scrolling/zooming beyond the available data range.
 */
struct TimeRange {
    // Current visible range
    int64_t start;
    int64_t end;
    
    // Hard limits from TimeFrame
    int64_t min_bound;  // First valid time index
    int64_t max_bound;  // Last valid time index
    
    /**
     * @brief Construct from a TimeFrame's valid range
     */
    static TimeRange fromTimeFrame(TimeFrame const& tf);
    
    /**
     * @brief Set visible range with automatic clamping
     */
    void setVisibleRange(int64_t new_start, int64_t new_end);
    
    /**
     * @brief Zoom centered on a point, respecting bounds
     * @return Actual range achieved (may differ due to clamping)
     */
    int64_t setCenterAndZoom(int64_t center, int64_t range_width);
    
    /**
     * @brief Get visible range width
     */
    [[nodiscard]] int64_t getWidth() const { return end - start; }
};
```

**Integration with ViewState:**

```cpp
// Extended ViewState for time-series plots
struct TimeSeriesViewState : ViewState {
    TimeRange time_range;           // X-axis bounds (time-aware)
    
    // Overrides from ViewState apply to Y-axis only
    // X-axis zoom/pan is handled through time_range
};
```

**Usage Example:**

```cpp
// In DataViewer initialization
void DataViewer_Widget::setMasterTimeFrame(std::shared_ptr<TimeFrame> tf) {
    _master_time_frame = tf;
    
    // Initialize TimeRange from TimeFrame bounds
    _view_state.time_range = TimeRange::fromTimeFrame(*tf);
    
    // Set initial visible range (e.g., first 1000 samples)
    _view_state.time_range.setCenterAndZoom(500, 1000);
}

// In mouse wheel handler
void DataViewer_Widget::handleZoom(int delta, int64_t center_time) {
    int64_t current_width = _view_state.time_range.getWidth();
    int64_t new_width = (delta > 0) ? current_width / 2 : current_width * 2;
    
    // This automatically clamps to TimeFrame bounds
    int64_t actual_width = _view_state.time_range.setCenterAndZoom(center_time, new_width);
    
    // If actual_width != new_width, we hit bounds
    updatePlot();
}

// In scroll handler  
void DataViewer_Widget::handleScroll(int64_t delta) {
    int64_t new_start = _view_state.time_range.start + delta;
    int64_t new_end = _view_state.time_range.end + delta;
    
    // This automatically clamps - user can't scroll past data bounds
    _view_state.time_range.setVisibleRange(new_start, new_end);
    
    updatePlot();
}
```

This design ensures:
1. User cannot scroll beyond the data's time bounds
2. Zooming respects minimum/maximum visible ranges
3. The relationship between `TimeFrameIndex` and display coordinates is explicit
4. TimeRange is decoupled from Qt/OpenGL but aware of TimeFrame semantics

### TimeFrame Range Adapters

Different data sources (video, neural signals, analog sensors) often have different TimeFrames with different sampling rates. The `TimeFrameAdapters` provide C++20 range adapters for converting TimeFrameIndex values to/from absolute time:

```cpp
// CorePlotting/CoordinateTransform/TimeFrameAdapters.hpp

// Forward conversion: TimeFrameIndex → absolute time
// Works with AnalogTimeSeries::TimeValueRangeView (pair<TimeFrameIndex, float>)
for (auto [abs_time, value] : series.getTimeValueRangeInTimeFrameIndexRange(start, end)
                              | toAbsoluteTime(series.getTimeFrame().get())) {
    vertices.push_back(static_cast<float>(abs_time));  // X coordinate
    vertices.push_back(value);                          // Y coordinate
}

// Works with DigitalEventSeries (bare TimeFrameIndex)
for (int abs_time : events.getEventsInRange(start, end)
                    | toAbsoluteTime(events.getTimeFrame().get())) {
    drawEventAt(abs_time);
}

// Inverse conversion: absolute time → TimeFrameIndex (for mouse interaction)
float mouse_x_time = screenXToTime(mouse_pos.x());
TimeFrameIndex hover_idx = toTimeFrameIndex(mouse_x_time, master_time_frame);

// Cross-TimeFrame conversion (series → master coordinate system)
for (auto [master_idx, value] : series.getAllSamples()
                                | toTargetFrame(series_tf, master_tf)) {
    // master_idx is now in master TimeFrame coordinates
}
```

**Key Features:**
- **Type-aware**: Automatically handles `pair<TimeFrameIndex, T>`, bare `TimeFrameIndex`, `Interval`, `EventWithId`, `IntervalWithId`
- **Pipe-friendly**: Works with C++20 ranges and `std::views::filter`, etc.
- **Bidirectional**: Forward (index→time) for rendering, inverse (time→index) for interaction
- **Cross-frame**: Align data from different sources to a master TimeFrame

## Architecture Layers


### 1. The Scene Description (CorePlotting)

CorePlotting produces a "Scene Description" — a pure data representation of what should be drawn. It does not draw. This decouples the *source* of the data from the *representation*.

To ensure high performance (e.g., 100,000+ lines), we use **Batched Primitives**. These structures hold flattened data arrays that are "GPU-ready" (layout-compatible with VBOs), minimizing allocation overhead and copying.

```cpp
/**
 * @brief A batch of lines (e.g., LineData, Epochs, or segmented AnalogSeries)
 * 
 * Designed for efficient rendering via glMultiDrawArrays or Instancing.
 * The Transformer flattens source data into these vectors.
 */
struct RenderablePolyLineBatch {
    // Geometry: Flat array of vertices {x, y, x, y...}
    // Ready for direct VBO upload (or SSBO for Compute Shaders).
    std::vector<float> vertices; 
    
    // Topology: Defines individual lines within the vertex buffer
    std::vector<int32_t> line_start_indices;
    std::vector<int32_t> line_vertex_counts;
    
    // Per-Line Attributes (Optional)
    // If size == line_count: Each line has a unique ID (e.g., Epochs).
    // If empty: All lines share 'global_entity_id' (e.g., Segmented AnalogSeries).
    std::vector<EntityId> entity_ids; 
    EntityId global_entity_id; 

    // Per-Line Colors (Optional)
    // If size == line_count: Each line has a unique color.
    // If empty: All lines use 'global_color'.
    std::vector<glm::vec4> colors;    
    
    // Global Attributes
    float thickness{1.0f};
    glm::vec4 global_color{1,1,1,1};
    
    // Model matrix for this batch (positions in world space)
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief A batch of glyphs (e.g., Events in a Raster Plot, Points)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableGlyphBatch {
    // Instance Data: {x, y} positions in world space
    std::vector<glm::vec2> positions;
    
    // Per-Glyph Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;

    // Global Attributes
    enum class GlyphType { Circle, Square, Tick, Cross };
    GlyphType glyph_type{GlyphType::Circle};
    float size{5.0f};
    
    // Model matrix for this batch
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief A batch of rectangles (e.g., DigitalIntervalSeries)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableRectangleBatch {
    // Instance Data: {x, y, width, height} per rectangle in world space
    std::vector<glm::vec4> bounds;
    
    // Per-Rectangle Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;
    
    // Model matrix for this batch
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief Complete scene description for a frame
 * 
 * Contains all primitives and shared transformation matrices.
 * Produced by LayoutEngine, consumed by Renderers.
 */
struct RenderableScene {
    // Primitive batches (each has its own Model matrix)
    std::vector<RenderablePolyLineBatch> poly_line_batches;
    std::vector<RenderableRectangleBatch> rectangle_batches;
    std::vector<RenderableGlyphBatch> glyph_batches;
    
    // Shared transformation matrices (apply to all batches)
    glm::mat4 view_matrix{1.0f};        // Camera pan/zoom
    glm::mat4 projection_matrix{1.0f};  // World → NDC mapping
    
    // Spatial index for hit testing (built from same layout as primitives)
    std::unique_ptr<QuadTree<EntityId>> spatial_index;
};
```

### 2. LayoutEngine vs SceneGraph: Clarifying Responsibilities

**LayoutEngine**, **Mappers**, and **SceneGraph (RenderableScene)** serve different purposes:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           Data Flow                                          │
│                                                                              │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │ Series Info │───▶│LayoutEngine  │───▶│ Mapper      │───▶│ SceneBuilder│  │
│  │ (counts,    │    │ (Y positions)│    │ (positions) │    │ (geometry)  │  │
│  │  types)     │    │              │    │             │    │             │  │
│  └─────────────┘    └──────────────┘    └─────────────┘    └─────────────┘  │
│                            │                   │                  │          │
│                            ▼                   ▼                  ▼          │
│                    ┌──────────────┐    ┌─────────────┐    ┌─────────────┐   │
│                    │SeriesLayout  │    │MappedPos[]  │    │Renderable   │   │
│                    │ (transforms) │    │entity_ids[] │    │Scene +      │   │
│                    │              │    │             │    │QuadTree     │   │
│                    └──────────────┘    └─────────────┘    └─────────────┘   │
│                                                                   │          │
│                                              ┌────────────────────┴───────┐  │
│                                              ▼                            ▼  │
│                                       OpenGL Renderer              SVG Export│
└──────────────────────────────────────────────────────────────────────────────┘
```

| Component | Input | Output | Responsibility |
|-----------|-------|--------|----------------|
| **LayoutEngine** | Series count, types, viewport | `SeriesLayout[]` | "What transforms apply to each series?" |
| **Mappers** | Series data + layout + TimeFrame | `positions[], entity_ids[]` | "What are the world coordinates?" |
| **SceneBuilder** | `positions[], entity_ids[]` | `RenderableScene` | "Package into GPU-ready batches + QuadTree" |
| **Renderer** | `RenderableScene` | OpenGL draw calls | "How to draw it" |
| **Interaction** | `RenderableScene.spatial_index` | EntityId | "What did user click?" |

**Key insight**: The Mapper is the visualization-specific component. Different visualizations (DataViewer, SpatialOverlay, Raster) use different Mappers but share the same SceneBuilder and Renderer.

### QuadTree Ownership

The **QuadTree lives inside `RenderableScene`** for a critical reason: **synchronization**.

Both the rendering primitives and the spatial index are built from the same position data. By bundling them together:

1. **Single Source of Truth**: When positions change, both geometry and index are rebuilt together
2. **No Sync Bugs**: Impossible for QuadTree coordinates to drift from rendered positions
3. **Clear Lifecycle**: Scene owns the index; when scene is replaced, old index is automatically cleaned up

```cpp
// SceneBuilder creates both primitives AND spatial index from same position data
// The builder receives PRE-COMPUTED positions from visualization-specific mappers

// Example: DataViewer building a scene
auto event_positions = TimeSeriesMapper::mapEvents(events, time_frame, layout);
auto analog_line = TimeSeriesMapper::mapAnalog(analog, time_frame, layout, start, end);

RenderableScene scene = SceneBuilder()
    .setBounds(bounds)
    .setViewState(view_state)
    .addGlyphs("events", event_positions.positions, event_positions.entity_ids)
    .addPolyLine("analog", analog_line.vertices, {analog_line.entity_id})
    .build();  // Automatically builds spatial index from all discrete elements

// Example: SpatialOverlay building a scene
auto point_positions = SpatialMapper::mapPoints(points, current_frame, layout);

RenderableScene scene = SceneBuilder()
    .setBounds(viewport_bounds)
    .addGlyphs("whisker_points", point_positions.positions, point_positions.entity_ids)
    .build();
```

**Key insight**: `SceneBuilder` doesn't know about `TimeFrame`, `DigitalEventSeries`, or any specific data types. It only knows about positions and entity IDs. The **Mappers** (visualization-specific) handle coordinate extraction and layout application. The **SceneBuilder** (generic) handles geometry batching and spatial indexing.

This separation allows:
1. Changing layout without touching rendering code
2. Reusing transformers across different layout strategies
3. Testing layout logic independently from OpenGL

### 3. MVP Matrix Strategy

Different plot types require different MVP strategies:

#### Spatial Plots (SpatialOverlayOpenGLWidget pattern)
All data shares one coordinate system:
```cpp
// Single MVP for all data
glm::mat4 mvp = projection * view * model;  // model = identity
viz->render(mvp);  // Same matrix for points, masks, lines
```

#### Time-Series Plots (DataViewer pattern)
Each series needs its own vertical positioning:
```cpp
// Shared V and P, per-series M
glm::mat4 view = new_getAnalogViewMat(plotting_manager);      // Global pan
glm::mat4 proj = new_getAnalogProjectionMat(time_range, ...); // Time→NDC

for (auto& series : analog_series) {
    glm::mat4 model = new_getAnalogModelMat(series.display_options, 
                                             series.std_dev, 
                                             series.mean, 
                                             layout);
    // model handles: vertical position, scaling, mean-centering
    drawSeries(model * view * proj, series.vertices);
}
```

**The key principle**: 
- **Model** = per-series positioning and scaling (what makes this series unique)
- **View** = global camera state (shared across all series)  
- **Projection** = coordinate system mapping (shared across all series)

### 3.5 Coordinate Transform Utilities (Screen ↔ World ↔ Time)

While the forward path (Data → MVP → NDC → Screen) is handled by the Model/View/Projection matrices,
user interaction requires the **inverse** path: Screen → World → Data. This is more complex for
time-series plots because:

1. **X-axis**: Screen pixels → Time coordinates (requires TimeRange)
2. **Y-axis**: Screen pixels → Data values (requires per-series Model matrix inversion)

#### Time Axis Transforms

```cpp
// CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp

struct TimeAxisParams {
    int64_t time_start;      // From TimeRange.start
    int64_t time_end;        // From TimeRange.end  
    int viewport_width_px;   // Canvas width in pixels
};

// Forward: Time → Screen pixel X
float timeToCanvasX(float time, TimeAxisParams const& params) {
    float t = (time - params.time_start) / 
              static_cast<float>(params.time_end - params.time_start);
    return t * params.viewport_width_px;
}

// Inverse: Screen pixel X → Time
float canvasXToTime(float canvas_x, TimeAxisParams const& params) {
    float t = canvas_x / params.viewport_width_px;
    return params.time_start + t * (params.time_end - params.time_start);
}
```

#### Series Y-Axis Queries (Through the Hierarchy)

The Y-axis is more complex because each series has its own coordinate space. **Instead of
flattening all parameters into a struct, we query through the existing hierarchy:**

```
canvas_y 
  → screenToWorld(view_matrix, proj_matrix)  // Generic inverse VP
  → findSeriesAtWorldY(layout_response)       // LayoutEngine knows series regions
  → worldYToSeriesLocalY(series_layout)       // Simple offset subtraction
  → [data object interprets local_y]          // Series knows its own scaling
```

```cpp
// CorePlotting/CoordinateTransform/SeriesCoordinateQuery.hpp

struct SeriesQueryResult {
    std::string series_key;
    float local_y;              // Y in series-local space (0 = center)
    float distance_from_center; // Absolute distance from allocated_y_center
    bool is_within_bounds;      // Whether point is in allocated region
};

// Query the LayoutResponse to find which series (if any) contains world_y
std::optional<SeriesQueryResult> findSeriesAtWorldY(
    float world_y,
    LayoutResponse const& layout,
    float tolerance = 0.0f);

// Convert world Y to series-local Y (simple subtraction)
float worldYToSeriesLocalY(float world_y, SeriesLayout const& layout) {
    return world_y - layout.allocated_y_center;
}
```

**Key insight**: The data object itself handles conversion from `local_y` to actual data values.
This respects that:
- Not all series are mean-centered
- Different series may have different normalization strategies
- The series object is the single source of truth for its own scaling

### 3.6 Hit Testing Architecture

Hit testing answers: "What did the user click on?" For time-series plots, we need **multiple
query strategies** because different data types have different spatial representations:

| Data Type | Query Strategy | Returns EntityId? |
|-----------|---------------|-------------------|
| DigitalEventSeries | QuadTree point query | ✅ Yes |
| DigitalIntervalSeries | Time range containment | ✅ Yes |
| AnalogTimeSeries | Y-region containment | ❌ No (continuous) |

```cpp
// CorePlotting/Interaction/HitTestResult.hpp

struct HitTestResult {
    enum class HitType { 
        None,
        AnalogSeries,       // Hit region owned by analog series (no EntityId)
        DigitalEvent,       // Hit discrete event point (has EntityId)
        IntervalBody,       // Inside interval bounds (has EntityId)
        IntervalEdgeLeft,   // Near left edge (for drag handles)
        IntervalEdgeRight,  // Near right edge
    };
    
    HitType hit_type{HitType::None};
    std::string series_key;             // Always present if hit
    std::optional<EntityId> entity_id;  // Only for events/intervals
    float world_x, world_y;             // Query point
    float distance;                     // Distance from target
};
```

#### SceneHitTester: Orchestrating Multiple Strategies

```cpp
// CorePlotting/Interaction/SceneHitTester.hpp

class SceneHitTester {
public:
    /**
     * @brief Perform hit test using all applicable strategies.
     * 
     * Queries in priority order:
     * 1. QuadTree (events) - most precise
     * 2. Interval containment - time-based
     * 3. Series region - Y-based fallback
     * 
     * Returns closest/best match.
     */
    HitTestResult hitTest(
        float world_x, 
        float world_y,
        RenderableScene const& scene,
        LayoutResponse const& layout,
        float tolerance = 5.0f) const;

private:
    // Individual strategies
    std::optional<HitTestResult> queryQuadTree(
        float x, float y, 
        RenderableScene const& scene,
        float tolerance) const;
        
    std::optional<HitTestResult> queryIntervalContainment(
        float x,
        RenderableScene const& scene) const;
        
    std::optional<HitTestResult> querySeriesRegion(
        float y,
        LayoutResponse const& layout) const;
};
```

**Data flow:**
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                        Hit Testing Data Flow                                 │
│                                                                              │
│  Mouse Event (canvas_x, canvas_y)                                            │
│         │                                                                    │
│         ▼                                                                    │
│  screenToWorld(view_matrix, proj_matrix) ──▶ (world_x, world_y)              │
│         │                                                                    │
│         ▼                                                                    │
│  ┌─────────────────────────────────────────────────────────────┐             │
│  │ SceneHitTester.hitTest(world_x, world_y, scene, layout)     │             │
│  │                                                             │             │
│  │  ┌─────────────┐  ┌──────────────────┐  ┌───────────────┐   │             │
│  │  │ QuadTree    │  │ Interval         │  │ Series Region │   │             │
│  │  │ (events)    │  │ Containment      │  │ (analog Y)    │   │             │
│  │  │ → EntityId  │  │ → EntityId       │  │ → series_key  │   │             │
│  │  └─────────────┘  └──────────────────┘  └───────────────┘   │             │
│  │         │                  │                    │           │             │
│  │         └──────────────────┼────────────────────┘           │             │
│  │                            ▼                                │             │
│  │                   Compare distances                         │             │
│  │                   Return best match                         │             │
│  └─────────────────────────────────────────────────────────────┘             │
│         │                                                                    │
│         ▼                                                                    │
│  HitTestResult { series_key, entity_id?, hit_type, distance }                │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 3.7 Interaction Controllers (State Machines)

Complex interactions like interval dragging involve state that persists across multiple
mouse events. Instead of spreading this state across widget member variables, we
encapsulate it in controller objects:

```cpp
// CorePlotting/Interaction/IntervalDragController.hpp

class IntervalDragController {
public:
    struct State {
        std::string series_key;
        bool is_left_edge;
        int64_t original_start, original_end;
        int64_t current_start, current_end;
        int64_t min_allowed, max_allowed;  // Constraints
        bool is_active{false};
    };
    
    bool tryStartDrag(HitTestResult const& hit, 
                      int64_t current_start, int64_t current_end);
    int64_t updateDrag(float canvas_x, TimeAxisParams const& params);
    std::optional<std::pair<int64_t, int64_t>> finishDrag();
    void cancelDrag();
    
    State const& getState() const { return _state; }
    
private:
    State _state;
};
```

**Benefits**:
1. **Testable**: Pass coordinates to controller, verify state changes
2. **Reusable**: Same controller works for any widget with intervals
3. **Encapsulated**: Widget doesn't manage drag state directly

### 3. Rendering Abstraction (PlottingOpenGL)

The `CorePlotting` layer produces `Renderable*Batch` structures. The `PlottingOpenGL` layer consumes them. This separation allows us to swap rendering strategies without changing the data generation logic.

We define abstract Renderers. Concrete implementations handle hardware specifics:

- **StandardPolyLineRenderer** (OpenGL 3.3/4.1): Uses `glMultiDrawArrays` or simple loops. Compatible with macOS. Ideal for `DataViewer` (fewer, longer lines).
- **ComputePolyLineRenderer** (OpenGL 4.3+): Uses SSBOs and Compute Shaders for advanced culling/selection. Ideal for `Analysis Dashboard` (100k+ short lines).

The *Application* chooses the renderer at runtime based on hardware capabilities and use case.

### 3.1 Shader Management Integration

The `ShaderManager` singleton is now consolidated into the `PlottingOpenGL` library (located in `src/PlottingOpenGL/ShaderManager/`). It provides:

1. **Centralized Shader Registry**: All shader programs are loaded once and shared across widgets
2. **Hot-Reloading**: File-based shaders can be modified at runtime during development
3. **Multi-Context Support**: Programs are managed per OpenGL context to avoid cross-context invalidation
4. **Qt Resource Loading**: Shaders can be embedded in Qt resources for deployment
5. **Compute Shader Support**: Full support for compute shaders (OpenGL 4.3+)

**PlottingOpenGL Renderer Integration:**

All PlottingOpenGL renderers (`PolyLineRenderer`, `GlyphRenderer`, `RectangleRenderer`) support dual shader loading modes:

```cpp
// Option 1: Embedded shaders (default, simple deployment)
renderer.initialize();  // Uses embedded GLSL strings as fallback

// Option 2: ShaderManager integration (recommended for development)
PolyLineRenderer renderer("shaders/line");  // Base path for .vert/.frag files
renderer.initialize();  // Loads from ShaderManager with hot-reload support
```

The renderers use the following uniform naming convention (matching existing shaders in `src/WhiskerToolbox/shaders/`):
- `u_mvp_matrix`: Model-View-Projection matrix
- `u_color`: RGBA color vector

**ShaderManager API Reference:**

```cpp
#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"

// Load a shader program
ShaderManager::instance().loadProgram(
    "plot_line",                        // Unique name
    "shaders/line.vert",                // Vertex shader
    "shaders/line.frag",                // Fragment shader
    "shaders/line.geom",                // Optional geometry shader
    ShaderSourceType::FileSystem        // or ShaderSourceType::QtResource
);

// Retrieve for use
ShaderProgram* program = ShaderManager::instance().getProgram("plot_line");
program->use();
program->setUniform("u_mvp_matrix", mvp_matrix);
```

Existing shaders are located in `src/WhiskerToolbox/shaders/` and include specialized variants for different OpenGL versions (4.1 for macOS compatibility, 4.3+ for compute shaders).

### 4. Data Transformers (The "Pipeline")

To populate the `RenderableScene`, we use **Transformers**. These pure functions convert complex data relationships into the batched primitives defined above.

**Example 1: Gap Detection (DataViewer style)**
Takes a single `AnalogTimeSeries` and breaks it into a batch of segments based on a gap threshold.
```cpp
RenderablePolyLineBatch breakIntoSegments(
    AnalogTimeSeries const& series, 
    float gap_threshold, 
    EntityId series_id
);
```

**Example 2: Epoch Alignment (Analysis Dashboard style)**
Takes an `AnalogTimeSeries` and a `DigitalEventSeries`, producing a batch of many short lines.
```cpp
RenderablePolyLineBatch createEpochs(
    AnalogTimeSeries const& signal,
    DigitalEventSeries const& alignment_events,
    TimeWindow window
);
```

## Directory Structure

```
src/CorePlotting/
├── CMakeLists.txt
├── DESIGN.md                      # This document
├── ROADMAP.md                     # Implementation roadmap
├── README.md                      # Quick start guide
│
├── Mappers/                       # Visualization-specific position mapping (NEW)
│   ├── MappedPositions.hpp        # Common output types (positions[], entity_ids[])
│   ├── TimeSeriesMapper.hpp       # DataViewer: events→time, intervals→time, analog→time
│   ├── SpatialMapper.hpp          # SpatialOverlay: points→x/y, lines→x/y
│   ├── RasterMapper.hpp           # Raster/PSTH: events→relative time
│   └── ScatterMapper.hpp          # Scatter plots: series_a→X, series_b→Y
│
├── SceneGraph/                    # Generic scene assembly (position-agnostic)
│   ├── RenderableScene.hpp        # Complete frame description
│   ├── RenderablePrimitives.hpp   # Batches (PolyLine, Rect, Glyph)
│   └── SceneBuilder.hpp           # Takes positions[], builds batches + QuadTree
│
├── Layout/                        # Plot-type-specific layout strategies
│   ├── LayoutEngine.hpp           # Strategy pattern coordinator
│   ├── SeriesLayout.hpp           # Per-series layout transforms
│   ├── StackedLayoutStrategy.hpp  # DataViewer: vertical stacking
│   ├── RowLayoutStrategy.hpp      # Raster: equal-height rows
│   └── SpatialLayoutStrategy.hpp  # SpatialOverlay: bounds fitting (NEW)
│
├── Transformers/                  # Data-to-geometry converters (low-level)
│   ├── GapDetector.hpp            # Analog → segmented PolyLineBatch
│   ├── EpochAligner.hpp           # Analog + Events → PolyLineBatch
│   └── RasterBuilder.hpp          # Events → GlyphBatch (legacy, being superseded by Mappers)
│
├── CoordinateTransform/           # MVP matrix utilities + inverse transforms
│   ├── ViewState.hpp              # Camera state (zoom, pan, bounds)
│   ├── TimeRange.hpp              # TimeFrame-aware X-axis bounds
│   ├── TimeAxisCoordinates.hpp    # Canvas X ↔ Time conversions
│   ├── SeriesCoordinateQuery.hpp  # World Y → Series lookup (queries LayoutResponse)
│   ├── Matrices.hpp               # Matrix construction helpers
│   ├── ScreenToWorld.hpp          # Generic inverse VP transform
│   └── SeriesMatrices.hpp         # Per-series Model matrix builders
│
├── Interaction/                   # Hit testing and interaction controllers
│   ├── HitTestResult.hpp          # Result struct with optional EntityId
│   ├── SceneHitTester.hpp         # Multi-strategy hit testing through SceneGraph
│   ├── IntervalDragController.hpp # State machine for interval edge dragging
│   └── IntervalCreationController.hpp # State machine for new interval creation
│
├── DataTypes/                     # Style and configuration structs
│   ├── SeriesStyle.hpp            # Color, alpha, thickness (rendering)
│   └── DigitalEventSeries/        # Event-specific types
│
├── SpatialAdapter/                # Bridges data types to QuadTree
│   ├── ISpatiallyIndexed.hpp      # Common interface for spatial queries
│   ├── EventSpatialAdapter.hpp    # DigitalEventSeries → QuadTree (stacked/raster)
│   ├── PointSpatialAdapter.hpp    # RenderableGlyphBatch → QuadTree
│   └── PolyLineSpatialAdapter.hpp # RenderablePolyLineBatch → QuadTree (3 strategies)
│
├── EventRow/                      # Row-based event model
│   ├── EventRow.hpp               # Data description of an event row
│   ├── EventRowLayout.hpp         # Layout/positioning information
│   ├── PlacedEventRow.hpp         # Combined: data + layout
│   └── EventRowBuilder.hpp        # Factory functions for common patterns
│
├── Export/                        # SVG and other export formats
│   ├── SVGPrimitives.hpp          # Batch → SVG element conversion
│   └── SVGPrimitives.cpp
│
└── tests/                         # Catch2 unit tests
    ├── TimeSeriesMapper.test.cpp  # NEW
    ├── SpatialMapper.test.cpp     # NEW
    ├── EventRow.test.cpp
    ├── TimeRange.test.cpp
    ├── LayoutEngine.test.cpp
    └── CoordinateTransform.test.cpp
```

## Core Concepts

### 1. The Row Model

A **row** represents a horizontal band of events on a canvas. Each row:
- Contains events from a single `DigitalEventSeries`
- Has a **center time** defining the x-axis origin for display
- Is positioned at a **y-coordinate** in world space

```cpp
/**
 * @brief Pure data description of an event row
 * 
 * Contains only the logical relationship: which series, what x-offset.
 * Does NOT know about rendering position.
 */
struct EventRow {
    DigitalEventSeries const* series;   // Source data (not owned)
    TimeFrameIndex center_time;          // X-axis origin for this row
};
```

### 2. Layout vs Data Separation

Layout information is computed by a PlottingManager or equivalent and kept separate from the data:

```cpp
/**
 * @brief Layout information for a row on the canvas
 * 
 * Computed by PlottingManager. This IS the "Model transform"
 * expressed as simple values rather than a matrix.
 */
struct EventRowLayout {
    float y_center;     // World Y coordinate for row center
    float row_height;   // Height allocated to this row in world coords
};

/**
 * @brief Combined: data + layout = ready for rendering and indexing
 */
struct PlacedEventRow {
    EventRow row;
    EventRowLayout layout;
    int row_index;      // For identification (which row was clicked)
};
```

### 3. Coordinate Spaces & Interaction Strategy

We adopt the **World Space Strategy** successfully used by `SpatialOverlayOpenGLWidget`.

1.  **Static Buffers**: `Renderable*Batch` data is generated in **World Space** (Time, Layout Y). It is uploaded to the GPU once and *not* modified during zoom/pan.
2.  **GPU Transformation**: Zooming and panning are handled entirely by the **Vertex Shader** via View/Projection matrices.
3.  **Inverse Querying**: For interaction (hover/selection), the CPU transforms the mouse position from **Screen Space → World Space** and queries the **World Space QuadTree**.

**Why this works:**
- **Performance**: No CPU re-calculation or buffer re-upload during zoom/pan.
- **Consistency**: The QuadTree and the Render Buffer are always in sync (both in World Space).
- **Proven**: This is exactly how `SpatialOverlayOpenGLWidget` handles 100,000+ points with real-time zooming.

```
User hovers at screen (px, py)
         │
         ▼
Screen → World transform: inverse(P * V) applied to NDC coords
         │
         ▼
QuadTree.findNearest(world_x, world_y, world_tolerance)
         │
         ▼
Returns: QuadTreePoint<EntityId>* → {x, y, entity_id}
```

### 4. Spatial Indexing Strategy

Spatial adapters provide multiple strategies for building `QuadTree<EntityId>` indexes from different data types.

#### Event Spatial Indexing

```cpp
// For stacked events (DataViewer style - absolute time)
auto index = EventSpatialAdapter::buildStacked(
    event_series, time_frame, layout, bounds);

// For raster plots (EventPlotWidget style - relative time per row)
auto index = EventSpatialAdapter::buildRaster(
    event_series, time_frame, row_layouts, row_centers, bounds);

// For pre-computed positions
auto index = EventSpatialAdapter::buildFromPositions(
    positions, entity_ids, bounds);
    
    for (auto const& placed : placed_rows) {
        float y = placed.layout.y_center;  // World Y = same as Model matrix Y
        
        for (auto const& event : placed.row.series->view()) {
            float x = static_cast<float>(
                event.time.getValue() - placed.row.center_time.getValue()
            );
            index->insert(x, y, event.entity_id);
        }
    }
    
    return index;
}
```

#### PolyLine Spatial Indexing

Polyline adapters provide three indexing strategies optimized for different use cases:

```cpp
// Strategy 1: Insert every vertex (best for sparse data)
auto index = PolyLineSpatialAdapter::buildFromVertices(batch, bounds);

// Strategy 2: Insert AABB corners + center (efficient for dense data)
auto index = PolyLineSpatialAdapter::buildFromBoundingBoxes(batch, bounds);

// Strategy 3: Uniformly sample points along lines (best for very long lines)
auto index = PolyLineSpatialAdapter::buildFromSampledPoints(
    batch, sample_interval, bounds);
```

#### Point/Glyph Spatial Indexing

```cpp
// Build from RenderableGlyphBatch (events, points)
auto index = PointSpatialAdapter::buildFromGlyphs(glyph_batch, bounds);

// Build from explicit positions
auto index = PointSpatialAdapter::buildFromPositions(
    positions, entity_ids, bounds);
```

**Key insight**: The QuadTree Y coordinate uses the **same value** as the Model matrix's Y translation. This ensures rendering and queries are always synchronized.

### 5. EntityID as Complete Reference

When the user hovers over or selects an event, the QuadTree returns an `EntityId`. This is sufficient to:

1. **Frame jump**: Look up `TimeFrameIndex` via `DigitalEventSeries::getEventsWithIdsInRange()` or EntityRegistry
2. **Group coloring**: Query GroupManager for color
3. **Selection tracking**: Store EntityId in selection set

No reverse transformation (adding back center times) is needed — the EntityId directly references the original event.

## Usage Patterns

### DataViewer Stacked Events

All rows share the same center (typically 0), displaying absolute time:

```cpp
std::vector<PlacedEventRow> buildDataViewerRows(
    std::vector<std::pair<std::string, DigitalEventSeries const*>> const& series_list,
    PlottingManager const& manager)
{
    std::vector<PlacedEventRow> rows;
    
    for (int i = 0; i < series_list.size(); ++i) {
        auto const& [key, series] = series_list[i];
        
        float y_center, row_height;
        manager.calculateDigitalEventSeriesAllocation(i, y_center, row_height);
        
        rows.push_back({
            .row = {
                .series = series,
                .center_time = TimeFrameIndex(0)  // All same center
            },
            .layout = {
                .y_center = y_center,
                .row_height = row_height
            },
            .row_index = i
        });
    }
    
    return rows;
}
```

### Raster Plot (EventPlotWidget)

Each row represents a trial with its own center time:

```cpp
std::vector<PlacedEventRow> buildRasterRows(
    DigitalEventSeries const* event_series,
    std::vector<TimeFrameIndex> const& trial_centers,
    float row_height)
{
    std::vector<PlacedEventRow> rows;
    
    for (int i = 0; i < trial_centers.size(); ++i) {
        rows.push_back({
            .row = {
                .series = event_series,          // Same series for all rows!
                .center_time = trial_centers[i]  // Different center per row
            },
            .layout = {
                .y_center = static_cast<float>(i),  // Row index as Y
                .row_height = row_height
            },
            .row_index = i
        });
    }
    
    return rows;
}
```

**Note**: The same `DigitalEventSeries` appears in every row with different centers. The same EntityId may appear at different (x, y) positions across rows.

## Query Flow

```
User hovers at screen (px, py)
         │
         ▼
Screen → World transform: inverse(P * V) applied to NDC coords
         │
         ▼
QuadTree.findNearest(world_x, world_y, world_tolerance)
         │
         ▼
Returns: QuadTreePoint<EntityId>* → {x, y, entity_id}
         │
         ▼
EntityId used for:
  - Frame jump: series->getDataByEntityId(entity_id) → TimeFrameIndex
  - Highlight: add to selection set
  - Group color: GroupManager lookup
```

## Model Matrix Construction

The Model matrix uses the same layout values as the QuadTree:

```cpp
glm::mat4 getEventModelMatrix(PlacedEventRow const& placed) {
    glm::mat4 M(1.0f);
    
    // Scale: tick marks span [-1, +1] in model space
    // Scale to fit allocated height with margin
    M[1][1] = placed.layout.row_height * 0.5f * 0.9f;
    
    // Translate: move row center to world position
    M[3][1] = placed.layout.y_center;  // SAME value used in QuadTree
    
    return M;
}
```

## Interface: ISpatiallyIndexed

Common interface for widgets that support spatial queries:

```cpp
/**
 * @brief Interface for spatially-indexed visualizations
 */
class ISpatiallyIndexed {
public:
    virtual ~ISpatiallyIndexed() = default;
    
    /**
     * @brief Find entity at screen position
     */
    virtual std::optional<EntityId> findEntityAtPosition(
        float screen_x, float screen_y) const = 0;
    
    /**
     * @brief Find entities within screen-space box
     */
    virtual std::vector<EntityId> findEntitiesInBox(
        BoundingBox const& screen_box) const = 0;
    
    /**
     * @brief Get source time for entity (for frame jump)
     */
    virtual std::optional<TimeFrameIndex> getSourceTime(EntityId id) const = 0;
};
```

## Spatial Adapters (Phase 1.5)

Spatial adapters provide factory methods for building `QuadTree<EntityId>` indexes from different data sources.

### Event Spatial Indexing

```cpp
// For stacked events (DataViewer style - absolute time)
auto index = EventSpatialAdapter::buildStacked(
    event_series, time_frame, layout, bounds);

// For raster plots (EventPlotWidget style - relative time per row)
auto index = EventSpatialAdapter::buildRaster(
    event_series, time_frame, row_layouts, row_centers, bounds);

// For pre-computed positions
auto index = EventSpatialAdapter::buildFromPositions(
    positions, entity_ids, bounds);
```

### PolyLine Spatial Indexing

Polyline adapters provide three indexing strategies:

```cpp
// Strategy 1: Insert every vertex (best for sparse data)
auto index = PolyLineSpatialAdapter::buildFromVertices(batch, bounds);

// Strategy 2: Insert AABB corners + center (efficient for dense data)
auto index = PolyLineSpatialAdapter::buildFromBoundingBoxes(batch, bounds);

// Strategy 3: Uniformly sample points along lines (best for very long lines)
auto index = PolyLineSpatialAdapter::buildFromSampledPoints(
    batch, sample_interval, bounds);
```

### Point/Glyph Spatial Indexing

```cpp
// Build from RenderableGlyphBatch (events, points)
auto index = PointSpatialAdapter::buildFromGlyphs(glyph_batch, bounds);

// Build from explicit positions
auto index = PointSpatialAdapter::buildFromPositions(
    positions, entity_ids, bounds);
```

**Architecture**: All adapters return `std::unique_ptr<QuadTree<EntityId>>` and use static factory methods. The QuadTree coordinates match the Model matrix coordinates, ensuring rendering and spatial queries are synchronized.

```

## Future Extensions

### Multi-Type Canvas

A canvas can contain multiple data types with separate QuadTrees or a unified index:

```cpp
// Build separate indexes for different data types
auto event_index = EventSpatialAdapter::buildStacked(
    event_series, time_frame, layout, bounds);
auto polyline_index = PolyLineSpatialAdapter::buildFromVertices(
    polyline_batch, bounds);

// Store in scene or widget
struct CanvasSpatialIndex {
    std::unique_ptr<QuadTree<EntityId>> event_index;
    std::unique_ptr<QuadTree<EntityId>> polyline_index;
    std::unique_ptr<QuadTree<EntityId>> point_index;
    
    std::optional<EntityId> findNearest(float x, float y, float tolerance) const {
        // Query both, return closest
    }
};
```

## Testing Strategy

### Layer 1: Pure Geometry (No Qt/OpenGL)

Test coordinate transformations and layout calculations:

```cpp
TEST_CASE("EventRow coordinate transform") {
    DigitalEventSeries series;
    series.addEvent(TimeFrameIndex(150));
    
    EventRow row{&series, TimeFrameIndex(100)};  // center = 100
    
    // Event at time 150 should display at x = 50
    auto x = computeDisplayX(row, TimeFrameIndex(150));
    REQUIRE(x == 50.0f);
}
```

### Layer 2: Spatial Index Integration

Test QuadTree building and queries:

```cpp
TEST_CASE("buildEventSpatialIndex creates correct index") {
    // Setup series with known events and EntityIds
    // Build index
    // Verify findNearest returns correct EntityId
}
```

### Layer 3: MVP Coordination

Test that Model matrix Y matches QuadTree Y:

```cpp
TEST_CASE("Model matrix and QuadTree use same Y coordinate") {
    PlacedEventRow placed = /* ... */;
    
    auto M = getEventModelMatrix(placed);
    float model_y = M[3][1];  // Y translation
    
    REQUIRE(model_y == placed.layout.y_center);
    // Same value used when inserting into QuadTree
}
```

## Summary

### Component Overview (Updated with Position Mappers)

| Component | Location | Purpose |
|-----------|----------|---------|
| **Mappers** | `Mappers/` | **Visualization-specific**: Convert data + layout → positions[] |
| `TimeSeriesMapper` | `Mappers/` | DataViewer: events/intervals/analog → time-based positions |
| `SpatialMapper` | `Mappers/` | SpatialOverlay: points/lines → direct X/Y positions |
| `RasterMapper` | `Mappers/` | Raster/PSTH: events → relative-time positions |
| **SceneBuilder** | `SceneGraph/` | **Generic**: positions[] + entity_ids[] → RenderableScene |
| `TimeRange` | `CoordinateTransform/` | TimeFrame-aware X-axis bounds management |
| `TimeSeriesViewState` | `CoordinateTransform/` | Combined time + Y-axis camera state |
| `LayoutEngine` | `Layout/` | Calculates layout transforms for series positioning |
| `SeriesLayout` | `Layout/` | Output: X/Y transforms (offset, scale, bounds) |
| `RenderableScene` | `SceneGraph/` | Complete frame description (batches + V/P matrices + QuadTree) |
| `Renderable*Batch` | `SceneGraph/` | GPU-ready geometry + Model matrix per batch |
| `QuadTree` | Owned by `RenderableScene` | Spatial index for hit testing (built from same positions as rendering) |
| `SceneHitTester` | `Interaction/` | Multi-strategy hit testing through SceneGraph |

### Data Flow Summary (Updated)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Complete Data Flow                                  │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                    Visualization-Specific Layer                      │   │
│   │                                                                      │   │
│   │   Series Data ──▶ LayoutEngine ──▶ SeriesLayout (transforms)        │   │
│   │        │                              │                              │   │
│   │        │                              ▼                              │   │
│   │        └──────────────▶ Mapper ──▶ positions[], entity_ids[]        │   │
│   │                                       │                              │   │
│   │   (TimeSeriesMapper, SpatialMapper, RasterMapper, etc.)             │   │
│   └───────────────────────────────────────┼──────────────────────────────┘   │
│                                           ▼                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                       Generic Layer                                  │   │
│   │                                                                      │   │
│   │   positions[] ──▶ SceneBuilder ──▶ RenderableScene                  │   │
│   │                         │                  │                         │   │
│   │                         │       ┌──────────┴──────────┐              │   │
│   │                         │       ▼                     ▼              │   │
│   │                         ▼   QuadTree          Renderable*Batches     │   │
│   │                   (both use same                                     │   │
│   │                    positions[])                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                           │                                  │
│                              ┌────────────┴────────────┐                    │
│                              ▼                         ▼                    │
│                       OpenGL Renderer            SVG Exporter               │
│                       (PlottingOpenGL)           (CorePlotting/Export)      │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Architectural Principles

1. **Mapper is the boundary**: Left of mapper = data semantics, Right of mapper = geometry
2. **SceneBuilder is data-type-agnostic**: Only knows positions[], entity_ids[], not TimeFrame or series types
3. **Positions computed once, used twice**: Same positions[] feed both rendering batches and QuadTree
4. **Each visualization type has its own Mapper**: DataViewer, SpatialOverlay, Raster use different mappers
5. **Layout + Mapper = Complete Position**: LayoutEngine provides transforms, Mapper applies them to data

### Key Invariants

1. **QuadTree positions = Rendered positions**: Both built from same Mapper output
2. **TimeRange respects TimeFrame bounds**: User cannot scroll/zoom beyond valid data
3. **Model = per-series, View/Projection = shared**: Consistent MVP pattern for time-series
4. **RenderableScene is renderer-agnostic**: Same scene feeds OpenGL and SVG export

### Migration from Current Architecture

| Current (DataViewer) | New (CorePlotting) |
|---------------------|---------------------|
| `XAxis` | `TimeRange` (TimeFrame-aware) |
| `PlottingManager` series storage | Removed (single source in widget) |
| `PlottingManager` allocation methods | `LayoutEngine` → `SeriesLayout` |
| `NewAnalogTimeSeriesDisplayOptions` | Split into `SeriesStyle` + `SeriesLayout` |
| `MVP_*.cpp` in `DataViewer/` | `SeriesMatrices.cpp` in `CorePlotting/` |
| Inline vertex generation | `Mapper` → `SceneBuilder` → `RenderableBatch` |
| `SceneBuilder.addEventSeries(series, tf)` | `Mapper.mapEvents()` → `SceneBuilder.addGlyphs(positions)` |
| `_verticalPanOffset`, `_yMin`, `_yMax` | `ViewState` |

