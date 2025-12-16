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
    - `RasterBuilder`: `DigitalEventSeries` + row layout → `RenderableGlyphBatch` ✓
        - Location: [RasterBuilder.hpp](Transformers/RasterBuilder.hpp), [RasterBuilder.cpp](Transformers/RasterBuilder.cpp)
        - Multi-trial raster plot visualization
        - Time window filtering per row

### 1.5 Spatial Indexing ✅
- [x] **Finalize `QuadTree<EntityId>` implementation** ✓
- [x] **Create spatial adapters** ✓:
    - `EventSpatialAdapter`: DigitalEventSeries → QuadTree (stacked/raster/explicit positions) ✓
    - `PointSpatialAdapter`: RenderableGlyphBatch → QuadTree ✓
    - `PolyLineSpatialAdapter`: RenderablePolyLineBatch → QuadTree (vertices/bounding boxes/sampled points) ✓
    - Location: [SpatialAdapter/](SpatialAdapter/)
    - ISpatiallyIndexed interface for common query patterns
    - All adapters use static factory methods returning `std::unique_ptr<QuadTree<EntityId>>`
    - Tests: All 259 assertions passing ✓

### 1.6 Integration Tests ✅
**Goal:** Validate end-to-end workflows before moving to OpenGL rendering layer.

- [x] **Raster Plot Integration Tests**:
    - Multiple `DigitalEventSeries` representing trials/rows
    - Events centered on reference times (negative/positive relative times)
    - Combined QuadTree containing all events across all rows
    - Test: simulated click at (x, y) → verify correct EntityId returned
    - Location: [IntegrationTests.test.cpp](/tests/CorePlotting/IntegrationTests.test.cpp)

- [x] **Stacked Events Integration Tests (DataViewer style)**:
    - Multiple `DigitalEventSeries` in separate stacked rows
    - Absolute time positioning (no centering)
    - Combined QuadTree for all series
    - Test: simulated click at various positions → verify correct series and EntityId

- [x] **End-to-End Scene Building**:
    - `LayoutEngine` → `RasterBuilder`/Transformer → QuadTree
    - Validate QuadTree positions match glyph positions in batch

- [x] **Coordinate Transform Round-Trip**:
    - Screen → World → QuadTree query → verify EntityId
    - Test with various zoom/pan states (ViewState)

- [x] **TimeRange Bounds Enforcement**:
    - Test TimeRange prevents scrolling/zooming beyond TimeFrame bounds
    - Integration with ViewState for complete camera state management

- [x] **GapDetector + PolyLineSpatialAdapter Integration**:
    - Analog series with gaps → segmented polylines
    - Spatial index built from segments
    - Query at various points within/between segments
    - Fixed bug: GapDetector was storing float indices instead of vertex indices

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

### 3.5 Integration Tests: Real Data Types, Plain Language Scenarios
**Goal:** Verify the complete pipeline from data → layout → scene → hit test using real types.

These tests use **no Qt/OpenGL**—just CorePlotting + DataManager types. This gives us high confidence before touching any widget code.

