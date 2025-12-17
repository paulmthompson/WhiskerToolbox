# CorePlotting Refactoring Roadmap

This document outlines the roadmap for consolidating the plotting architecture in WhiskerToolbox. The goal is to move from parallel, coupled implementations (DataViewer, Analysis Dashboard, SVGExporter) to a layered architecture centered around `CorePlotting`.

## Current State Analysis

### DataViewer Widget Issues
- **XAxis isolation**: Manages time range without TimeFrame integration
- **Duplicate storage**: Both PlottingManager and OpenGLWidget store series maps
- **DisplayOptions conflation**: Mixes style, layout output, and cached data
- **Per-frame vertex generation**: Rebuilds geometry every render

### What Works Well (to preserve)
- **SpatialOverlayOpenGLWidget**: Clean ViewState integration, single MVP
- **MVP_*.cpp functions**: Correct Model matrix logic per series type
- **PlottingManager allocation**: Good layout algorithms

---

## Phase 0: Immediate Cleanup (No Architecture Change)
**Goal:** Remove duplication and clarify responsibilities before refactoring.

- [x] **Eliminate duplicate series storage**:
    - Removed `PlottingManager::analog_series_map` (and event/interval maps)
    - Series storage remains in `OpenGLWidget` only
    - `PlottingManager` is now a pure layout calculator (no data storage)
    - Added `getAnalogSeriesAllocationForKey()` to support spike sorter configuration
    
- [x] **Split DisplayOptions structs**:
    - Created `CorePlotting::SeriesStyle` (color, alpha, thickness, visibility) — pure configuration
    - Created `CorePlotting::SeriesLayoutResult` (allocated_y_center, allocated_height) — layout output
    - Created `CorePlotting::SeriesDataCache` (cached std_dev, mean) — mutable cache
    - Updated all three DisplayOptions structs to use nested members
    - Updated all MVP functions, SVGExporter, and test code
    - All tests passing ✓
    
- [x] **Rename PlottingManager → LayoutCalculator**:
    - Renamed struct to `LayoutCalculator` to reflect single responsibility
    - Created compatibility header with type alias `using PlottingManager = LayoutCalculator;`
    - Updated all forward declarations across codebase
    - All tests passing ✓

---

## Phase 1: Core Abstractions (The "Brain")
**Goal:** Centralize math, layout, and querying logic. No Qt, No OpenGL.

### 1.1 TimeRange Integration
- [x] **Create `TimeRange` struct** in `CorePlotting/CoordinateTransform/`:
    - Port logic from `XAxis` but add TimeFrame awareness
    - Methods: `fromTimeFrame()`, `setCenterAndZoom()` with bounds enforcement
    - Integrate with `ViewState` for time-series plots
    - Location: [TimeRange.hpp](CoordinateTransform/TimeRange.hpp)
    - Tests: [TimeRange.test.cpp](/tests/CorePlotting/TimeRange.test.cpp)
    
- [x] **Create `TimeSeriesViewState`**:
    - Extends `ViewState` with `TimeRange` for X-axis
    - Standard `ViewState` zoom/pan applies to Y-axis only
    - X-axis controlled through `TimeRange` methods
    - Defined in: [TimeRange.hpp](CoordinateTransform/TimeRange.hpp)

- [x] **Create TimeFrame Range Adapters** ✅:
    - C++20 range adapters for TimeFrameIndex ↔ absolute time conversion
    - Location: [TimeFrameAdapters.hpp](CoordinateTransform/TimeFrameAdapters.hpp)
    - Tests: [TimeFrameAdapters.test.cpp](/tests/CorePlotting/TimeFrameAdapters.test.cpp) — 71 assertions passing
    - **Forward conversion** (`toAbsoluteTime`): TimeFrameIndex → absolute time (int)
        - Works with `std::pair<TimeFrameIndex, T>` (AnalogTimeSeries ranges)
        - Works with bare `TimeFrameIndex` (DigitalEventSeries ranges)
        - Works with `Interval` and `TimeFrameInterval`
        - Works with `EventWithId`-like types (has `.event_time`)
        - Works with `IntervalWithId`-like types (has `.interval`)
    - **Inverse conversion** (`toTimeFrameIndex`): absolute time → TimeFrameIndex
        - For mouse hover: screen X → time → TimeFrameIndex
    - **Cross-TimeFrame conversion** (`toTargetFrame`, `convertTimeFrameIndex`):
        - Convert indices between different TimeFrames (e.g., series → master)
    - **TimeFrameConverter context class**: Bidirectional conversion helper
    - Pipe operator support: `series.getAllSamples() | toAbsoluteTime(tf)`

### 1.2 MVP Matrix Consolidation ✅
- [x] **Move matrix logic to CorePlotting**:
    - `MVP_AnalogTimeSeries.cpp` → `CorePlotting/CoordinateTransform/SeriesMatrices.cpp` ✓
    - `MVP_DigitalEvent.cpp` → same location ✓
    - `MVP_DigitalInterval.cpp` → same location ✓
    - Location: [SeriesMatrices.hpp](CoordinateTransform/SeriesMatrices.hpp), [SeriesMatrices.cpp](CoordinateTransform/SeriesMatrices.cpp)
    - Tests: [SeriesMatrices.test.cpp](/tests/CorePlotting/SeriesMatrices.test.cpp) — 177 assertions passing
    - Fixed user_vertical_offset bug (was using glm::translate incorrectly)
    - ✓ Updated `OpenGLWidget` to use new CorePlotting functions

- [x] **Document MVP strategy**:
    - **Model**: Per-series (vertical positioning, scaling)
    - **View**: Shared (global pan)
    - **Projection**: Shared (time range → NDC)
    - Documented in SeriesMatrices.hpp header

- [x] **Phase 1.2 Cleanup — Remove legacy MVP files** ✅:
    - Created `AnalogSeriesHelpers.{hpp,cpp}` for migrated helper functions (setAnalogIntrinsicProperties, getCachedStdDev, invalidateDisplayCache)
    - Updated all consumers to use CorePlotting API:
        - `SVGExporter.cpp`: Converted to parameter struct-based API ✓
        - `OpenGLWidget.cpp`: Already using CorePlotting (previous session) ✓
        - `test_data_viewer.cpp`: Updated 50+ function calls to parameter structs ✓
    - Removed 9 legacy files: `MVP_AnalogTimeSeries.{cpp,hpp,test.cpp}`, `MVP_DigitalEvent.{cpp,hpp,test.cpp}`, `MVP_DigitalInterval.{cpp,hpp,test.cpp}` ✓
    - Updated CMakeLists.txt files to reference new locations ✓
    - Fixed test linking (added CorePlotting to test dependencies) ✓
    - All tests passing (177 CorePlotting unit tests + 3 DataViewer integration tests) ✓

### 1.3 Layout Engine
- [x] **Create `LayoutEngine`** (evolution of PlottingManager):
    - Pure functions that take series count/configuration → layout positions
    - No data storage, no global state
    - Input: `LayoutRequest` (series types, counts, viewport bounds)
    - Output: `std::vector<SeriesLayoutResult>`
    - Location: [LayoutEngine.hpp](Layout/LayoutEngine.hpp), [LayoutEngine.cpp](Layout/LayoutEngine.cpp)
    - Tests: [LayoutEngine.test.cpp](/tests/CorePlotting/LayoutEngine.test.cpp) — 82 assertions passing ✓
    
- [x] **Implement layout strategies**:
    - `StackedLayoutStrategy`: DataViewer-style vertical stacking ✓
        - Location: [StackedLayoutStrategy.hpp](Layout/StackedLayoutStrategy.hpp), [StackedLayoutStrategy.cpp](Layout/StackedLayoutStrategy.cpp)
        - Handles mixed stackable (analog, stacked events) and full-canvas (intervals) series
    - `RowLayoutStrategy`: Raster plot-style horizontal rows ✓
        - Location: [RowLayoutStrategy.hpp](Layout/RowLayoutStrategy.hpp), [RowLayoutStrategy.cpp](Layout/RowLayoutStrategy.cpp)
        - Equal row heights for multi-trial visualization

