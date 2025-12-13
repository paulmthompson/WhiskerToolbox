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

### 1.3 Layout Engine
- [ ] **Create `LayoutEngine`** (evolution of PlottingManager):
    - Pure functions that take series count/configuration → layout positions
    - No data storage, no global state
    - Input: `LayoutRequest` (series types, counts, viewport bounds)
    - Output: `std::vector<SeriesLayoutResult>`
    
- [ ] **Implement layout strategies**:
    - `StackedLayoutStrategy`: DataViewer-style vertical stacking
    - `RowLayoutStrategy`: Raster plot-style horizontal rows

### 1.4 Renderable Primitives
- [ ] **Finalize `RenderableScene` struct**:
    - Each batch contains its own Model matrix
    - Scene contains shared View + Projection matrices
    - Add `SceneBuilder` helper class

- [ ] **Implement Transformers**:
    - `GapDetector`: `AnalogTimeSeries` → segmented `RenderablePolyLineBatch`
    - `RasterBuilder`: `DigitalEventSeries` + row layout → `RenderableGlyphBatch`

### 1.5 Spatial Indexing
- [ ] **Finalize `QuadTree<EntityId>` implementation**
- [ ] **Create spatial adapters**:
    - `EventSpatialAdapter`: DigitalEventSeries → QuadTree
    - `PointSpatialAdapter`: PointData → QuadTree
    - `PolyLineSpatialAdapter`: RenderablePolyLineBatch → QuadTree (bounding boxes)

---

## Phase 2: Rendering Strategies (The "Painter")
**Goal:** Create a library for rendering the "Scene Description" using OpenGL.

- [ ] **Create `PlottingOpenGL` library** (or folder in WhiskerToolbox):
    - `BatchRenderer` base class
    - `PolyLineRenderer`: Takes `RenderablePolyLineBatch`, issues draw calls
    - `GlyphRenderer`: Takes `RenderableGlyphBatch`, uses instancing
    - `RectangleRenderer`: Takes `RenderableRectangleBatch`

- [ ] **Refactor `OpenGLWidget` to use renderers**:
    - Replace inline vertex generation with `RenderableScene` consumption
    - Shader management stays in widget (or moves to renderer)

- [ ] **Refactor `SVGExporter`**:
    - Consume `RenderableScene` instead of querying OpenGLWidget
    - Iterate batches → write SVG elements
    - Same coordinate transforms as OpenGL (validates correctness)

---

## Phase 3: Interaction & Qt Integration (The "Controller")
**Goal:** Handle user input and bridge it to CorePlotting queries.

- [ ] **Port `PlotInteractionController`** patterns to DataViewer:
    - Generalize for time-series context (TimeRange-aware zoom/pan)
    - Handle interval dragging through same abstraction

- [ ] **Adopt `ViewState` in DataViewer**:
    - Replace `_verticalPanOffset`, `_yMin`, `_yMax` with ViewState
    - Use `TimeSeriesViewState` for integrated time + Y management

- [ ] **Implement spatial queries for DataViewer**:
    - Build QuadTree from visible series
    - Use for tooltip/hover detection
    - Use for selection (replaces current linear search)

---

## Phase 4: Widget Migration
**Goal:** Update existing widgets to use the new stack.

### 4.1 DataViewer Migration
- [ ] Replace internal layout logic with `LayoutEngine`
- [ ] Replace inline rendering with `PlottingOpenGL` renderers
- [ ] Replace XAxis with `TimeSeriesViewState` + `TimeRange`
- [ ] Add spatial indexing for hover/selection

### 4.2 Analysis Dashboard Updates
- [ ] `EventPlotWidget` (Raster): Use `CorePlotting` row layout
- [ ] `SpatialOverlayWidget`: Already using ViewState, add CorePlotting transformers
- [ ] Shared tooltip infrastructure across widgets

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