#### Test Scenario 1: Stacked Analog + Events (DataViewer Style)
```cpp
TEST_CASE("DataViewer-style stacked layout with hit testing") {
    // === ARRANGE: Create real data types ===
    auto analog1 = std::make_shared<AnalogTimeSeries>();
    analog1->setData({0.5f, 1.2f, -0.3f, 0.8f, 0.1f});  // 5 samples
    
    auto analog2 = std::make_shared<AnalogTimeSeries>();
    analog2->setData({-0.2f, 0.4f, 0.9f, -0.1f, 0.6f});
    
    auto events = std::make_shared<DigitalEventSeries>();
    events->addEvent(TimeFrameIndex(1), EntityId(100));
    events->addEvent(TimeFrameIndex(3), EntityId(101));
    
    // === ARRANGE: Build layout (3 stacked series) ===
    LayoutRequest request;
    request.series = {
        {"analog1", SeriesType::Analog, true},
        {"analog2", SeriesType::Analog, true},
        {"events",  SeriesType::DigitalEvent, true}  // Stacked mode
    };
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    
    LayoutEngine engine(std::make_unique<StackedLayoutStrategy>());
    auto layout = engine.compute(request);
    
    // Verify layout positions are distinct
    REQUIRE(layout.layouts[0].allocated_y_center != layout.layouts[1].allocated_y_center);
    REQUIRE(layout.layouts[1].allocated_y_center != layout.layouts[2].allocated_y_center);
    
    // === ARRANGE: Build scene with spatial index ===
    auto scene = SceneBuilder()
        .addEventBatch("events", *events, layout.findLayout("events"))
        .withTimeRange(0, 5)
        .buildWithSpatialIndex();
    
    // === ACT & ASSERT: Hit test at various world coordinates ===
    SceneHitTester tester;
    
    // Click on event at time=1 (should hit EntityId 100)
    float event_y = layout.findLayout("events")->allocated_y_center;
    auto hit1 = tester.hitTest(1.0f, event_y, scene, layout);
    REQUIRE(hit1.hit_type == HitTestResult::HitType::DigitalEvent);
    REQUIRE(hit1.entity_id == EntityId(100));
    REQUIRE(hit1.series_key == "events");
    
    // Click in analog1's region (no EntityId, just series_key)
    float analog1_y = layout.findLayout("analog1")->allocated_y_center;
    auto hit2 = tester.hitTest(2.0f, analog1_y, scene, layout);
    REQUIRE(hit2.hit_type == HitTestResult::HitType::AnalogSeries);
    REQUIRE(hit2.series_key == "analog1");
    REQUIRE_FALSE(hit2.entity_id.has_value());
    
    // Click between series (should return closest or None)
    float between_y = (analog1_y + analog2_y) / 2.0f;
    auto hit3 = tester.hitTest(2.0f, between_y, scene, layout, /*tolerance=*/0.0f);
    REQUIRE(hit3.hit_type == HitTestResult::HitType::None);
}
```

#### Test Scenario 2: Interval Selection and Edge Detection
```cpp
TEST_CASE("Interval hit testing with edge detection for drag handles") {
    // === ARRANGE: Create interval series ===
    auto intervals = std::make_shared<DigitalIntervalSeries>();
    intervals->addInterval(10, 30, EntityId(200));  // Interval [10, 30]
    intervals->addInterval(50, 80, EntityId(201));  // Interval [50, 80]
    
    // Layout: intervals use full canvas
    LayoutRequest request;
    request.series = {{"intervals", SeriesType::DigitalInterval, false}};
    
    LayoutEngine engine(std::make_unique<StackedLayoutStrategy>());
    auto layout = engine.compute(request);
    
    auto scene = SceneBuilder()
        .addIntervalBatch("intervals", *intervals, layout.findLayout("intervals"))
        .withTimeRange(0, 100)
        .buildWithSpatialIndex();
    
    SceneHitTester tester;
    
    // === Click inside interval body ===
    auto hit_body = tester.hitTest(20.0f, 0.0f, scene, layout);
    REQUIRE(hit_body.hit_type == HitTestResult::HitType::IntervalBody);
    REQUIRE(hit_body.entity_id == EntityId(200));
    
    // === Click near left edge (drag handle) ===
    // Mark this interval as selected for edge detection
    std::map<std::string, std::pair<int64_t, int64_t>> selected = {
        {"intervals", {10, 30}}
    };
    auto hit_edge = tester.findIntervalEdge(10.5f, scene, selected, /*tolerance=*/2.0f);
    REQUIRE(hit_edge.has_value());
    REQUIRE(hit_edge->hit_type == HitTestResult::HitType::IntervalEdgeLeft);
    
    // === Click outside any interval ===
    auto hit_miss = tester.hitTest(40.0f, 0.0f, scene, layout);
    REQUIRE(hit_miss.hit_type == HitTestResult::HitType::None);
}
```

#### Test Scenario 3: Raster Plot (Multi-Row Events)
```cpp
TEST_CASE("Raster plot with row-based event layout") {
    // === ARRANGE: Multiple trials as separate event series ===
    auto trial1 = std::make_shared<DigitalEventSeries>();
    trial1->addEvent(TimeFrameIndex(5), EntityId(1));
    trial1->addEvent(TimeFrameIndex(15), EntityId(2));
    
    auto trial2 = std::make_shared<DigitalEventSeries>();
    trial2->addEvent(TimeFrameIndex(8), EntityId(3));
    trial2->addEvent(TimeFrameIndex(12), EntityId(4));
    
    // Use RowLayoutStrategy for raster-style rows
    LayoutRequest request;
    request.series = {
        {"trial1", SeriesType::DigitalEvent, true},
        {"trial2", SeriesType::DigitalEvent, true}
    };
    
    LayoutEngine engine(std::make_unique<RowLayoutStrategy>());
    auto layout = engine.compute(request);
    
    // Build with RasterBuilder
    auto scene = SceneBuilder()
        .addEventBatch("trial1", *trial1, layout.findLayout("trial1"))
        .addEventBatch("trial2", *trial2, layout.findLayout("trial2"))
        .withTimeRange(0, 20)
        .buildWithSpatialIndex();
    
    SceneHitTester tester;
    
    // Hit event in trial1
    float trial1_y = layout.findLayout("trial1")->allocated_y_center;
    auto hit = tester.hitTest(5.0f, trial1_y, scene, layout);
    REQUIRE(hit.entity_id == EntityId(1));
    REQUIRE(hit.series_key == "trial1");
    
    // Hit event in trial2 (different row)
    float trial2_y = layout.findLayout("trial2")->allocated_y_center;
    auto hit2 = tester.hitTest(8.0f, trial2_y, scene, layout);
    REQUIRE(hit2.entity_id == EntityId(3));
    REQUIRE(hit2.series_key == "trial2");
}
```