### 1.4 Renderable Primitives
- [x] **Finalize `RenderableScene` struct**:
    - Each batch contains its own Model matrix ✓
    - Scene contains shared View + Projection matrices ✓
    - QuadTree integrated for spatial queries ✓
    - Location: [RenderablePrimitives.hpp](SceneGraph/RenderablePrimitives.hpp)
    - Updated to match DESIGN.md architecture
    
- [x] **Add `SceneBuilder` helper class**:
    - Fluent interface for constructing scenes ✓
    - Automatic spatial index building from batches ✓
    - Location: [SceneBuilder.hpp](SceneGraph/SceneBuilder.hpp), [SceneBuilder.cpp](SceneGraph/SceneBuilder.cpp)

- [x] **Implement Transformers**:
    - `GapDetector`: `AnalogTimeSeries` → segmented `RenderablePolyLineBatch` ✓
        - Location: [GapDetector.hpp](Transformers/GapDetector.hpp), [GapDetector.cpp](Transformers/GapDetector.cpp)
        - Time and value-based gap detection
        - Configurable minimum segment length
    - ~~`RasterBuilder`~~: Superseded by `RasterMapper` in Mappers/ (Phase 3.7)

### 1.5 Spatial Indexing ✅
- [x] **Finalize `QuadTree<EntityId>` implementation** ✓
- [x] ~~**Create spatial adapters**~~: Superseded by Mappers + SceneBuilder (Phase 3.7)
    - Spatial indexing now handled by `SceneBuilder.addGlyphs()` / `addRectangles()` which build QuadTree automatically
    - See `TimeSeriesMapper`, `SpatialMapper`, `RasterMapper` for position generation

### 1.6 Integration Tests ✅
**Goal:** Validate end-to-end workflows before moving to OpenGL rendering layer.

*Note: Original integration tests in IntegrationTests.test.cpp were removed in favor of Phase3_5_IntegrationTests.test.cpp which uses the Mappers architecture.*

- [x] **Stacked Events Integration Tests (DataViewer style)**:
    - Multiple `DigitalEventSeries` in separate stacked rows
    - Absolute time positioning
    - Combined QuadTree for all series
    - Test: simulated click at various positions → verify correct series and EntityId

- [x] **End-to-End Scene Building**:
    - `LayoutEngine` → `Mapper` → `SceneBuilder` → QuadTree
    - Validate QuadTree positions match glyph positions in batch

- [x] **Coordinate Transform Round-Trip**:
    - Screen → World → QuadTree query → verify EntityId
    - Test with various zoom/pan states (ViewState)

- [x] **TimeRange Bounds Enforcement**:
    - Test TimeRange prevents scrolling/zooming beyond TimeFrame bounds
    - Integration with ViewState for complete camera state management

- [x] **GapDetector Integration**:
    - Analog series with gaps → segmented polylines
    - Query at various points within/between segments

- [x] **Mixed Series Scene**:
    - Multiple series types (Analog + Events) in one layout
    - Verify layout positions are correct and distinct
    - Combined spatial index from events at different Y positions

---

## Phase 2: Rendering Strategies (The "Painter")
**Goal:** Create a library for rendering the "Scene Description" using OpenGL.

- [x] **Create `PlottingOpenGL` library** (src/PlottingOpenGL):
    - `IBatchRenderer` base interface (`Renderers/IBatchRenderer.hpp`)
    - `GLContext` helpers for Qt OpenGL wrappers (`GLContext.hpp/cpp`)
    - `PolyLineRenderer`: Takes `RenderablePolyLineBatch`, issues draw calls ✓
    - `GlyphRenderer`: Takes `RenderableGlyphBatch`, uses instancing ✓
    - `RectangleRenderer`: Takes `RenderableRectangleBatch`, uses instancing ✓
    - `SceneRenderer`: Coordinates all batch renderers ✓
    - Uses Qt6's OpenGL wrappers (QOpenGLFunctions, QOpenGLExtraFunctions)
    - GLSL 330 shaders embedded in headers

- [x] **Refactor `OpenGLWidget` to use renderers** ✓:
    - Added `SceneRenderer` member to `OpenGLWidget`
    - Created `SceneBuildingHelpers.{hpp,cpp}` to convert series data to `RenderableBatch` objects
    - `renderWithSceneRenderer()` method builds and uploads batches for all series types
    - Toggle between legacy and new rendering via `setUseSceneRenderer(bool)`
    - Legacy inline rendering preserved for backwards compatibility
    - All existing tests passing ✓

- [x] **Integrate ShaderManager into PlottingOpenGL** ✓:
    - Moved ShaderManager from `src/WhiskerToolbox/ShaderManager/` to `src/PlottingOpenGL/ShaderManager/`
    - All renderers support dual loading: embedded shaders (default) or ShaderManager (via constructor path)
    - Uniform naming convention aligned with existing shaders (`u_mvp_matrix`, `u_color`)
    - Existing battle-tested shaders remain in `src/WhiskerToolbox/shaders/`
    - Hot-reload support available for development workflows

- [x] **Refactor `SVGExporter`** ✓:
    - Created `CorePlotting/Export/SVGPrimitives.{hpp,cpp}` with batch-to-SVG conversion functions
    - `renderPolyLineBatchToSVG()`, `renderGlyphBatchToSVG()`, `renderRectangleBatchToSVG()`
    - `renderSceneToSVG()` and `buildSVGDocument()` for complete scene export
    - Refactored `SVGExporter` to build `RenderableScene` using `SceneBuildingHelpers`
    - Same coordinate transforms as OpenGL rendering (validates correctness)
    - All existing tests passing ✓

---

## Phase 3: Coordinate Transform Utilities (Screen ↔ World ↔ Time) ✅
**Goal:** Create testable, Qt-independent coordinate conversion functions that replace inline calculations in OpenGLWidget.

### 3.1 Time-Axis Coordinate Transforms ✅
**Current Problem:** `OpenGLWidget::canvasXToTime()` does inline math with widget-specific state.

- [x] **Create `TimeAxisCoordinates` utility** in `CorePlotting/CoordinateTransform/`:
    - Location: [CoordinateTransform/TimeAxisCoordinates.hpp](CoordinateTransform/TimeAxisCoordinates.hpp)
    - Tests: [TimeAxisCoordinates.test.cpp](/tests/CorePlotting/TimeAxisCoordinates.test.cpp)
    - `TimeAxisParams` struct bundles time_start, time_end, viewport_width_px
    - `canvasXToTime()`: Canvas X pixel → Time coordinate (float for sub-frame precision)
    - `timeToCanvasX()`: Time coordinate → Canvas X pixel
    - `timeToNDC()`: Time coordinate → Normalized Device Coordinate [-1, 1]
    - `ndcToTime()`: NDC → Time coordinate
    - `pixelsPerTimeUnit()` and `timeUnitsPerPixel()`: Scale helpers

- [x] **Integration with `TimeRange`**:
    - Added `TimeRange::toAxisParams(int viewport_width)` convenience method
    - Ensures consistent parameter bundling

### 3.2 World-to-Series Coordinate Queries ✅
**Current Problem:** `canvasYToAnalogValue()` has 70 lines duplicating state from multiple sources.

- [x] **Create `SeriesCoordinateQuery` in `CorePlotting/CoordinateTransform/`**:
    - Location: [CoordinateTransform/SeriesCoordinateQuery.hpp](CoordinateTransform/SeriesCoordinateQuery.hpp)
    - Tests: [SeriesCoordinateQuery.test.cpp](/tests/CorePlotting/SeriesCoordinateQuery.test.cpp)
    - `SeriesQueryResult` struct with series_key, series_local_y, normalized_y, is_within_bounds, series_index
    - `findSeriesAtWorldY()`: Given world Y, find which series (if any) contains it
    - `findClosestSeriesAtWorldY()`: Always returns closest series (for gap handling)
    - `worldYToSeriesLocalY()`: Convert world Y to series-local coordinate
    - `getSeriesWorldBounds()`: Get allocated min/max Y for a series
    - `worldYToNormalizedSeriesY()`: Convert world Y to [-1, +1] normalized coordinate
    - `normalizedSeriesYToWorldY()`: Inverse of above
    - `screenToWorld()`: Screen coordinates → world coordinates via inverse MVP

