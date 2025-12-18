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

Layout transforms convert raw values to world coordinates using a simple affine model:

```cpp
// Core/Layout/LayoutTransform.hpp

/**
 * @brief Pure geometric transform: output = input * gain + offset
 * 
 * This is the fundamental building block for positioning data in world space.
 * The LayoutEngine outputs these, and they become part of the Model matrix.
 * 
 * Key insight: Layout is purely geometric (offset + gain). Data normalization
 * (z-score, peak-to-peak, etc.) is widget-specific and computed via helper functions.
 */
struct LayoutTransform {
    float offset{0.0f};  ///< Translation (applied after scaling)
    float gain{1.0f};    ///< Scale factor
    
    /// Apply transform: output = input * gain + offset
    [[nodiscard]] float apply(float value) const {
        return value * gain + offset;
    }
    
    /// Inverse transform: recover original value
    [[nodiscard]] float inverse(float transformed) const;
    
    /// Compose transforms: result applies this AFTER other
    [[nodiscard]] LayoutTransform compose(LayoutTransform const& other) const {
        return { other.offset * gain + offset, other.gain * gain };
    }
    
    /// Convert to 4x4 Model matrix for Y-axis transform
    [[nodiscard]] glm::mat4 toModelMatrixY() const;
    
    /// Convert to 4x4 Model matrix for X-axis transform
    [[nodiscard]] glm::mat4 toModelMatrixX() const;
    
    [[nodiscard]] static LayoutTransform identity() { return {0.0f, 1.0f}; }
};
```

#### Normalization Helpers

Data normalization (z-score, peak-to-peak, etc.) is **widget-specific**—the layout layer
should not know about standard deviations or data ranges. Instead, normalization is
computed separately and composed with layout transforms:

```cpp
// Core/Layout/NormalizationHelpers.hpp

namespace NormalizationHelpers {
    /// Z-score normalization: centers on mean, scales by std_dev
    /// Result: (value - mean) / std_dev
    [[nodiscard]] LayoutTransform forZScore(float mean, float std_dev);
    
    /// Peak-to-peak normalization: maps [min, max] → [target_min, target_max]
    [[nodiscard]] LayoutTransform forPeakToPeak(float min, float max,
                                                 float target_min = -1.0f,
                                                 float target_max = 1.0f);
    
    /// Standard deviation range: maps mean ± num_std_devs*std → [-1, +1]
    [[nodiscard]] LayoutTransform forStdDevRange(float mean, float std_dev,
                                                  float num_std_devs = 3.0f);
    
    /// Unit range normalization: maps [min, max] → [0, 1]
    [[nodiscard]] LayoutTransform forUnitRange(float min, float max);
    
    /// Percentile range normalization  
    [[nodiscard]] LayoutTransform forPercentileRange(float p_low_value, float p_high_value,
                                                      float target_min = -1.0f,
                                                      float target_max = 1.0f);
    
    /// Centered normalization: subtracts center, no scaling
    [[nodiscard]] LayoutTransform forCentered(float center);
    
    /// Manual gain/offset
    [[nodiscard]] LayoutTransform manual(float offset, float gain);
}
```

#### Usage Pattern: Composing Transforms

The widget computes the final transform by composing normalization with layout:

```cpp
// Widget code (DataViewer style)
void buildAnalogSeriesBatch(series_key, series_data, layout, view_state) {
    // 1. Get layout transform from LayoutEngine
    SeriesLayout const& series_layout = layout_response.findLayout(series_key);
    
    // 2. Compute data normalization (widget-specific)
    float mean = series_config.data_cache.cached_mean;
    float std_dev = series_config.data_cache.cached_std_dev;
    LayoutTransform data_norm = NormalizationHelpers::forStdDevRange(mean, std_dev, 3.0f);
    
    // 3. Apply user adjustments
    LayoutTransform user_adj = NormalizationHelpers::manual(
        series_config.user_vertical_offset,
        series_config.user_scale_factor);
    
    // 4. Compose: data_norm → user_adj → layout
    LayoutTransform final_transform = series_layout.y_transform
        .compose(user_adj)
        .compose(data_norm);
    
    // 5. Create model matrix directly
    glm::mat4 model_matrix = final_transform.toModelMatrixY();
    
    // 6. Build batch with model matrix
    PolyLineStyle style { model_matrix, config.color, config.thickness };
    builder.addPolyLine(key, mapped_vertices, style);
}
```