#### Test Scenario 4: Coordinate Transform Round-Trip
```cpp
TEST_CASE("Screen → World → Hit → Verify coordinates") {
    // Setup: 800x600 canvas, time range [0, 1000]
    TimeAxisParams time_params{0, 1000, 800};
    
    // Create a scene with known event at time=500
    auto events = std::make_shared<DigitalEventSeries>();
    events->addEvent(TimeFrameIndex(500), EntityId(42));
    
    // ... build layout and scene ...
    
    // Simulate: User clicks at canvas pixel (400, 300)
    // Step 1: Canvas X → Time
    float time = canvasXToTime(400.0f, time_params);
    REQUIRE(time == Approx(500.0f));  // Mid-screen = mid-time
    
    // Step 2: Canvas → World (using matrices)
    glm::mat4 view = CorePlotting::getEventViewMatrix(...);
    glm::mat4 proj = CorePlotting::getEventProjectionMatrix(...);
    auto world = screenToWorld({400, 300}, {800, 600}, view, proj);
    
    // Step 3: World → Hit test
    auto hit = tester.hitTest(world.x, world.y, scene, layout);
    REQUIRE(hit.entity_id == EntityId(42));
    
    // Step 4: Reverse - Entity position → Screen (for tooltip placement)
    auto screen = worldToScreen({500.0f, event_y}, {800, 600}, view, proj);
    REQUIRE(screen.x == Approx(400.0f));
}
```

#### Test Scenario 5: Mixed Series Priority (Event Beats Analog)
```cpp
TEST_CASE("When event overlaps analog region, event takes priority") {
    // Analog series fills region Y=[-0.5, 0.5]
    // Event at time=100 is placed at Y=0.0 (overlapping analog)
    
    // ... setup with FullCanvas events overlapping analog ...
    
    // Click at (100, 0) - both analog region and event are candidates
    auto hit = tester.hitTest(100.0f, 0.0f, scene, layout, /*tolerance=*/5.0f);
    
    // Event should win (more specific/discrete)
    REQUIRE(hit.hit_type == HitTestResult::HitType::DigitalEvent);
    REQUIRE(hit.entity_id.has_value());
}
```

#### Why This Testing Approach Works

1. **Real Data Types**: Uses actual `DigitalEventSeries`, `AnalogTimeSeries`, etc.
2. **Real Layout**: Uses `LayoutEngine` with real strategies
3. **Real Scene Graph**: Uses `SceneBuilder` and `RenderableScene`
4. **Real Spatial Index**: QuadTree built from actual batch data
5. **No Mocks**: Everything is the production code path
6. **Plain Language**: Each test describes a real user scenario
7. **Backend Agnostic**: No Qt, no OpenGL, no widget code

#### Test Scenario 6: IntervalDragController State Machine
```cpp
TEST_CASE("Interval drag controller enforces constraints") {
    IntervalDragController controller;
    
    // Setup: interval [100, 200], dragging left edge
    HitTestResult hit;
    hit.hit_type = HitTestResult::HitType::IntervalEdgeLeft;
    hit.series_key = "my_intervals";
    
    REQUIRE(controller.tryStartDrag(hit, 100, 200));
    REQUIRE(controller.getState().is_active);
    REQUIRE(controller.getState().is_left_edge == true);
    
    // Drag left edge to 150 (valid)
    auto new_pos = controller.updateDrag(150.0f);
    REQUIRE(new_pos == 150);
    REQUIRE(controller.getState().current_start == 150);
    
    // Try to drag past right edge (should clamp)
    new_pos = controller.updateDrag(250.0f);
    REQUIRE(new_pos < 200);  // Clamped to stay left of right edge
    
    // Finish drag
    auto result = controller.finishDrag();
    REQUIRE(result.has_value());
    REQUIRE(result->first < result->second);  // Valid interval
    REQUIRE_FALSE(controller.getState().is_active);
}

TEST_CASE("Interval drag cancel restores original") {
    IntervalDragController controller;
    // ... start drag ...
    controller.updateDrag(150.0f);  // Move somewhere
    controller.cancelDrag();
    
    REQUIRE_FALSE(controller.getState().is_active);
    // Original values available for UI to restore
}
```

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