### 3.3 Unified Hit Testing Architecture ✅
**Current Problem:** Multiple ad-hoc functions (`findIntervalAtTime`, `findSeriesAtPosition`, etc.) with inconsistent approaches.

- [x] **Create `HitTestResult` with flexible payload**:
    - Location: [Interaction/HitTestResult.hpp](Interaction/HitTestResult.hpp)
    - Tests: [HitTestResult.test.cpp](/tests/CorePlotting/HitTestResult.test.cpp)
    - `HitType` enum: None, DigitalEvent, IntervalBody, IntervalEdgeLeft, IntervalEdgeRight, AnalogSeries, Point, PolyLine, SeriesRegion
    - Factory methods: `eventHit()`, `intervalBodyHit()`, `intervalEdgeHit()`, `analogSeriesHit()`, `pointHit()`, `polyLineHit()`, `seriesRegionHit()`
    - Comparison operators for priority sorting

- [x] **Create `SceneHitTester` that queries RenderableScene**:
    - Location: [Interaction/SceneHitTester.hpp](Interaction/SceneHitTester.hpp)
    - Tests: [SceneHitTester.test.cpp](/tests/CorePlotting/SceneHitTester.test.cpp)
    - `HitTestConfig` struct with point_tolerance, edge_tolerance, prioritize_discrete flag
    - `hitTest()`: Perform hit test using all available strategies
    - `queryQuadTree()`: Query spatial index for discrete events
    - `queryIntervals()`: Check time-based containment for intervals
    - `findIntervalEdge()`: Specialized query for interval edges (drag handles)
    - `querySeriesRegion()`: Check if Y is within series' allocated region
    - `selectBestHit()`: Choose best result when multiple strategies match

### 3.4 Interval Dragging State Machine ✅
**Current Problem:** `startIntervalDrag()`, `updateIntervalDrag()`, `finishIntervalDrag()` spread across 400+ lines.

- [x] **Create `IntervalDragController`** in `CorePlotting/Interaction/`:
    - Location: [Interaction/IntervalDragController.hpp](Interaction/IntervalDragController.hpp)
    - Tests: [IntervalDragController.test.cpp](/tests/CorePlotting/IntervalDragController.test.cpp)
    - `IntervalDragState` struct with series_key, is_left_edge, original/current start/end times, constraints
    - `IntervalDragConfig` struct with min_interval_duration, snap_to_frame, enforce_bounds
    - `startDrag()`: Initialize drag from HitTestResult
    - `updateDrag()`: Update position given world X coordinate
    - `finishDrag()`: Commit changes and return final interval
    - `cancelDrag()`: Restore original values
    - Constraint enforcement: minimum duration, bounds checking, snap-to-frame

- [ ] **Create `IntervalCreationController`** for double-click-to-create workflow (Phase 4)

### 3.5 Integration Tests: Real Data Types, Plain Language Scenarios ✅
**Goal:** Verify the complete pipeline from data → layout → scene → hit test using real types.

These tests use **no Qt/OpenGL**—just CorePlotting + DataManager types. This gives us high confidence before touching any widget code.

**Implementation:** [Phase3_5_IntegrationTests.test.cpp](/tests/CorePlotting/Phase3_5_IntegrationTests.test.cpp)

- [x] **Scenario 1: Stacked Analog + Events (DataViewer Style)**
    - Verifies stacked layout produces distinct Y positions for multiple series
    - Tests hit detection on discrete events returns correct EntityId
    - Tests hit detection on analog regions returns series_key without EntityId
    - Tests hit detection outside bounds returns no hit

- [x] **Scenario 2: Interval Selection and Edge Detection**
    - Creates DigitalIntervalSeries with known intervals and EntityIds
    - Tests click inside interval body returns IntervalBody hit with correct bounds
    - Tests click near left/right edges returns IntervalEdgeLeft/Right for drag handles
    - Tests click outside intervals returns no hit

- [x] **Scenario 3: Raster Plot (Multi-Row Events)**
    - Uses RowLayoutStrategy for equal-height rows
    - Builds combined QuadTree from multiple event series at different Y positions
    - Tests Y position distinguishes events at similar times from different trials

- [x] **Scenario 4: Coordinate Transform Round-Trip**
    - Tests TimeAxisParams converts screen X ↔ time correctly
    - Tests ViewState screenToWorld transforms work with various zoom/pan states
    - Tests complete pipeline: screen → world → QuadTree query → EntityId
    - Tests panned view correctly offsets coordinate transforms

- [x] **Scenario 5: Mixed Series Priority (Event Beats Analog)**
    - Verifies discrete elements (events) take priority over region hits
    - Tests SceneHitTester prioritize_discrete configuration
    - Tests fallback to region hit when no discrete element nearby

- [x] **Scenario 6: IntervalDragController State Machine**
    - Tests basic drag workflow: start → update → finish
    - Tests minimum width constraint prevents interval collapse
    - Tests time bounds constraint prevents exceeding limits
    - Tests cancel restores original values
    - Tests edge swap behavior when allowed
    - Tests only interval edge hits can start drag

- [x] **TimeRange Bounds Integration**
    - Tests fromTimeFrame captures bounds correctly
    - Tests zoom/scroll operations respect TimeFrame bounds
    - Tests setCenterAndZoom maintains center when possible, shifts near edges

- [x] **Scenario 7-8: SceneBuilder High-Level API**
    - Tests SceneBuilder.addEventSeries() creates glyph batches with correct positions
    - Tests SceneBuilder.addIntervalSeries() creates rectangle batches with correct bounds
    - Tests automatic spatial index construction from discrete series
    - Tests batch key mapping for interval hit testing

**Test Characteristics:**
- Uses real `DigitalEventSeries`, `DigitalIntervalSeries`, `TimeFrame` types
- Uses real `LayoutEngine` with `StackedLayoutStrategy` and `RowLayoutStrategy`
- Uses real `SceneHitTester` and `IntervalDragController`
- Uses real QuadTree spatial indexing via `SceneBuilder`
- No mocks — all production code paths
- 157 assertions across 9 test cases

### 3.6 SceneBuilder Enhancement ✅
**Goal:** Elevate SceneBuilder from low-level batch assembly to high-level data series API.

**Problem:** Original tests bypassed SceneBuilder and manually built QuadTree spatial indices, creating an architectural gap where rendered geometry and spatial index could diverge.

**Solution:** Enhanced SceneBuilder with:
- `setBounds(BoundingBox)`: Required first step, defines QuadTree coverage
- `addEventSeries(key, series, layout, time_frame)`: Creates GlyphBatch AND registers for spatial index
- `addIntervalSeries(key, series, layout, time_frame)`: Creates RectangleBatch AND registers for spatial index
- `build()`: Automatically constructs spatial index from all pending discrete series
- `getRectangleBatchKeyMap()`: Returns batch index → series key mapping for hit testing

**Key Insight:** Bounds are now a first-class input (not an afterthought). When discrete elements are added, the spatial index is built automatically in `build()`, ensuring perfect synchronization between rendered geometry and hit testing.

**Implementation:**
- [SceneBuilder.hpp](SceneGraph/SceneBuilder.hpp) - High-level API with PendingEventSeries/PendingIntervalSeries structs
- [SceneBuilder.cpp](SceneGraph/SceneBuilder.cpp) - Implementation with automatic spatial index construction
- [Phase3_5_IntegrationTests.test.cpp](/tests/CorePlotting/Phase3_5_IntegrationTests.test.cpp) - Scenarios 7-8 demonstrate new API

---

## Phase 3.7: Architectural Evolution — Position Mappers (NEW)

**Problem Identified:** The current `SceneBuilder` API bakes DataViewer-specific coordinate logic into the "generic" scene building layer:

```cpp
// CURRENT (problematic): SceneBuilder knows about TimeFrame and computes positions
builder.addEventSeries("events", series, layout, time_frame);  // Computes X from TimeFrame
```

This doesn't generalize to:
- **SpatialOverlay**: X/Y come directly from PointData/LineData coordinates
- **Scatter plots**: X from series_a value, Y from series_b value  
- **Raster plots**: X from relative time (event - reference), not absolute time