#### SeriesLayout Structure

```cpp
// Core/Layout/SeriesLayout.hpp

/**
 * @brief Complete layout specification for a single series
 */
struct SeriesLayout {
    std::string series_id;              ///< Unique identifier
    LayoutTransform y_transform;        ///< Y-axis transform (offset=center, gain=half_height)
    LayoutTransform x_transform;        ///< X-axis transform (usually identity)
    int series_index{0};                ///< Index for ordering
    
    /// Convenience: compute Y model matrix from y_transform
    [[nodiscard]] glm::mat4 computeModelMatrix() const {
        return y_transform.toModelMatrixY();
    }
};
```

**Convention for y_transform:**
- `offset` = center Y position in world coordinates
- `gain` = half-height of allocated region (so normalized [-1,+1] maps to allocated height)

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

// For multi-line data (LineData, event-aligned traces)
// A view over a single mapped line with lazy vertex iteration
struct MappedLineView {
    EntityId entity_id;
    // Returns transformed vertices as a range (lazy, zero-copy)
    auto vertices() const -> /* range of glm::vec2 */;
};

// Concept for line ranges (range of MappedLineView)
template<typename R>
concept MappedLineRange = std::ranges::input_range<R> &&
    requires(std::ranges::range_value_t<R> line) {
        { line.entity_id } -> std::convertible_to<EntityId>;
        { line.vertices() } -> std::ranges::input_range;
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
    
    // Returns range of MappedLineView (each line has entity_id + vertices sub-range)
    auto mapLines(
        LineData const& lines,
        TimeFrameIndex at_time,
        SeriesLayout const& layout) -> /* MappedLineRange */;
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

// EventAlignedMapper - analog traces aligned to events (PSTH-style line plots)
// **PLANNED** - Not yet implemented. See Mappers/ for available mappers.
namespace EventAlignedMapper {
    enum class TrialLayoutMode {
        Overlaid,   // All trials at same Y, differentiated by color/alpha
        Stacked,    // Each trial gets vertical offset (waterfall plot)
        Averaged    // Compute mean ± SEM, single line with error band
    };
    
    // Each alignment event produces one polyline (one "trial")
    // X = relative time from alignment event
    // Y = analog value (optionally stacked per trial)
    auto mapTrials(
        AnalogTimeSeries const& series,
        DigitalEventSeries const& align_events,
        TimeFrame const& analog_tf,
        TimeFrame const& events_tf,
        float window_before,      // e.g., 0.5 seconds before event
        float window_after,       // e.g., 1.0 seconds after event
        TrialLayoutMode layout_mode,
        SeriesLayout const& layout) -> /* MappedLineRange */;
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

**Event-Aligned Analog Traces (PSTH-style line plot):**
```cpp
// 1. Layout for trial stacking (or overlaid)
auto layout = RowLayout().compute(num_events, -1.0f, 1.0f);  // Stacked
// auto layout = SpatialLayout().compute(...);  // Overlaid

// 2. Build scene - EventAlignedMapper produces MappedLineRange
//    Each alignment event becomes one polyline (trial)
//    X = relative time from event, Y = analog value
RenderableScene scene = SceneBuilder()
    .setBounds(plot_bounds)
    .addPolyLines("aligned_traces",
                  EventAlignedMapper::mapTrials(
                      analog_series, stimulus_events,
                      analog_tf, events_tf,
                      /*window_before=*/0.5f, /*window_after=*/1.0f,
                      EventAlignedMapper::TrialLayoutMode::Stacked,
                      layout))
    .build();

// Hit testing: Use compute shader approach (same as LineData)
// Many polylines benefit from GPU-parallel intersection testing
```

**SpatialOverlay with Dense Lines (LineData):**
```cpp
// 1. Spatial layout fits data bounds to viewport
auto layout = SpatialLayout().compute(data_bounds, viewport_bounds);

// 2. Build scene - SpatialMapper::mapLines returns MappedLineRange
//    Each Line2D becomes one entry with entity_id + vertices sub-range
RenderableScene scene = SceneBuilder()
    .setBounds(viewport_bounds)
    .addPolyLines("whiskers",
                  SpatialMapper::mapLines(whisker_lines, current_frame, layout))
    .build();

// Hit testing: Compute shader (100k+ lines)
// See LineDataVisualization for reference implementation
```

### Key Design Principles

1. **SceneBuilder consumes ranges, never vectors** — no intermediate allocations
2. **Single traversal for x, y, entity_id** — data flows directly to GPU buffer + spatial index
3. **Data objects can provide mapped views** — `points.viewAtTime(frame, layout)` short-circuits the Mapper
4. **Mappers are visualization-centric, not data-centric** — e.g., `EventAlignedMapper` takes `AnalogTimeSeries` + `DigitalEventSeries` and outputs `MappedLineRange`
5. **Layout engines are plot-type specific** — but share a common output format (`SeriesLayout`)
6. **Data type coupling is acceptable** — CorePlotting knows about DataManager types; this is intentional
7. **Hit testing strategy is data-type-dependent** — see below

### Hit Testing Strategies

**Spatial indexing is optional and depends on data characteristics:**

| Data Type | Hit Test Strategy | Reason |
|-----------|-------------------|--------|
| Events (discrete, sparse) | QuadTree | O(log n) lookup, low element count |
| Points (discrete, sparse) | QuadTree | O(log n) lookup, low element count |
| Intervals | Time range query | 1D containment test on time axis |
| Lines (dense, 100k+) | **Compute Shader** | GPU parallelism beats QuadTree for dense data |
| Event-aligned traces | **Compute Shader** | Many polylines, same as LineData |
| Analog series | Region + interpolate | Y region check + X interpolation |

**For dense polyline data** (LineData, event-aligned traces), the existing `LineDataVisualization` pattern is preferred:
- All vertices stored in flat GPU buffer
- Line ranges tracked separately (`start_vertex`, `vertex_count` per line)
- Compute shader performs line-line intersection in NDC space
- Selection/visibility via GPU mask buffers (SSBOs)

This approach handles 100k+ lines efficiently where QuadTree would struggle.

### Directory Structure

```
src/CorePlotting/
├── CMakeLists.txt
├── DESIGN.md                      # This document
├── ROADMAP.md                     # Implementation roadmap
│
├── Mappers/                       # Visualization-specific coordinate mapping
│   ├── MappedElement.hpp          # Common element types (MappedElement, MappedRectElement, MappedVertex)
│   ├── MappedLineView.hpp         # Line view type for multi-line ranges
│   ├── MapperConcepts.hpp         # C++20 concepts for range requirements
│   ├── TimeSeriesMapper.hpp       # DataViewer: events→time, intervals→time, analog→time
│   ├── SpatialMapper.hpp          # SpatialOverlay: points→x/y, lines→x/y
│   └── RasterMapper.hpp           # Raster/PSTH: events→relative time
│   # PLANNED: EventAlignedMapper.hpp — Event-aligned traces: analog+events→MappedLineRange
│
├── Layout/                        # Plot-type-specific layout strategies
│   ├── LayoutEngine.hpp           # Strategy pattern coordinator
│   ├── LayoutEngine.cpp
│   ├── SeriesLayout.hpp           # Per-series layout result
│   ├── LayoutTransform.hpp        # Affine transform: output = input * gain + offset
│   ├── NormalizationHelpers.hpp   # Data normalization → LayoutTransform
│   ├── StackedLayoutStrategy.hpp  # DataViewer: vertical stacking
│   ├── StackedLayoutStrategy.cpp
│   ├── RowLayoutStrategy.hpp      # Raster: equal-height rows
│   ├── RowLayoutStrategy.cpp
│   ├── SpatialLayoutStrategy.hpp  # SpatialOverlay: bounds fitting
│   ├── SpatialLayoutStrategy.cpp
│   └── README.md
│
├── SceneGraph/                    # Generic scene assembly (range-based API)
│   ├── SceneBuilder.hpp           # Fluent builder consuming ranges
│   ├── SceneBuilder.cpp
│   └── RenderablePrimitives.hpp   # Batches (PolyLine, Rect, Glyph) + RenderableScene
│
├── CoordinateTransform/           # MVP matrix utilities + inverse transforms
│   ├── ViewState.hpp              # Camera state (zoom, pan, bounds)
│   ├── ViewState.cpp
│   ├── TimeRange.hpp              # TimeFrame-aware X-axis bounds
│   ├── TimeAxisCoordinates.hpp    # Canvas X ↔ Time, Canvas Y ↔ World Y conversions
│   ├── TimeFrameAdapters.hpp      # C++20 range adapters for TimeFrame conversion
│   ├── SeriesCoordinateQuery.hpp  # World Y → Series lookup
│   ├── Matrices.hpp               # Matrix construction helpers
│   ├── Matrices.cpp
│   ├── ScreenToWorld.hpp          # Generic inverse VP transform
│   ├── ScreenToWorld.cpp
│   ├── SeriesMatrices.hpp         # Per-series Model matrix builders + inverse
│   └── SeriesMatrices.cpp
│
├── Interaction/                   # Hit testing and interaction controllers
│   ├── HitTestResult.hpp          # Result struct with optional EntityId
│   ├── SceneHitTester.hpp         # Multi-strategy hit testing
│   └── IntervalDragController.hpp # State machine for interval edge dragging
│
├── DataTypes/                     # Style and configuration structs
│   ├── SeriesStyle.hpp            # Color, alpha, thickness (rendering)
│   ├── SeriesLayoutResult.hpp     # Layout output (y_center, height)
│   └── SeriesDataCache.hpp        # Cached statistics (std_dev, mean)
│
├── Transformers/                  # Data-to-geometry converters
│   ├── GapDetector.hpp            # Analog → segmented PolyLineBatch
│   └── GapDetector.cpp
│
└── Export/                        # SVG and other export formats
    ├── SVGPrimitives.hpp          # Batch → SVG element conversion
    └── SVGPrimitives.cpp
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

## Architecture Status

### Completed Migrations

The following architectural improvements have been completed:

1. ✅ **TimeRange replaces XAxis**: The legacy `XAxis` class has been replaced with `TimeRange`, which integrates with `TimeFrame` to enforce valid data bounds and prevent scrolling/zooming beyond the available data range.

2. ✅ **Unified series storage**: Eliminated duplicate series storage. Series data now lives only in `OpenGLWidget`. Layout computation is handled by `LayoutEngine` with no data storage.

3. ✅ **DisplayOptions split**: Split `DisplayOptions` into three separate structs:
   - `SeriesStyle`: Rendering properties (color, alpha, thickness)
   - `SeriesLayoutResult`: Layout output (allocated_y_center, allocated_height)
   - `SeriesDataCache`: Cached calculations (std_dev, mean)

4. ✅ **Scene-based rendering**: `OpenGLWidget` now uses `SceneBuilder` to construct `RenderableScene` objects, separating data preparation from rendering.

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

### Single Source of Truth: ViewState for Camera State

**Design Decision (December 2025)**: `ViewState` is the single source of truth for all
zoom and pan state. The `LayoutEngine` does NOT contain zoom/pan fields.

**Rationale**:
- **ViewState** is already rendering-aware (viewport dimensions, projection matrices)
- **LayoutEngine** computes *relative* series positioning (Model matrices), not camera state
- **Separation of concerns**: Layout = "where is series N relative to series M", 
  ViewState = "what part of world do we see"

**MVP Composition in Widgets**:
```cpp
// 1. LayoutEngine computes series positions (no zoom/pan)
LayoutResponse layout = _layout_engine.compute(request);

// 2. ViewState handles zoom/pan → View/Projection matrices
glm::mat4 view, proj;
computeMatricesFromViewState(_view_state, view, proj);

// 3. Compose for rendering
for (auto const& series_layout : layout.layouts) {
    glm::mat4 model = series_layout.computeModelMatrix();
    glm::mat4 mvp = proj * view * model;
    // Render with mvp...
}
```

**When to Recalculate**:
- **Layout recalculation**: Series added/removed, viewport bounds fundamentally change
- **No layout recalculation**: User zooms or pans (only ViewState changes)

This makes zoom/pan extremely cheap (just update ViewState and redraw), while
layout changes are more expensive but rare.

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

### TimeSeriesViewState: Real-Time Plotting View Model

`TimeSeriesViewState` is the view state model for time-series plotting widgets like DataViewer/OpenGLWidget. It is architecturally distinct from the general `ViewState` used for spatial plots:

| Aspect         | ViewState (Spatial)     | TimeSeriesViewState (Real-time)  |
|----------------|-------------------------|----------------------------------|
| Buffer scope   | All data loaded once    | Only visible time window         |
| X zoom         | MVP transform           | Triggers buffer rebuild          |
| X pan          | MVP transform           | External (scrollbar, sync)       |
| Y zoom/pan     | MVP transform           | MVP transform                    |
| Bounds         | Data bounds enforced    | No bounds—blank areas allowed    |

```cpp
// CorePlotting/CoordinateTransform/TimeRange.hpp

/**
 * @brief View state for time-series plots with real-time/streaming paradigm
 * 
 * The time window (time_start, time_end) defines what data is loaded into
 * GPU buffers—not just what portion of pre-loaded data is visible.
 * No bounds enforcement; areas outside the data range render as blank space.
 */
struct TimeSeriesViewState {
    // Time window - defines buffer scope
    int64_t time_start{0};
    int64_t time_end{1000};
    
    // Y-axis viewport bounds
    float y_min{-1.0f};
    float y_max{1.0f};
    float vertical_pan_offset{0.0f};
    
    // Global scale factors
    float global_zoom{1.0f};
    float global_vertical_scale{1.0f};
    
    /**
     * @brief Get visible time window width
     */
    [[nodiscard]] int64_t getTimeWidth() const { return time_end - time_start + 1; }
    
    /**
     * @brief Get center of visible time window
     */
    [[nodiscard]] int64_t getTimeCenter() const;
    
    /**
     * @brief Set time window centered on a point with specified width
     * No bounds clamping—allows viewing any time range.
     */
    void setTimeWindow(int64_t center, int64_t width);
    
    /**
     * @brief Set time window with explicit start and end
     */
    void setTimeRange(int64_t start, int64_t end);
};
```

**Usage Example:**

```cpp
// In DataViewer initialization
void DataViewer_Widget::setMasterTimeFrame(std::shared_ptr<TimeFrame> tf) {
    _master_time_frame = tf;
    
    // Set initial visible range (first 10,000 samples or full range if smaller)
    int64_t total = tf->getTotalFrameCount();
    int64_t initial_range = std::min(int64_t(10000), total);
    _view_state.setTimeWindow(initial_range / 2, initial_range);
}

// Zooming changes what data is loaded (triggers buffer rebuild)
void DataViewer_Widget::handleZoom(int delta) {
    int64_t center = _view_state.getTimeCenter();
    int64_t current_width = _view_state.getTimeWidth();
    int64_t new_width = (delta > 0) ? current_width / 2 : current_width * 2;
    
    // No clamping—can zoom to show blank areas beyond data bounds
    _view_state.setTimeWindow(center, new_width);
    
    // This changes buffer scope, triggering data reload
    rebuildBuffers();
    updatePlot();
}

// X scrolling is typically controlled externally (scrollbar yoked to other widgets)
// Not handled via TimeSeriesViewState methods in this widget
```

This design enables:
1. Real-time/streaming data visualization (load only what's needed)
2. Blank areas shown when viewing times without data (no bounds enforcement)
3. External synchronization of time position via scrollbar

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

struct YAxisParams {
    float world_y_min;       // Bottom of world space (default -1.0)
    float world_y_max;       // Top of world space (default 1.0)
    float pan_offset;        // Vertical pan offset
    int viewport_height_px;  // Canvas height in pixels
};

// Forward: Time → Screen pixel X
float timeToCanvasX(float time, TimeAxisParams const& params);

// Inverse: Screen pixel X → Time
float canvasXToTime(float canvas_x, TimeAxisParams const& params);

// Forward: World Y → Canvas pixel Y
float worldYToCanvasY(float world_y, YAxisParams const& params);

// Inverse: Canvas pixel Y → World Y (accounts for Y-flip: canvas 0=top, world y_max=top)
float canvasYToWorldY(float canvas_y, YAxisParams const& params);
```

#### Series Y-Axis Queries (Through the Hierarchy)

The Y-axis is more complex because each series has its own coordinate space. The implementation
uses a three-step approach:

```
canvas_y 
  → canvasYToWorldY(y_params)                 // Canvas → World (accounts for Y-flip)
  → worldYToAnalogValue(model_params)         // Inverts Model matrix scaling/offset
  → data_value                                // Actual value in series units
```

**Actual Implementation (December 2025):**

```cpp
// Step 1: Canvas Y → World Y (using YAxisParams)
// Handles the Y-flip: canvas 0=top, world y_max=top
CorePlotting::YAxisParams y_params(world_y_min, world_y_max, viewport_height, pan_offset);
float world_y = CorePlotting::canvasYToWorldY(canvas_y, y_params);

// Step 2: Build AnalogSeriesMatrixParams from display options
// Same params used to create the Model matrix for rendering
CorePlotting::AnalogSeriesMatrixParams model_params{
    .allocated_y_center = layout_result.allocated_y_center,
    .allocated_height = layout_result.allocated_height,
    .intrinsic_scale = style.intrinsic_scale,
    .user_scale_factor = style.user_scale_factor,
    .global_zoom = style.global_zoom,
    .user_vertical_offset = style.user_vertical_offset,
    .data_mean = data_cache.mean,
    .std_dev = data_cache.std_dev,
    .global_vertical_scale = global_vertical_scale
};

// Step 3: Invert the Model matrix transform
// Forward: y_world = (y_data - mean) * scale + y_center + offset
// Inverse: y_data = (y_world - y_center - offset) / scale + mean
float data_value = CorePlotting::worldYToAnalogValue(world_y, model_params);
```

**Key insight**: The inverse transform is computed analytically (not via matrix inversion).
The `AnalogSeriesMatrixParams` struct is reused from the rendering path, ensuring the forward
and inverse transforms are always consistent.

For series identification (finding which series the cursor is over), see `SeriesCoordinateQuery.hpp`:

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
```

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

See the "Directory Structure" subsection in "Architectural Evolution: Position Mappers" above for the complete, up-to-date directory listing.

## Core Concepts

### 1. Coordinate Spaces & Interaction Strategy

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

### 2. EntityID as Complete Reference

When the user hovers over or selects an event, the QuadTree returns an `EntityId`. This is sufficient to:

1. **Frame jump**: Look up `TimeFrameIndex` via `DigitalEventSeries::getEventsWithIdsInRange()` or EntityRegistry
2. **Group coloring**: Query GroupManager for color
3. **Selection tracking**: Store EntityId in selection set

No reverse transformation is needed — the EntityId directly references the original event.

### 3. Spatial Indexing via SceneBuilder

The `SceneBuilder` automatically builds spatial indexes when discrete elements (glyphs, rectangles) are added:

```cpp
// Build scene with automatic spatial indexing
RenderableScene scene = SceneBuilder()
    .setBounds(bounds)
    .addGlyphs("events", TimeSeriesMapper::mapEvents(events, layout, tf))
    .addRectangles("intervals", TimeSeriesMapper::mapIntervals(intervals, layout, tf))
    .build();

// The scene.spatial_index is automatically populated
auto const* hit = scene.spatial_index->findNearest(world_x, world_y, tolerance);
if (hit) {
    EntityId entity_id = hit->data;
    // Use entity_id for frame jump, selection, etc.
}
```

**Key insight**: The QuadTree and the render buffers are built from the **same position data** (output of Mappers), ensuring they are always synchronized.

## Summary

### Component Overview (Updated with Position Mappers)

| Component | Location | Purpose |
|-----------|----------|---------|
| **Mappers** | `Mappers/` | **Visualization-specific**: Convert data + layout → positions[] |
| `TimeSeriesMapper` | `Mappers/` | DataViewer: events/intervals/analog → time-based positions |
| `SpatialMapper` | `Mappers/` | SpatialOverlay: points/lines → direct X/Y positions |
| `RasterMapper` | `Mappers/` | Raster/PSTH: events → relative-time positions |
| **SceneBuilder** | `SceneGraph/` | **Generic**: positions[] + entity_ids[] → RenderableScene |
| `TimeRange` | `CoordinateTransform/` | TimeFrame-aware X-axis bounds (replaces legacy XAxis) |
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

### Completed Migrations

| Legacy Component | New (CorePlotting) | Status |
|------------------|---------------------|--------|
| `XAxis` | `TimeRange` (TimeFrame-aware) | ✅ Complete |
| `PlottingManager` series storage | Removed (single source in widget) | ✅ Complete |
| `PlottingManager` allocation methods | `LayoutEngine` → `SeriesLayout` | ✅ Complete |
| `NewAnalogTimeSeriesDisplayOptions` | Split into `SeriesStyle` + `SeriesLayout` | ✅ Complete |
| `MVP_*.cpp` in `DataViewer/` | `SeriesMatrices.cpp` in `CorePlotting/` | ✅ Complete |
| Inline vertex generation | `Mapper` → `SceneBuilder` → `RenderableBatch` | ✅ Complete |
| `SceneBuilder.addEventSeries(series, tf)` | `Mapper.mapEvents()` → `SceneBuilder.addGlyphs(positions)` | ✅ Complete |
| `_verticalPanOffset`, `_yMin`, `_yMax` | `TimeSeriesViewState` | ✅ Complete |