### 4.2 Migration Step 1: Replace Coordinate Functions (Low Risk)
**Goal:** Replace `canvasXToTime()` with CorePlotting; refactor `canvasYToAnalogValue()` to query through hierarchy.

- [ ] **Add `TimeAxisCoordinates` to CorePlotting** (Phase 3.1)
- [ ] **Update `OpenGLWidget::canvasXToTime()`**:
    ```cpp
    float OpenGLWidget::canvasXToTime(float canvas_x) const {
        CorePlotting::TimeAxisParams params{
            _xAxis.getStart(), _xAxis.getEnd(), width()
        };
        return CorePlotting::canvasXToTime(canvas_x, params);
    }
    ```
- [ ] **Refactor `canvasYToAnalogValue()` to use query flow**:
    ```cpp
    float OpenGLWidget::canvasYToAnalogValue(float canvas_y, std::string const& series_key) const {
        // 1. Screen → World
        auto world_pos = CorePlotting::screenToWorld(
            {canvas_x, canvas_y}, {width(), height()}, 
            _current_view_matrix, _current_proj_matrix);
        
        // 2. Get series layout from LayoutEngine result (cached)
        auto* layout = _cached_layout.findLayout(series_key);
        if (!layout) return 0.0f;
        
        // 3. World Y → Series-local Y
        float local_y = CorePlotting::worldYToSeriesLocalY(world_pos.y, *layout);
        
        // 4. Let the data object interpret local_y
        // (series knows its own scaling/offset)
        return _analog_series[series_key].series->localYToDataValue(local_y, *layout);
    }
    ```
- [ ] **Verify:** All existing tests pass, hover behavior unchanged

### 4.3 Migration Step 2: Replace Hit Testing (Medium Risk)
**Goal:** Replace manual iteration with `SceneHitTester` queries through SceneGraph.

- [ ] **Add `SceneHitTester` to CorePlotting** (Phase 3.3)
- [ ] **Cache `LayoutResponse` in OpenGLWidget** (rebuilt on series add/remove)
- [ ] **Replace `findIntervalAtTime()`**:
    ```cpp
    std::optional<std::pair<int64_t, int64_t>> OpenGLWidget::findIntervalAtTime(
        std::string const& series_key, float time_coord) const 
    {
        auto hit = _hit_tester.hitTest(time_coord, 0.0f, _current_scene, _cached_layout);
        if (hit.hit_type == HitTestResult::HitType::IntervalBody && 
            hit.series_key == series_key) {
            // Get interval bounds from the interval series
            return getIntervalBoundsForEntity(hit.entity_id.value());
        }
        return std::nullopt;
    }
    ```
- [ ] **Replace `findIntervalEdgeAtPosition()`** with `_hit_tester.findIntervalEdge()`
- [ ] **Replace `findSeriesAtPosition()`** with layout region query
- [ ] **Verify:** Click-to-select intervals still works

### 4.4 Migration Step 3: Replace Interval Dragging (Medium Risk)
**Goal:** Extract state machine from widget into testable controller.

- [ ] **Add `IntervalDragController` to CorePlotting** (Phase 3.4)
- [ ] **Add `_interval_drag_controller` member to OpenGLWidget**
- [ ] **Simplify `mousePressEvent()`**:
    ```cpp
    if (auto hit = _hit_tester.findIntervalEdge(canvas_x, canvas_y)) {
        auto interval = getSelectedInterval(hit->series_key);
        if (interval && _interval_drag_controller.tryStartDrag(*hit, interval->first, interval->second)) {
            // Dragging active
        }
    }
    ```
- [ ] **Simplify `mouseMoveEvent()`**:
    ```cpp
    if (_interval_drag_controller.getState().is_active) {
        _interval_drag_controller.updateDrag(canvas_x, getTimeAxisParams());
        update();
    }
    ```