**Solution:** Separate **coordinate mapping** (visualization-specific) from **scene building** (generic).

### 3.7.1 Create Range-Based Mapper Infrastructure ✅

**Design Principle:** Mappers return **ranges/views**, not vectors. This enables:
- Zero-copy: Data flows directly from source → GPU buffer + QuadTree
- Single traversal: x, y, and entity_id extracted together in one pass
- Data source controls iteration: Each data type knows how to efficiently access its storage

**Implementation:** Located in `CorePlotting/Mappers/`

- [x] **`MappedElement.hpp`** — Element types yielded by mapper ranges:
    - `MappedElement` (x, y, entity_id) with `position()` → `glm::vec2`
    - `MappedRectElement` (x0, y0, x1, y1, entity_id) with `bounds()` → `glm::vec4`
    - `MappedVertex` (x, y) for line vertices

- [x] **`MapperConcepts.hpp`** — C++20 concepts for type constraints:
    - `MappedElementLike`, `MappedRectLike`, `MappedVertexLike` — element type detection
    - `MappedLineViewLike` — multi-vertex line with entity_id
    - `MappedElementRange`, `MappedRectRange`, `MappedLineRange` — range constraints

- [x] **`MappedLineView.hpp`** — Line view types for polyline data:
    - `MappedLineView<R>` — generic wrapper over any vertex range
    - `SpanLineView`, `OwningLineView` — concrete view types
    - `TransformingLineView<T>` — lazy vertex transformation
    - Factory functions: `makeLineView()`, `makeTimeSeriesLineView()`

- [x] **`TimeSeriesMapper.hpp`** — DataViewer-style time-series mapping:
    - `mapEvents()`, `mapEventsInRange()` — DigitalEventSeries → MappedElement range
    - `mapIntervals()`, `mapIntervalsInRange()` — DigitalIntervalSeries → MappedRectElement range
    - `mapAnalogSeries()`, `mapAnalogSeriesFull()` — AnalogTimeSeries → MappedLineView
    - Both lazy range versions and materialized `*ToVector()` versions

- [x] **`SpatialMapper.hpp`** — SpatialOverlay-style spatial mapping:
    - `mapPointsAtTime()` — PointData → vector of MappedElement
    - `mapPoint()`, `mapPoints()` — individual/range point mapping
    - `mapLinesAtTime()`, `mapLine()`, `mapLineLazy()` — LineData → OwningLineView
    - All functions support coordinate transformation (scale/offset)

- [x] **`RasterMapper.hpp`** — PSTH/raster plot relative time mapping:
    - `mapEventsRelative()` — events relative to reference time
    - `mapEventsInWindow()` — events within time window
    - `mapTrials()` — event-aligned trial views
    - `computeRowYCenter()`, `makeRowLayout()` — row positioning utilities

**Tests:** [Mappers.test.cpp](/tests/CorePlotting/Mappers.test.cpp) — 19 test cases, 111 assertions ✓

### 3.7.2 Refactor SceneBuilder to Range-Based API ✅

- [x] **Add range-consuming methods to SceneBuilder**:
    - `addGlyphs<R>(key, elements, style)` — consumes `MappedElementRange`
    - `addRectangles<R>(key, elements, style)` — consumes `MappedRectRange`
    - `addPolyLine<R>(key, vertices, entity_id, style)` — single line from `MappedVertexRange`
    - `addPolyLines<R>(key, lines, style)` — multiple lines from `MappedLineRange`
    - Single traversal populates both GPU buffer and spatial index

- [x] **Remove data-type-specific methods**:
    - Removed `addEventSeries()`, `addIntervalSeries()` entirely
    - Removed `PendingEventSeries`, `PendingIntervalSeries` structs
    - Updated `buildSpatialIndexFromPending()` to use new `_pending_spatial_inserts` vector

- [x] **Add style structs for new API**:
    - `GlyphStyle` (glyph_type, size, color, model_matrix)
    - `RectangleStyle` (color, model_matrix)
    - `PolyLineStyle` (thickness, color, model_matrix)

- [x] **Update integration tests to use new API**:
    - Scenario 7 & 8 now use `TimeSeriesMapper::mapEvents()` + `addGlyphs()`
    - Scenario 8 uses `TimeSeriesMapper::mapIntervals()` + `addRectangles()`

### 3.7.3 Create Plot-Type-Specific Layout Strategies ✅

- [x] **Create `SpatialLayoutStrategy.hpp`**:
    - Location: [Layout/SpatialLayoutStrategy.hpp](Layout/SpatialLayoutStrategy.hpp)
    - Tests: [SpatialLayoutStrategy.test.cpp](/tests/CorePlotting/SpatialLayoutStrategy.test.cpp)
    - Three layout modes: `Fit` (uniform scale, preserves aspect ratio), `Fill` (non-uniform scale), `Identity`
    - `SpatialTransform` struct with `apply(glm::vec2)`, `applyX(float)`, `applyY(float)` methods
    - `SpatialSeriesLayout` combines standard `SeriesLayout` with `SpatialTransform`
    - `SpatialLayoutResponse` with effective data bounds (includes padding) and viewport bounds
    - Handles degenerate cases (zero-width/height bounds)
    - 14 unit tests covering all modes and edge cases

- [x] **Ensure existing strategies are complete**:
    - `StackedLayoutStrategy`: Verified for DataViewer ✓
    - `RowLayoutStrategy`: Verified for raster plots ✓

### 3.7.4 Update Integration Tests ✅

- [x] **Add Scenario 9: TimeSeriesMapper End-to-End**:
    - Location: [Phase3_5_IntegrationTests.test.cpp](/tests/CorePlotting/Phase3_5_IntegrationTests.test.cpp)
    - Create events + analog series with custom TimeFrame (index i → time i*10)
    - Map using `TimeSeriesMapper::mapEvents()` and `mapAnalogSeries()`
    - Build scene with position-based `SceneBuilder::addGlyphs()`
    - Verify positions match expected TimeFrame conversions
    - Verify hit testing works correctly with `findNearest()`

- [x] **Add Scenario 10: SpatialMapper End-to-End**:
    - Create PointData with known positions at specific time frames
    - Map using `SpatialMapper::mapPointsAtTime()`
    - Test scaling/offset transforms for NDC conversion
    - Build scene and verify QuadTree contains correct positions
    - Test hit testing with direct X/Y coordinates (no TimeFrame)
    - Verify EntityIds are preserved through mapping

- [x] **Add Scenario 11: RasterMapper with Relative Time**:
    - Create events with absolute times
    - Map using `RasterMapper::mapEventsRelative()` with reference time
    - Verify output positions are relative to reference
    - Test `mapEventsInWindow()` for window filtering
    - Test multi-trial mapping with `mapTrials()` using different reference times
    - Verify Y positions differ between trials

### 3.7.5 Migration Path for Existing Code

**SceneBuildingHelpers.cpp** currently uses the data-type-specific SceneBuilder API. Migration approach:

1. Replace `builder.addEventSeries(...)` with `builder.addGlyphs(key, TimeSeriesMapper::mapEvents(...), style)`
2. Replace `builder.addIntervalSeries(...)` with `builder.addRectangles(key, TimeSeriesMapper::mapIntervals(...), style)`
3. For SpatialOverlay, use `SpatialMapper::mapPointsAtTime()` or `mapLinesAtTime()`
4. Update integration tests to use new Mapper → SceneBuilder flow

**Benefits of range-based approach:**
- **Zero-copy**: No intermediate `vector<glm::vec2>` or `vector<EntityId>`
- **Single traversal**: x, y, entity_id extracted together, fed to both GPU buffer and QuadTree
- **Composable**: Mappers can use `std::views::transform`, `std::views::filter`, etc.
- **Gradual migration**: Old vector-based API still works via span overloads

---

## Phase 4: Widget Migration (DataViewer OpenGLWidget)
**Goal:** Systematically replace inline code with CorePlotting/PlottingOpenGL calls.

### 4.1 Prerequisites Analysis

