# CorePlotting Library Design Document

## Overview

CorePlotting is the foundational, Qt-independent C++ library for the WhiskerToolbox plotting stack. It provides unified abstractions for spatial visualization of time-series data, separating **data & layout** from **rendering** and **interaction**.

The ultimate goal is to de-complect the current parallel implementations (DataViewer, Analysis Dashboard, SVG Exporter) into a layered architecture:

1.  **CorePlotting** (This Library): Data structures, Layout engines, Coordinate transforms (MVP), Spatial Indexing. (No Qt, No OpenGL).
2.  **PlottingOpenGL** (Future): Rendering strategies that consume CorePlotting descriptions and issue OpenGL commands.
3.  **QtPlotting** (Future): Qt widgets, Input handling, Tooltips, bridging user events to CorePlotting queries.

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

## Dependencies

```
CorePlotting
    ├── DataManager (DigitalEventSeries, PointData, EntityId, TimeFrameIndex)
    ├── SpatialIndex (QuadTree)
    ├── Entity (EntityId, EntityRegistry)
    ├── TimeFrame (TimeFrameIndex)
    └── glm (matrix math)
```

**Note**: CorePlotting does NOT depend on Qt or OpenGL. It provides pure data structures and algorithms that Qt-based widgets consume.

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
};

/**
 * @brief A batch of glyphs (e.g., Events in a Raster Plot, Points)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableGlyphBatch {
    // Instance Data: {x, y} positions
    std::vector<glm::vec2> positions;
    
    // Per-Glyph Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;

    // Global Attributes
    int glyph_type; // Enum: Circle, Square, Tick, etc.
    float size{5.0f};
};

/**
 * @brief A batch of rectangles (e.g., DigitalIntervalSeries)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableRectangleBatch {
    // Instance Data: {x, y, width, height} per rectangle
    std::vector<glm::vec4> bounds;
    
    // Per-Rectangle Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;
};

struct RenderableScene {
    std::vector<RenderablePolyLineBatch> poly_line_batches;
    std::vector<RenderableRectangleBatch> rectangle_batches;
    std::vector<RenderableGlyphBatch> glyph_batches;
    
    // Global State
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
};

## Rendering Abstraction (PlottingOpenGL)

The `CorePlotting` layer produces `Renderable*Batch` structures. The `PlottingOpenGL` layer consumes them. This separation allows us to swap rendering strategies without changing the data generation logic.

We define abstract Renderers. Concrete implementations handle hardware specifics:

- **StandardPolyLineRenderer** (OpenGL 3.3/4.1): Uses `glMultiDrawArrays` or simple loops. Compatible with macOS. Ideal for `DataViewer` (fewer, longer lines).
- **ComputePolyLineRenderer** (OpenGL 4.3+): Uses SSBOs and Compute Shaders for advanced culling/selection. Ideal for `Analysis Dashboard` (100k+ short lines).

The *Application* chooses the renderer at runtime based on hardware capabilities and use case.  float size{5.0f};
};

/**
 * @brief A batch of rectangles (e.g., DigitalIntervalSeries)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableRectangleBatch {
    // Instance Data: {x, y, width, height} per rectangle
    std::vector<glm::vec4> bounds;
    
    // Per-Rectangle Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;
};

struct RenderableScene {
    std::vector<RenderablePolyLineBatch> poly_line_batches;
    std::vector<RenderableRectangleBatch> rectangle_batches;
    std::vector<RenderableGlyphBatch> glyph_batches;
    
    // Global State
    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;
};

## Rendering Abstraction (PlottingOpenGL)

The `CorePlotting` layer produces `Renderable*Batch` structures. The `PlottingOpenGL` layer consumes them. This separation allows us to swap rendering strategies without changing the data generation logic.

We define abstract Renderers. Concrete implementations handle hardware specifics:

- **StandardPolyLineRenderer** (OpenGL 3.3/4.1): Uses `glMultiDrawArrays` or simple loops. Compatible with macOS. Ideal for `DataViewer` (fewer, longer lines).
- **ComputePolyLineRenderer** (OpenGL 4.3+): Uses SSBOs and Compute Shaders for advanced culling/selection. Ideal for `Analysis Dashboard` (100k+ short lines).

The *Application* chooses the renderer at runtime based on hardware capabilities and use case.
```

### 2. Data Transformers (The "Pipeline")

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
├── SceneGraph/                    # The "What to Draw"
│   ├── RenderableScene.hpp
│   ├── RenderablePrimitives.hpp   # Batches (PolyLine, Rect, etc.)
│   └── SceneBuilder.hpp
│
├── Transformers/                  # The "How to Create It"
│   ├── GapDetector.hpp            # Analog -> PolyLineBatch
│   ├── EpochAligner.hpp           # Analog + Events -> PolyLineBatch
│   └── RasterBuilder.hpp          # Events -> GlyphBatch
│
├── EventRow/                      # Row-based event model
│   ├── EventRow.hpp               # Data description of an event row
│   ├── EventRowLayout.hpp         # Layout/positioning information
│   ├── PlacedEventRow.hpp         # Combined: data + layout
│   └── EventRowBuilder.hpp        # Factory functions for common patterns
│
├── SpatialAdapter/                # Bridges data types to QuadTree
│   ├── EventSpatialAdapter.hpp    # DigitalEventSeries → QuadTree
│   ├── PointSpatialAdapter.hpp    # PointData → QuadTree
│   └── ISpatiallyIndexed.hpp      # Common interface
│
├── CoordinateTransform/           # MVP matrix utilities
│   ├── WorldCoordinates.hpp       # World space types and helpers
│   ├── ScreenToWorld.hpp          # Inverse VP transform for queries
│   └── ToleranceCalculation.hpp   # Screen pixels → world tolerance
│
└── tests/                         # Catch2 unit tests
    ├── EventRow.test.cpp
    ├── EventSpatialAdapter.test.cpp
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

A single `QuadTree<EntityId>` spans all rows for efficient queries:

```cpp
std::unique_ptr<QuadTree<EntityId>> buildEventSpatialIndex(
    std::vector<PlacedEventRow> const& placed_rows,
    BoundingBox const& world_bounds)
{
    auto index = std::make_unique<QuadTree<EntityId>>(world_bounds);
    
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

## Future Extensions

### PointData Support

The same architecture applies to PointData visualization:

```cpp
struct PointDataSpatialAdapter {
    static std::unique_ptr<QuadTree<EntityId>> buildFromPointData(
        PointData const& points,
        BoundingBox const& world_bounds);
};
```

### Multi-Type Canvas

A canvas could contain both events and points with separate QuadTrees or a unified index:

```cpp
struct CanvasSpatialIndex {
    std::unique_ptr<QuadTree<EntityId>> event_index;
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

CorePlotting provides:

| Component | Purpose |
|-----------|---------|
| `EventRow` | Logical description: series + center time |
| `EventRowLayout` | Rendering position: y_center + row_height |
| `PlacedEventRow` | Combined: ready for indexing and rendering |
| `buildEventSpatialIndex()` | Creates QuadTree from placed rows |
| `getEventModelMatrix()` | Creates M matrix using same layout values |
| `ISpatiallyIndexed` | Common query interface for widgets |

The key invariant: **QuadTree Y coordinates = Model matrix Y translation**. This ensures spatial queries and rendering are always synchronized without complex coordinate bookkeeping.