- [ ] **Simplify `mouseReleaseEvent()`**:
    ```cpp
    if (auto result = _interval_drag_controller.finishDrag()) {
        // Apply interval change
        emit intervalEdgeChanged(...);
    }
    ```
- [ ] **Verify:** Drag behavior unchanged

### 4.5 Migration Step 4: Unify Rendering Path (Higher Risk)
**Goal:** Remove legacy draw functions, use only SceneRenderer path.

- [ ] **Ensure `SceneBuildingHelpers` handles all cases**:
    - [x] `buildAnalogSeriesBatch()` — basic polyline ✓
    - [ ] Gap detection mode (use GapDetector)
    - [ ] Marker mode (use GlyphBatch instead of PolyLineBatch)
    - [x] `buildEventSeriesBatch()` — basic events ✓
    - [ ] Stacked vs FullCanvas modes
    - [x] `buildIntervalSeriesBatch()` — basic intervals ✓
    - [ ] Selection highlight overlays

- [ ] **Update `paintGL()` to only use scene path**:
    ```cpp
    void OpenGLWidget::paintGL() {
        // ... clear, time setup ...
        
        // Build scene (should be cached and only rebuilt on data change)
        auto scene = buildCurrentScene();
        _scene_renderer->render(scene);
        
        // Overlay elements (drag preview, selection)
        renderOverlays();
        
        drawAxis();
        drawGridLines();
    }
    ```

- [ ] **Remove legacy functions**:
    - `drawDigitalEventSeries()` → delete
    - `drawDigitalIntervalSeries()` → delete
    - `drawAnalogSeries()` → delete
    - `_drawAnalogSeriesWithGapDetection()` → delete
    - `_drawAnalogSeriesAsMarkers()` → delete

- [ ] **Verify:** All visual output identical

### 4.6 Migration Step 5: Replace XAxis with TimeSeriesViewState
**Goal:** Use unified camera state instead of separate `_xAxis` + `_yMin/_yMax/_verticalPanOffset`.

- [ ] **Add `_view_state` member** (type: `TimeSeriesViewState`)
- [ ] **Replace `_xAxis` calls**:
    - `_xAxis.getStart()` → `_view_state.time_range.start`
    - `_xAxis.getEnd()` → `_view_state.time_range.end`
    - `_xAxis.setCenterAndZoom()` → `_view_state.time_range.setCenterAndZoom()`
- [ ] **Replace Y-axis state**:
    - `_yMin, _yMax` → `_view_state.view_state.data_bounds`
    - `_verticalPanOffset` → `_view_state.view_state.pan_offset_y`
- [ ] **Update `setMasterTimeFrame()`**:
    ```cpp
    void OpenGLWidget::setMasterTimeFrame(std::shared_ptr<TimeFrame> tf) {
        _master_time_frame = tf;
        _view_state.time_range = TimeRange::fromTimeFrame(*tf);
    }
    ```
- [ ] **Verify:** Zoom/pan behavior unchanged

### 4.7 Final Cleanup
- [ ] Remove `_plotting_manager` dependency (replaced by LayoutEngine)
- [ ] Remove `m_vertices` class member (batches own their data)
- [ ] Remove per-series `_series_y_position` map (use LayoutEngine results)
- [ ] Update tests to use CorePlotting types directly
- [ ] Document new architecture in header comments

---

## Phase 5: Analysis Dashboard Updates
**Goal:** Apply same patterns to EventPlotWidget and other dashboard components.

### 5.1 EventPlotWidget (Raster Plot)
- [ ] Replace `vector<vector<float>>` data format with `DigitalEventSeries`
- [ ] Use `RowLayoutStrategy` from CorePlotting
- [ ] Add EntityId support for hover/click

### 5.2 SpatialOverlayWidget
- [ ] Already using ViewState ✓
- [ ] Add CorePlotting transformers for line/point data
- [ ] Unify tooltip infrastructure

### 5.3 Shared Infrastructure
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
- **Solution**: Both use `SeriesLayoutResult.allocated_y_center` — single source of truth

---

## Migration Checklist (Per Widget)

```
[ ] Widget uses CorePlotting LayoutEngine for positioning
[ ] Widget uses CorePlotting ViewState (or TimeSeriesViewState)
[ ] Widget renders via RenderableScene (not inline vertex generation)
[ ] Widget uses spatial index for hit testing
[ ] SVG export works via same RenderableScene
[ ] Unit tests for layout calculations
[ ] Unit tests for coordinate transforms
```