**Current `OpenGLWidget` Entanglements:**
| Function | Lines | Responsibilities | Target Module |
|----------|-------|------------------|---------------|
| `drawDigitalEventSeries()` | ~160 | Visibility counting, MVP setup, TimeFrame conversion, GL calls, per-event buffer | SceneBuildingHelpers + SceneRenderer |
| `drawDigitalIntervalSeries()` | ~180 | MVP setup, TimeFrame conversion, interval highlight, GL calls | SceneBuildingHelpers + SceneRenderer |
| `drawAnalogSeries()` | ~180 | Layout config (spike sorter), MVP setup, gap mode dispatch, GL calls | SceneBuildingHelpers + SceneRenderer |
| `_drawAnalogSeriesWithGapDetection()` | ~60 | Segment-by-segment GL upload | GapDetector (already exists) |
| `_drawAnalogSeriesAsMarkers()` | ~25 | Point rendering | RenderableGlyphBatch |
| `canvasXToTime()` | ~10 | Canvas X → Time | TimeAxisCoordinates |
| `canvasYToAnalogValue()` | ~70 | Canvas Y → Data value | SeriesYCoordinates |
| `findIntervalAtTime()` | ~40 | Time → Interval | TimeSeriesHitTester |
| `findIntervalEdgeAtPosition()` | ~40 | Canvas pos → Edge | TimeSeriesHitTester |
| `startIntervalDrag()` | ~25 | State init | IntervalDragController |
| `updateIntervalDrag()` | ~110 | Constraint logic | IntervalDragController |
| `finishIntervalDrag()` | ~70 | Commit changes | IntervalDragController |
| `drawDraggedInterval()` | ~90 | Visual feedback | SceneRenderer (overlay batch) |

### 4.2 Migration Step 1: Replace Coordinate Functions (Low Risk) ✅
**Goal:** Replace `canvasXToTime()` with CorePlotting; refactor `canvasYToAnalogValue()` to query through hierarchy.

- [x] **Add `TimeAxisCoordinates` to CorePlotting** (Phase 3.1) ✓
    - Added `YAxisParams` struct for Y-axis coordinate conversions
    - Added `canvasYToWorldY()` and `worldYToCanvasY()` functions
    - Added `worldYToNDC()` and `ndcToWorldY()` functions
    - Location: [TimeAxisCoordinates.hpp](CoordinateTransform/TimeAxisCoordinates.hpp)
    
- [x] **Add inverse transform utilities to SeriesMatrices** ✓
    - Added `worldYToAnalogValue()` — inverts Model matrix transform
    - Added `analogValueToWorldY()` — forward transform for completeness
    - Location: [SeriesMatrices.hpp](CoordinateTransform/SeriesMatrices.hpp)
    
- [x] **Update `OpenGLWidget::canvasXToTime()`** ✓:
    ```cpp
    float OpenGLWidget::canvasXToTime(float canvas_x) const {
        CorePlotting::TimeAxisParams params(_xAxis.getStart(), _xAxis.getEnd(), width());
        return CorePlotting::canvasXToTime(canvas_x, params);
    }
    ```
    
- [x] **Refactor `canvasYToAnalogValue()` to use CorePlotting** ✓:
    - Removed dead "legacy mode" code path (~30 lines, `_series_y_position` was never populated)
    - Simplified to clean 3-step approach using CorePlotting utilities:
    ```cpp
    float OpenGLWidget::canvasYToAnalogValue(float canvas_y, std::string const& series_key) const {
        // Step 1: Canvas Y → World Y (using CorePlotting::YAxisParams)
        CorePlotting::YAxisParams y_params(_yMin, _yMax, height(), _verticalPanOffset);
        float const world_y = CorePlotting::canvasYToWorldY(canvas_y, y_params);
        
        // Step 2: Build AnalogSeriesMatrixParams from display options
        CorePlotting::AnalogSeriesMatrixParams model_params = /* ... from series */;
        
        // Step 3: Use CorePlotting inverse transform
        return CorePlotting::worldYToAnalogValue(world_y, model_params);
    }
    ```
    
- [x] **Remove dead code** ✓:
    - Removed unused `_series_y_position` map from `OpenGLWidget.hpp`
    - Removed legacy mode branch (was never executed)
    
- [x] **Add comprehensive unit tests** ✓:
    - Y-axis coordinate conversion tests in [TimeAxisCoordinates.test.cpp](/tests/CorePlotting/TimeAxisCoordinates.test.cpp)
    - Inverse transform tests in [SeriesMatrices.test.cpp](/tests/CorePlotting/SeriesMatrices.test.cpp)
    - ~260 lines of new test coverage
    
- [x] **Verify:** All existing tests pass, hover behavior unchanged ✓

### 4.3 Migration Step 2: Replace Hit Testing (Medium Risk) ✅
**Goal:** Replace manual iteration with `SceneHitTester` queries through SceneGraph.

- [x] **Add `SceneHitTester` to CorePlotting** (Phase 3.3) ✓
    - `SceneHitTester` already exists in `CorePlotting/Interaction/SceneHitTester.hpp`
    - Added `_hit_tester` member to `OpenGLWidget`
    
- [x] **Cache `LayoutResponse` in OpenGLWidget** (rebuilt on series add/remove) ✓
    - Added `_cached_layout_response` member
    - Added `_layout_response_dirty` flag for lazy rebuild
    - Added `rebuildLayoutResponse()` method that builds layout from DisplayOptions
    - Series add/remove/clear functions now set `_layout_response_dirty = true`
    
- [x] **Replace `findSeriesAtPosition()`** with `SceneHitTester::querySeriesRegion()` ✓
    - Uses `_hit_tester.querySeriesRegion()` with cached layout
    - Determines series type from key lookup in series maps
    - Lazy evaluation pattern for layout rebuild
    
- [x] **Replace `findIntervalEdgeAtPosition()`** with CorePlotting coordinate utilities ✓
    - Uses `CorePlotting::TimeAxisParams` and `CorePlotting::timeToCanvasX()`
    - Uses `CorePlotting::canvasXToTime()` for time conversion
    - Calculates time tolerance from pixel tolerance
    - Still operates on `_selected_intervals` (widget-specific state)
    
- [x] **Review `findIntervalAtTime()`** — kept as data query (not scene hit test) ✓
    - This function queries underlying data series directly by key
    - Different purpose from scene-based hit testing
    - No changes needed — already clean and focused
    
- [x] **Verify:** All tests pass ✓

### 4.4 Migration Step 3: Replace Interval Dragging (Medium Risk) ✅
**Goal:** Extract state machine from widget into testable controller.

- [x] **Add `IntervalDragController` to CorePlotting** (Phase 3.4) ✓
    - Already implemented in `CorePlotting/Interaction/IntervalDragController.hpp`
    - Comprehensive tests in `tests/CorePlotting/IntervalDragController.test.cpp`
    
- [x] **Add `_interval_drag_controller` member to OpenGLWidget** ✓
    - Replaced manual state variables (`_is_dragging_interval`, `_dragging_series_key`, etc.)
    - Controller manages all drag state internally
    
- [x] **Refactor `startIntervalDrag()`** ✓:
    - Creates `HitTestResult::intervalEdgeHit()` from selected interval
    - Configures `IntervalDragConfig` with time frame bounds
    - Calls `_interval_drag_controller.startDrag(hit_result)`
    
- [x] **Refactor `updateIntervalDrag()`** ✓:
    - Uses controller state for series key and edge info
    - Performs collision detection in series time frame (widget-specific)
    - Calls `_interval_drag_controller.updateDrag(world_x)` with constrained position
    
- [x] **Refactor `finishIntervalDrag()`** ✓:
    - Gets final state via `_interval_drag_controller.finishDrag()`
    - Only applies changes if `final_state.hasChanged()`
    - Uses `final_state.current_start/end` for new interval bounds
    
- [x] **Refactor `cancelIntervalDrag()`** ✓:
    - Calls `_interval_drag_controller.cancelDrag()`
    - Resets cursor to arrow
    
- [x] **Update `drawDraggedInterval()`** ✓:
    - Uses `_interval_drag_controller.getState()` for all rendering coordinates
    
