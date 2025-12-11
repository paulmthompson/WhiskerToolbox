# CorePlotting Refactoring Roadmap

This document outlines the roadmap for consolidating the plotting architecture in WhiskerToolbox. The goal is to move from parallel, coupled implementations (DataViewer, Analysis Dashboard, SVGExporter) to a layered architecture centered around `CorePlotting`.

## Phase 1: Core Abstractions (The "Brain")
**Goal:** Centralize math, layout, and querying logic. No Qt, No OpenGL.

- [ ] **Consolidate MVP Logic**: 
    - Move matrix calculation logic from `MVP_*.hpp` (DataViewer) and `SVGExporter` into `CorePlotting/CoordinateTransform`.
    - Ensure `SVGExporter` and `OpenGLWidget` can both use these same functions.
- [ ] **Define Renderable Primitives**:
    - Create `RenderableScene`, `RenderablePolyLine`, `RenderableGlyph` structs.
    - These should be POD (Plain Old Data) types optimized for easy consumption by renderers.
- [ ] **Implement Data Transformers**:
    - `GapDetector`: `AnalogTimeSeries` -> `std::vector<RenderablePolyLine>`.
    - `EpochAligner`: `AnalogTimeSeries` + `DigitalEventSeries` -> `std::vector<RenderablePolyLine>`.
- [ ] **Unified Spatial Indexing**:
    - Finalize `QuadTree<EntityId>` implementation.
    - Create adapters for `PointData` and `DigitalEventSeries`.
    - **New**: Create adapter for `RenderablePolyLine` (using bounding boxes) to support selection of aligned analog traces.
- [ ] **Layout Engine**:
    - Implement `EventRow` and `EventRowLayout` structs.
    - Create a `LayoutManager` (evolution of `PlottingManager`) that outputs a "Scene Description" (pure data) rather than drawing directly.

## Phase 2: Rendering Strategies (The "Painter")
**Goal:** Create a library for rendering the "Scene Description" using OpenGL (and eventually others).

- [ ] **Create `PlottingOpenGL` Library**:
    - Implement `SeriesRenderer` classes (e.g., `OpenGLAnalogRenderer`, `OpenGLEventRenderer`) that take `CorePlotting` data structures and draw them.
    - **Strategy**: Use `glDrawArrays` for simple cases, and instanced rendering for "Many Lines" (Epoch plots).
    - Refactor `OpenGLWidget` to delegate drawing to these renderers.
- [ ] **Refactor SVG Exporter**:
    - Update `SVGExporter` to consume `RenderableScene`.
    - It will simply iterate over `poly_lines` and `glyphs` and write SVG tags.

## Phase 3: Interaction & Qt Integration (The "Controller")
**Goal:** Handle user input and bridge it to CorePlotting queries.

- [ ] **Create `QtPlotting` Library**:
    - Implement `InteractionController` (generalizing `PlotInteractionController` from Analysis Dashboard).
    - Handle mouse events (Zoom/Pan/Hover) and translate them into `CorePlotting` queries (e.g., `screenToWorld` -> `QuadTree::query`).
    - **Selection**: Use `CorePlotting` spatial index for selection (CPU-based), removing the dependency on OpenGL 4.3 Compute Shaders for Mac compatibility.
    - Implement a generic `TooltipOverlay` that takes an `EntityId` and resolves it to text/widgets.

## Phase 4: Widget Migration
**Goal:** Update existing widgets to use the new stack.

- [ ] **Migrate DataViewer**:
    - Replace internal `PlottingManager` with `CorePlotting` layout.
    - Use `PlottingOpenGL` for rendering.
- [ ] **Migrate Analysis Dashboard**:
    - Refactor `EventPlotWidget` (Raster) to use `CorePlotting` (treating trials as stacked rows).
    - Refactor `SpatialOverlayWidget` to use `CorePlotting` for PointData.

## Open Questions & Risks
- **Performance**: Will the abstraction layer introduce overhead for high-frequency real-time updates? (Mitigation: Keep `CorePlotting` structs POD and cache-friendly).
- **Legacy Compatibility**: How to handle legacy `DataViewer` features (like specific gap detection logic) during the transition?