- [x] **Update mouse event handlers** ✓:
    - `mouseMoveEvent()`: Uses `_interval_drag_controller.isActive()`
    - `mouseReleaseEvent()`: Uses `_interval_drag_controller.isActive()`
    - `isDraggingInterval()`: Delegates to `_interval_drag_controller.isActive()`
    
- [x] **Verify:** All existing tests pass ✓

**Benefits achieved:**
- Drag state machine is now testable independently of OpenGLWidget
- Constraint enforcement (min_width, time bounds) handled by controller
- Clear separation: Widget handles TimeFrame conversion and collision detection, 
  Controller handles state transitions and basic constraints
- ~50 lines of state variables reduced to single controller member

### 4.5 Migration Step 4: Unify Rendering Path (Higher Risk) ✅
**Goal:** Remove legacy draw functions, use only SceneRenderer path.

- [x] **Ensure `SceneBuildingHelpers` handles all cases**:
    - [x] `buildAnalogSeriesBatch()` — basic polyline ✓
    - [x] Gap detection mode (handled by existing GapDetector integration) ✓
    - [x] Marker mode — added `buildAnalogSeriesAsMarkersBatch()` using GlyphBatch ✓
    - [x] `buildEventSeriesBatch()` — basic events ✓
    - [x] Stacked vs FullCanvas modes (handled by MVP matrix params) ✓
    - [x] `buildIntervalSeriesBatch()` — basic intervals ✓
    - [x] Selection highlight overlays — added `buildIntervalHighlightPolylineBatch()` ✓

- [x] **Update `paintGL()` to only use scene path**:
    - `renderWithSceneRenderer()` is now always called
    - Legacy draw functions no longer used
    - Overlay elements (drag preview, new interval) still use direct GL for interactive feedback

- [x] **Remove legacy functions**:
    - `drawDigitalEventSeries()` → deleted ✓
    - `drawDigitalIntervalSeries()` → deleted ✓
    - `drawAnalogSeries()` → deleted ✓
    - `_drawAnalogSeriesWithGapDetection()` → deleted ✓
    - `_drawAnalogSeriesAsMarkers()` → deleted ✓

- [x] **Verify:** All tests pass ✓

### 4.6 Migration Step 5: Replace XAxis with TimeSeriesViewState ✅
**Goal:** Use unified camera state instead of separate `_xAxis` + `_yMin/_yMax/_verticalPanOffset`.

- [x] **Add `_view_state` member** (type: `TimeSeriesViewState`) ✓
    - Replaced `XAxis _xAxis` with `CorePlotting::TimeSeriesViewState _view_state`
    - Added include for `CorePlotting/CoordinateTransform/TimeRange.hpp`
    
- [x] **Replace `_xAxis` calls** ✓:
    - `_xAxis.getStart()` → `_view_state.time_range.start`
    - `_xAxis.getEnd()` → `_view_state.time_range.end`
    - `_xAxis.setCenterAndZoom()` → `_view_state.time_range.setCenterAndZoom()`
    - Updated all 25 usages in OpenGLWidget.cpp
    
- [x] **Update public API** ✓:
    - Removed `getXAxis()` accessor (returned legacy XAxis reference)
    - Added `getTimeRange()` returning `CorePlotting::TimeRange const&`
    - Added `getVisibleStart()` and `getVisibleEnd()` convenience methods
    - `setXLimit()` now updates `_view_state.time_range.max_bound`
    - `changeRangeWidth()` and `setRangeWidth()` now use TimeRange methods
    
- [x] **Update `setMasterTimeFrame()`** ✓:
    - Initializes TimeRange from TimeFrame bounds
    - Sets reasonable initial visible range (10,000 samples max, not entire dataset)
    - Prevents performance issues with million-sample datasets
    
- [x] **Update dependent code** ✓:
    - SVGExporter: Uses `getTimeRange()` instead of `getXAxis()`
    - DataViewer_Widget: Updated `_updateLabels()` and wheel zoom handler
    - DataViewer_Widget.test.cpp: Updated all test cases to use new API
    
- [x] **Replace Y-axis state** ✓ (completed in Phase 4.7):
    - `_yMin, _yMax` → `_view_state.y_min, _view_state.y_max`
    - `_verticalPanOffset` → `_view_state.vertical_pan_offset`
    - `_global_zoom` → `_view_state.global_zoom`
    
- [x] **Verify:** All tests pass ✓

### 4.7 Final Cleanup ✅
- [x] **Migrate Y-axis state to ViewState** ✓:
    - Extended `TimeSeriesViewState` with y_min, y_max, vertical_pan_offset, global_zoom, global_vertical_scale
    - Removed legacy `_yMin`, `_yMax`, `_verticalPanOffset`, `_global_zoom` members from OpenGLWidget
    - All rendering code now uses `_view_state.*` for Y-axis parameters
    - Added `applyVerticalPanDelta()`, `resetVerticalPan()`, `getEffectiveYBounds()` helpers
    
- [x] **Remove `m_vertices` class member** ✓:
    - Removed unused `std::vector<GLfloat> m_vertices` from OpenGLWidget
    - Batches now own their own vertex data
    
- [x] **Remove per-series `_series_y_position` map** ✓:
    - Already removed in earlier phase (no changes needed)
    
- [x] **Add public accessors for ViewState parameters** ✓:
    - Added `getGlobalZoom()`, `getGlobalVerticalScale()`, `getVerticalPanOffset()`
    - Added `setGlobalVerticalScale()` for DataViewer_Widget control
    
- [x] **Document new architecture** ✓:
    - Added comprehensive header documentation to OpenGLWidget.hpp
    - Documents layer architecture: Data → ViewState → Layout → Rendering → Interaction
    - Documents Phase 4.7 migration notes
    
- [x] **Partial removal of _plotting_manager dependency** ✓:
    - Global zoom/scale/pan parameters now read from `_view_state`, not `_plotting_manager`
    - LayoutCalculator still used for layout calculations (stacking, spike sorter config)
    - `_plotting_manager->setPanOffset()` calls kept for LayoutCalculator sync
    
- [x] **Verify:** All tests pass ✓

### 4.8 Adopt TimeSeriesMapper in SceneBuildingHelpers ✅
**Goal:** Replace manual coordinate mapping in SceneBuildingHelpers with TimeSeriesMapper ranges.

**Summary:** Refactored `SceneBuildingHelpers.cpp` to use range-based `TimeSeriesMapper` API with "local-space layout" pattern. All batch building functions now use mappers (`mapEventsInRange()`, `mapIntervalsInRange()`, `mapAnalogSeriesWithIndices()`) instead of reimplementing coordinate logic.

**Key Changes:**
- `buildEventSeriesBatch()`, `buildIntervalSeriesBatch()`, `buildAnalogSeriesBatch()`, `buildAnalogSeriesMarkerBatch()` all use TimeSeriesMapper
- Enhanced TimeSeriesMapper with `MappedAnalogVertex` (includes `time_index` for gap detection)
- All mappers return ranges; materialization at call site
- All tests pass ✓

### 4.9 Unify Layout Systems ✅
**Goal:** Replace dual layout systems (LayoutCalculator + manual LayoutResponse rebuild) with single LayoutEngine.

**Summary:** Replaced `LayoutCalculator* _plotting_manager` with `CorePlotting::LayoutEngine _layout_engine`. Layout computation is now centralized—`buildLayoutRequest()` and `computeAndApplyLayout()` build from visible series and update DisplayOptions->layout fields on demand.

**Key Changes:**
- OpenGLWidget owns `LayoutEngine` with `StackedLayoutStrategy`
- Spike sorter configuration moved from LayoutCalculator to OpenGLWidget
- Removed `_plotting_manager` from DataViewer_Widget
- SVGExporter gets view params from OpenGLWidget directly
- All tests pass ✓

### 4.10 Adopt SceneBuilder Fluent API ✅
**Goal:** Replace direct renderer upload with SceneBuilder-based scene construction.

**Solution:** Refactored `renderWithSceneRenderer()` to use `SceneBuilder` fluent API:

- [x] **Refactor `renderWithSceneRenderer()` to use SceneBuilder** ✓
    - Creates `SceneBuilder`, sets bounds from visible time range
    - Sets view/projection matrices via `builder.setMatrices()`
    
- [x] **Replace upload methods with builder pattern** ✓
    - Renamed `uploadAnalogBatches()` → `addAnalogBatchesToBuilder(SceneBuilder&)`
    - Renamed `uploadEventBatches()` → `addEventBatchesToBuilder(SceneBuilder&)`
    - Renamed `uploadIntervalBatches()` → `addIntervalBatchesToBuilder(SceneBuilder&)`
    - Methods now call `builder.addPolyLineBatch()`, `builder.addGlyphBatch()`, `builder.addRectangleBatch()`
    
- [x] **Build scene and upload to SceneRenderer** ✓
    - Calls `builder.build()` to finalize scene
    - Uses `_scene_renderer->uploadScene(scene)` then `render()`
    
- [x] **Remove manual batch key map tracking** ✓
    - Uses `builder.getRectangleBatchKeyMap()` for hit testing
    - No longer manually populates `_rectangle_batch_key_map`
    
- [x] **Verify:** All tests pass ✓

**Files Modified:**
- `OpenGLWidget.cpp`: Refactored render path to use SceneBuilder
- `OpenGLWidget.hpp`: Updated method signatures
- `CorePlotting/CMakeLists.txt`: Changed include directories from PRIVATE to PUBLIC

**Benefits:**
- Spatial index built automatically with geometry
- Guaranteed synchronization between rendering and hit testing
- Cleaner separation: OpenGLWidget describes scene, SceneBuilder constructs it

### 4.11 Complete SceneHitTester Integration ✅
**Goal:** Route all hit testing through SceneHitTester, removing ad-hoc implementations.

- [x] **Cache RenderableScene for hit testing** ✓
    - Added `_cached_scene` member to store last rendered scene
    - Added `_scene_dirty` flag to track when scene needs rebuild
    - Scene cached in `renderWithSceneRenderer()` after build
    - Also stores `_glyph_batch_key_map` for event hit testing

- [x] **Refactor `findIntervalEdgeAtPosition()` to use SceneHitTester** ✓
    - Now uses `SceneHitTester::findIntervalEdge()` with cached scene
    - Configures `HitTestConfig` with pixel-to-world tolerance conversion
    - Maintains legacy fallback for pre-first-paint case
    - Returns `HitTestResult` with `IntervalEdgeLeft`/`IntervalEdgeRight` type

- [x] **Enhance `findSeriesAtPosition()` with full hit testing** ✓
    - Now uses `SceneHitTester::hitTest()` when spatial index available
    - Queries QuadTree for discrete elements (events, points) first
    - Falls back to `querySeriesRegion()` for analog/continuous data
    - Returns series type and key for tooltip display

- [x] **Verify:** Build and test ✓

**Files Modified:**
- `OpenGLWidget.hpp`: Added `_cached_scene`, `_scene_dirty`, `_glyph_batch_key_map`
- `OpenGLWidget.cpp`: 
    - `renderWithSceneRenderer()`: Caches scene and batch key maps
    - `updateCanvas()`: Marks scene as dirty
    - `findIntervalEdgeAtPosition()`: Uses SceneHitTester::findIntervalEdge()
    - `findSeriesAtPosition()`: Uses SceneHitTester::hitTest() + querySeriesRegion()

**Benefits:**
- Single code path for hit testing across all query types
- QuadTree-accelerated discrete element lookup
- Consistent tolerance handling via HitTestConfig
- Clear separation: SceneHitTester owns hit logic, widget converts coordinates

**Note:** `findIntervalAtTime()` is intentionally kept as a data query (not scene-based)
because it needs to query intervals by exact time coordinate for selection, which is
different from spatial hit testing for mouse interaction.

---

### 4.12 Integrate GapDetector ✅
**Goal:** Replace inline gap detection with CorePlotting GapDetector.

**Solution:** Added range-based `GapDetector::segmentByGaps()` API and integrated into SceneBuildingHelpers.

- [x] **Add `segmentByGaps()` static method to GapDetector** ✓
    - New template method works with `MappedAnalogVertex` ranges
    - Added `detectGapByIndex()` static helper for index-based gap detection
    - Moved `MappedAnalogVertex` to `MappedElement.hpp` for shared access

- [x] **Use `GapDetector::segmentByGaps()` in buildAnalogSeriesBatch** ✓
    - Replaced inline gap detection logic
    - Now calls `GapDetector::segmentByGaps()` with time threshold config
    - Iterates returned segments to build polyline batch

- [x] **Consolidate gap threshold configuration** ✓
    - Maps `AnalogBatchParams::gap_threshold` to `GapDetector::Config::time_threshold`
    - Config struct enables future value-based gap detection

- [x] **Verify:** All tests pass ✓

**Files Modified:**
- `CorePlotting/Mappers/MappedElement.hpp`: Added `MappedAnalogVertex` struct
- `CorePlotting/Mappers/TimeSeriesMapper.hpp`: Removed duplicate `MappedAnalogVertex`
- `CorePlotting/Transformers/GapDetector.hpp`: Added `segmentByGaps()`, `detectGapByIndex()`
- `CorePlotting/Transformers/GapDetector.cpp`: Implemented new methods
- `WhiskerToolbox/DataViewer_Widget/SceneBuildingHelpers.cpp`: Uses `GapDetector::segmentByGaps()`

**Benefits:**
- Tested gap detection logic in CorePlotting library
- Single source of truth for gap algorithm
- Range-based API for efficient integration with TimeSeriesMapper

### 4.13 Clean Up Model Matrix Parameter Construction (Low Effort)
**Goal:** Reduce boilerplate when building MVP matrix parameters.

**Current Problem:** Matrix params built inline with 8-10 fields manually populated:

```cpp
// OpenGLWidget.cpp - repeated ~3 times for different series types
CorePlotting::AnalogSeriesMatrixParams model_params;
model_params.allocated_y_center = display_options->layout.allocated_y_center;
model_params.allocated_height = display_options->layout.allocated_height;
model_params.intrinsic_scale = display_options->scaling.intrinsic_scale;
model_params.user_scale_factor = display_options->user_scale_factor;
model_params.global_zoom = _view_state.global_zoom;
model_params.user_vertical_offset = display_options->scaling.user_vertical_offset;
model_params.data_mean = display_options->data_cache.cached_mean;
model_params.std_dev = display_options->data_cache.cached_std_dev;
model_params.global_vertical_scale = _view_state.global_vertical_scale;
```

- [ ] **Add factory methods to matrix param structs**:
    - `AnalogSeriesMatrixParams::fromDisplayOptions(options, view_state, layout)`
    - `EventSeriesMatrixParams::fromDisplayOptions(options, view_state, layout)`
    - `IntervalSeriesMatrixParams::fromDisplayOptions(options, view_state, layout)`
    
- [ ] **Update SceneBuildingHelpers to use factories**:
    - Replace inline construction with single factory call
    - Pass ViewState and layout as parameters
    
- [ ] **Consider builder pattern for complex params**:
    - For cases where not all fields come from single source
    
- [ ] **Verify:** All tests pass, visual output unchanged

**Benefits:**
- Reduced boilerplate
- Centralized param construction (easier to update)
- Clear dependencies (what data is needed to build matrices)

---

## Phase 4 Summary

| Phase | Task | Effort | Dependencies | Status |
|-------|------|--------|--------------|--------|
| 4.8 | Adopt TimeSeriesMapper in SceneBuildingHelpers | Medium | Phase 3.7 (Mappers) | ✅ Complete |
| 4.9 | Unify Layout Systems | Medium-High | None | ✅ Complete |
| 4.10 | Adopt SceneBuilder Fluent API | Medium | 4.8 (Mappers integration) | ✅ Complete |
| 4.11 | Complete SceneHitTester Integration | Medium | 4.9 (unified layout for region queries) | ✅ Complete |
| 4.12 | Integrate GapDetector | Low | 4.8 (can do together) | ✅ Complete |
| 4.13 | Clean Up Model Matrix Construction | Low | 4.9 (layout simplification) | Not Started |

**Completed:**
- ✅ 4.8 (TimeSeriesMapper) — Range-based mappers integrated into SceneBuildingHelpers
- ✅ 4.9 (LayoutEngine) — Unified layout system replaces LayoutCalculator
- ✅ 4.10 (SceneBuilder) — Fluent API for scene construction
- ✅ 4.11 (SceneHitTester) — Unified hit testing through cached scene
- ✅ 4.12 (GapDetector) — Range-based gap detection integrated into batch building

**Remaining:** 4.13 (Matrix param cleanup)

---

## Phase 5: Analysis Dashboard Updates
**Goal:** Apply same patterns to EventPlotWidget and other dashboard components.

### 5.1 EventPlotWidget (Raster Plot)
- [ ] Replace `vector<vector<float>>` data format with `DigitalEventSeries`
- [ ] Use `RowLayoutStrategy` from CorePlotting
- [ ] Add EntityId support for hover/click

### 5.3 SpatialOverlayWidget
- [ ] Already using ViewState ✓
- [ ] Add CorePlotting transformers for line/point data
- [ ] Unify tooltip infrastructure

### 5.4 Shared Infrastructure
- [ ] Create `PlotTooltipManager` that works with QuadTree results
- [ ] Standardize cursor change on hover (edge detection)

---

## Open Questions & Risks

### Performance
- **Risk**: Abstraction layer introduces overhead for real-time updates
- **Mitigation**: Keep `Renderable*` structs POD, cache geometry in VBOs, only rebuild on data change

### Legacy Compatibility  
- **Risk**: Breaking existing DataViewer features during transition
- **Mitigation**: Phase 0 cleanup maintains behavior, Phase 1+ uses adapter pattern

### TimeFrame Integration
- **Question**: Should `TimeRange` hold a weak reference to `TimeFrame`, or just copy bounds?
- **Recommendation**: Copy bounds at construction, provide `updateFromTimeFrame()` for refresh

### Coordinate System Consistency
- **Question**: How to ensure QuadTree coordinates match Model matrix transforms?
- **Solution**: Both built from same Mapper output — positions computed once, used for both rendering and indexing

### Data Type Coupling
- **Question**: Should CorePlotting know about DataManager types (DigitalEventSeries, AnalogTimeSeries, etc.)?
- **Decision**: Yes, this is acceptable. CorePlotting is designed for WhiskerToolbox's data model. The Mappers explicitly couple to DataManager types; SceneBuilder and below remain data-type-agnostic.

---

## Summary: What Changes with Position Mappers Architecture

### Components Created (Phase 3.7.1 ✅)
| Component | Location | Purpose |
|-----------|----------|---------|
| `MappedElement.hpp` | `Mappers/` | Element types: `MappedElement`, `MappedRectElement`, `MappedVertex` |
| `MappedLineView.hpp` | `Mappers/` | Line view types: `MappedLineView<R>`, `SpanLineView`, `OwningLineView` |
| `MapperConcepts.hpp` | `Mappers/` | C++20 concepts: `MappedElementRange`, `MappedRectRange`, `MappedLineRange` |
| `TimeSeriesMapper.hpp` | `Mappers/` | DataViewer: `mapEvents()`, `mapIntervals()`, `mapAnalogSeries()` |
| `SpatialMapper.hpp` | `Mappers/` | SpatialOverlay: `mapPointsAtTime()`, `mapLinesAtTime()` |
| `RasterMapper.hpp` | `Mappers/` | Raster: `mapEventsRelative()`, `mapTrials()` |

### Components Created (Phase 3.7.2 ✅)
| Component | Location | Purpose |
|-----------|----------|---------|
| `GlyphStyle` | `SceneBuilder.hpp` | Style struct for glyph batches (type, size, color, model_matrix) |
| `RectangleStyle` | `SceneBuilder.hpp` | Style struct for rectangle batches (color, model_matrix) |
| `PolyLineStyle` | `SceneBuilder.hpp` | Style struct for polyline batches (thickness, color, model_matrix) |

### Components Modified (Phase 3.7.2 ✅)
| Component | Change |
|-----------|--------|
| `SceneBuilder` | Added range-consuming `addGlyphs<R>()`, `addRectangles<R>()`, `addPolyLine<R>()`, `addPolyLines<R>()` |
| `SceneBuilder` | Removed `addEventSeries()`, `addIntervalSeries()` (breaking change) |
| `SceneBuilder` | Removed `PendingEventSeries`, `PendingIntervalSeries` structs |
| `SceneBuilder` | Added `_pending_spatial_inserts` for unified spatial index building |
| `SceneBuilder` | Added `getGlyphBatchKeyMap()` for symmetry with rectangle batch key map |
| `Phase3_5_IntegrationTests` | Updated Scenarios 7-8 to use `TimeSeriesMapper` + new `addGlyphs`/`addRectangles` API |

### Components to Create (Future)
| Component | Location | Purpose |
|-----------|----------|---------|
| `EventAlignedMapper` | `Mappers/` | Event-aligned analog traces: analog+events→MappedLineRange |
| `SpatialLayoutStrategy` | `Layout/` | Simple bounds-fitting layout |

### Components Unchanged
| Component | Reason |
|-----------|--------|
| `LayoutEngine`, strategies | Already outputs SeriesLayout (transforms), not positions |
| `RenderablePrimitives` | Already position-based |
| `SceneHitTester` | Queries QuadTree by position, data-type-agnostic (for sparse data) |
| `PlottingOpenGL` renderers | Consume batches, don't care about data origin |
| `SVGPrimitives` | Consume batches, don't care about data origin |

### Key Architectural Principles

**1. Mappers are visualization-centric, not data-centric:**

| Visualization | Mapper | Input Data | Output |
|---------------|--------|------------|--------|
| Spatial whiskers | `SpatialMapper::mapLines` | `LineData` | `MappedLineRange` |
| Time-series trace | `TimeSeriesMapper::mapAnalog` | `AnalogTimeSeries` | `MappedVertexRange` (single line) |
| Event-aligned traces | `EventAlignedMapper::mapTrials` | `AnalogTimeSeries + DigitalEventSeries` | `MappedLineRange` (many lines) |
| Raster plot | `RasterMapper::mapEvents` | `DigitalEventSeries` | `MappedElementRange` |

**2. Hit testing strategy is data-type-dependent:**

| Data Type | Hit Test Strategy | Reason |
|-----------|-------------------|--------|
| Events (discrete, sparse) | QuadTree | O(log n) lookup, low element count |
| Points (discrete, sparse) | QuadTree | O(log n) lookup, low element count |
| Intervals | Time range query | 1D containment test on time axis |
| Lines (dense, 100k+) | **Compute Shader** | GPU parallelism beats QuadTree for dense data |
| Event-aligned traces | **Compute Shader** | Many polylines, same as LineData |

**3. The Mapper is the boundary between "data semantics" and "geometry":**

```
DataManager Types ──▶ Mapper (viz-specific) ──▶ positions[] ──▶ SceneBuilder (generic) ──▶ RenderableScene
```

- **Left of Mapper**: Knows about TimeFrame, DigitalEventSeries, AnalogTimeSeries, PointData
- **Right of Mapper**: Only knows about positions, entity_ids, styles

This enables:
1. Adding new visualization types by writing a new Mapper (no SceneBuilder changes)
2. Adding new data types by extending relevant Mappers (no SceneBuilder changes)
3. Testing Mappers independently from rendering
4. Reusing SceneBuilder, renderers across all visualizations
5. Choosing hit testing strategy appropriate to data density

---

## Migration Checklist (Per Widget)

```
[ ] Widget uses CorePlotting LayoutEngine for positioning
[ ] Widget uses appropriate Mapper for its visualization type
[ ] Widget uses position-based SceneBuilder API
[ ] Widget uses CorePlotting ViewState (or TimeSeriesViewState)
[ ] Widget renders via RenderableScene (not inline vertex generation)
[ ] Widget uses appropriate hit testing strategy:
    [ ] QuadTree for sparse discrete data (events, points)
    [ ] Compute shader for dense line data (LineData, event-aligned traces)
    [ ] Time range query for intervals
[ ] SVG export works via same Mapper → SceneBuilder flow
[ ] Unit tests for Mapper output
[ ] Unit tests for layout calculations
[ ] Unit tests for coordinate transforms
```
