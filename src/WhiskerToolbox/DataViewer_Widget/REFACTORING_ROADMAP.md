# DataViewer Widget Refactoring Roadmap

## Overview

This document outlines the planned refactoring of `OpenGLWidget` to address "god class" symptoms and improve separation of concerns. The goal is to reduce the widget from ~1900 lines to ~500 lines by extracting cohesive subsystems.

**Current State:** OpenGLWidget handles too many responsibilities directly:
- OpenGL lifecycle management
- ~~Series data storage~~ ✅ Extracted (Phase 3)
- ~~Input handling (mouse events, gestures)~~ ✅ Extracted (Phase 1)
- ~~Interaction state machine (interval creation, edge dragging)~~ ✅ Extracted (Phase 1)
- ~~Coordinate transformations~~ ✅ Extracted (Phase 4)
- ~~Selection management~~ ✅ Extracted (Phase 2)
- ~~Tooltip management~~ ✅ Extracted (Phase 2)
- Layout orchestration
- Scene building
- ~~Axis/grid rendering~~ ✅ Extracted (Phase 5)
- Theme management

**Target State:** OpenGLWidget becomes a thin coordinator that:
1. Owns Qt/OpenGL lifecycle
2. Routes input to handlers
3. Coordinates between subsystems
4. Emits signals for external consumers

---

## Phase 1: Extract Input & Interaction Handling ✅ COMPLETED (December 2025)

**Priority:** High  
**Estimated Impact:** ~400 lines removed from OpenGLWidget  
**Status:** ✅ Complete

### 1.1 Create DataViewerInputHandler ✅

Extracted mouse event processing into a dedicated handler.

**Files Created:** `DataViewerInputHandler.hpp/cpp`

**Implementation Notes:**
- Uses `DataViewer::InputContext` struct to receive view state, layout, scene, and dimensions
- Provides callbacks for series info and analog value lookup (avoids tight coupling)
- Hit testing delegated to `CorePlotting::SceneHitTester`
- All state changes communicated via Qt signals

**Signals implemented:**
- `panStarted()`, `panDelta(float)`, `panEnded()` - Pan gesture handling
- `clicked()`, `hoverCoordinates()` - Coordinate emission
- `entityClicked(EntityId, bool)` - Selection with Ctrl modifier support
- `intervalEdgeDragRequested()`, `intervalCreationRequested()` - Interaction triggers
- `cursorChangeRequested()`, `tooltipRequested()`, `tooltipCancelled()` - UI feedback
- `repaintRequested()` - Redraw triggers

### 1.2 Create DataViewerInteractionManager ✅

Extracted the interaction state machine for interval creation and modification.

**Files Created:** `DataViewerInteractionManager.hpp/cpp`

**Implementation Notes:**
- Owns the `IGlyphInteractionController` (moved from OpenGLWidget)
- Uses `DataViewer::InteractionContext` for coordinate conversions
- Handles both interval creation and edge dragging modes
- Preview geometry exposed via `getPreview()` for rendering

**API:**
- `setMode()`, `mode()`, `isActive()`, `cancel()` - Mode management
- `startIntervalCreation()`, `startEdgeDrag()` - Controller lifecycle
- `update()`, `complete()` - Interaction progression
- `getPreview()` - Preview access for rendering

### 1.3 Migration Completed ✅

**Changes to OpenGLWidget:**
- Removed member variables: `_isPanning`, `_lastMousePos`, `_glyph_controller`, `_interaction_mode`, `_interaction_series_key`
- Added: `_input_handler`, `_interaction_manager` (composed components)
- Mouse event handlers now delegate via signal/slot connections
- `interactionMode()` method now delegates to `_interaction_manager->mode()`
- Removed `commitInteraction()` - now handled internally by InteractionManager

**Lines Removed:** ~200 lines of direct mouse handling and interaction logic

---

## Phase 2: Extract Selection & Tooltip Management ✅ COMPLETED

**Priority:** Medium  
**Estimated Impact:** ~150 lines removed
**Status:** Completed December 23, 2025

### 2.1 Create DataViewerSelectionManager ✅

**New File:** `DataViewerSelectionManager.hpp/cpp`

**Implementation Notes:**
- Manages entity selection state with multi-select support (Ctrl+click)
- Emits `selectionChanged(EntityId, bool)` for individual changes
- Emits `selectionCleared()` and `selectionModified()` for batch operations
- `handleEntityClick(EntityId, bool ctrl_pressed)` implements standard selection behavior

**API:**
- `select()`, `deselect()`, `toggle()`, `clear()` - Selection manipulation
- `handleEntityClick()` - Encapsulates click behavior (single vs multi-select)
- `isSelected()`, `selectedEntities()` - Query methods
- `hasSelection()`, `selectionCount()` - Convenience methods

### 2.2 Create DataViewerTooltipController ✅

**New File:** `DataViewerTooltipController.hpp/cpp`

**Implementation Notes:**
- Manages hover-delay tooltip display using QTimer
- Uses callback (`SeriesInfoProvider`) to look up series information
- Formats tooltip text based on series type (Analog, Event, Interval)
- `SeriesInfo` struct provides type, key, and optional value

**API:**
- `scheduleTooltip()`, `cancel()` - Timer control
- `setDelay()`, `delay()` - Configurable delay
- `setSeriesInfoProvider()` - Callback for series lookup
- `isScheduled()` - Query timer state

### 2.3 Migration Completed ✅

**Changes to OpenGLWidget:**
- Removed member variables: `_selected_entities`, `_tooltip_timer`, `_tooltip_hover_pos`, `TOOLTIP_DELAY_MS`
- Removed methods: `startTooltipTimer()`, `cancelTooltipTimer()`, `showSeriesInfoTooltip()`
- Added: `_selection_manager`, `_tooltip_controller` (composed components)
- Selection API methods (`selectEntity()`, etc.) now delegate to `_selection_manager`
- Input handler signals connect to `_tooltip_controller` instead of direct timer calls
- `findSeriesAtPosition()` retained as private helper, used by tooltip callback

**Lines Removed:** ~80 lines of selection and tooltip logic

---

## Phase 3: Consolidate Series Data Storage ✅ COMPLETED

**Priority:** Medium  
**Estimated Impact:** ~200 lines removed, cleaner API
**Status:** Completed December 23, 2025

### 3.1 Create TimeSeriesDataStore ✅

**Files Created:** `TimeSeriesDataStore.hpp/cpp`

**Implementation Notes:**
- Centralized storage for all time series data (analog, event, interval)
- Each series type has its own entry struct (`AnalogSeriesEntry`, etc.)
- Handles display options creation and default color assignment
- Computes intrinsic properties (mean, std_dev) for analog series on add
- Provides `findSeriesTypeByKey()` for series type lookup
- `applyLayoutResponse()` updates display options from layout computation
- Emits `layoutDirty` signal when series are added/removed

**API:**
- `addAnalogSeries()`, `addEventSeries()`, `addIntervalSeries()` - Add with auto-config
- `removeAnalogSeries()`, `removeEventSeries()`, `removeIntervalSeries()` - Remove by key
- `clearAll()` - Remove all series
- `analogSeries()`, `eventSeries()`, `intervalSeries()` - Const accessors
- `analogSeriesMutable()`, etc. - Mutable accessors for internal use
- `getAnalogConfig()`, `getEventConfig()`, `getIntervalConfig()` - Display options access
- `buildLayoutRequest()` - Create LayoutRequest for LayoutEngine
- `applyLayoutResponse()` - Apply computed layout to display options
- `findSeriesTypeByKey()` - Determine series type from key

**Signals:**
- `seriesAdded(QString key, QString type)` - Emitted when series added
- `seriesRemoved(QString key)` - Emitted when series removed
- `cleared()` - Emitted when all series cleared
- `layoutDirty()` - Emitted when layout needs recomputation

### 3.2 Migration Completed ✅

**Changes to OpenGLWidget:**
- Removed member variables: `_analog_series`, `_digital_event_series`, `_digital_interval_series`
- Added: `_data_store` (TimeSeriesDataStore instance)
- All `add*TimeSeries()`, `remove*TimeSeries()`, `clearSeries()` delegate to data store
- `getAnalogConfig()`, `getDigitalEventConfig()`, `getDigitalIntervalConfig()` delegate to data store
- `getAnalogSeriesMap()`, etc. now return `_data_store->analogSeries()` etc.
- `computeAndApplyLayout()` uses `_data_store->applyLayoutResponse()`
- `findSeriesAtPosition()` uses `_data_store->findSeriesTypeByKey()`
- Rendering methods (`addAnalogBatchesToBuilder()`, etc.) iterate via data store accessors
- Legacy type aliases maintained: `AnalogSeriesData` → `DataViewer::AnalogSeriesEntry`
- Legacy `TimeSeriesDefaultValues` namespace delegates to `DataViewer::DefaultColors`

**Lines Removed:** ~120 lines of direct series management and add/remove logic

---

## Phase 4: Extract Coordinate Transforms ✅ COMPLETED

**Priority:** Medium  
**Estimated Impact:** ~100 lines consolidated, reduced duplication
**Status:** Completed December 28, 2025

### 4.1 Create DataViewerCoordinates ✅

**Files Created:** `DataViewerCoordinates.hpp/cpp`

**Implementation Notes:**
- Consolidates all coordinate transformation logic into a single class
- Wraps CorePlotting::TimeAxisParams and YAxisParams for unified interface
- Provides canvas↔world, canvas↔time, and tolerance conversions
- Used by InputHandler, InteractionManager, and OpenGLWidget
- InputContext and InteractionContext structs include `makeCoordinates()` factory method

**API:**
- `canvasXToTime()`, `canvasXToWorldX()` - X coordinate conversion
- `canvasYToWorldY()` - Y coordinate conversion (accounts for pan offset)
- `canvasToWorld()` - Convenience for both X and Y
- `timeToCanvasX()`, `worldYToCanvasY()` - Inverse conversions
- `canvasYToAnalogValue()` - Data value via inverse LayoutTransform
- `pixelToleranceToWorldX()`, `pixelToleranceToWorldY()` - Hit testing tolerances
- `timeUnitsPerPixel()`, `worldYUnitsPerPixel()` - Scale factors

### 4.2 Migration Completed ✅

**Changes to DataViewerInputHandler:**
- Removed local `canvasXToTime()` method
- Uses `_ctx.makeCoordinates()` for all coordinate conversions
- Hit testing tolerance now via `coords.pixelToleranceToWorldX()`

**Changes to DataViewerInteractionManager:**
- Uses `_ctx.makeCoordinates()` for interval edge drag coordinate conversion
- Simplified `timeToCanvasX()` calls via DataViewerCoordinates

**Changes to OpenGLWidget:**
- `canvasXToTime()` now delegates to DataViewerCoordinates
- `canvasYToAnalogValue()` uses DataViewerCoordinates for canvas→world conversion
- `findIntervalEdgeAtPosition()` uses DataViewerCoordinates
- `hitTestAtPosition()` uses DataViewerCoordinates with structured binding

**Lines Consolidated:** ~80 lines of duplicated coordinate conversion code

---

## Phase 5: Extract Axis/Grid Rendering ✅ COMPLETED

**Priority:** Low  
**Estimated Impact:** ~100 lines removed, consistent rendering path
**Status:** Completed December 28, 2025

### 5.1 Create AxisRenderer in PlottingOpenGL ✅

**Files Created:** `PlottingOpenGL/Renderers/AxisRenderer.hpp/cpp`

**Implementation Notes:**
- Dedicated renderer for axis lines and dashed grid overlays
- Manages own VAO/VBO for line rendering
- Uses "axes" shader (colored_vertex.vert/frag) for solid axis lines
- Uses "dashed_line" shader for grid lines with configurable dash/gap
- Caches uniform locations on initialization for efficiency
- Two-stage architecture: Stage 1 (OpenGL) complete, Stage 2 (CorePlotting scene graph) for future SVG export

**Structs:**
```cpp
struct AxisConfig {
    float x_position{0.0f};    // X position of vertical axis
    float y_min{-1.0f};        // Y range minimum
    float y_max{1.0f};         // Y range maximum
    glm::vec3 color{1.0f};     // RGB color
    float alpha{1.0f};         // Transparency
};

struct GridConfig {
    int64_t time_start{0};     // Time range start
    int64_t time_end{1000};    // Time range end
    int64_t spacing{100};      // Grid line spacing in time units
    float y_min{-1.0f};        // Y range for grid lines
    float y_max{1.0f};
    glm::vec3 color{0.5f};     // Gray by default
    float alpha{0.5f};
    float dash_length{3.0f};
    float gap_length{3.0f};
};
```

**API:**
- `initialize()` - Create VAO/VBO and cache shader uniform locations
- `cleanup()` - Release OpenGL resources
- `isInitialized()` - Query initialization state
- `renderAxis()` - Draw solid vertical axis line
- `renderGrid()` - Draw time-aligned dashed vertical grid lines
- `renderDashedLine()` - Internal helper for individual dashed lines

### 5.2 Migration Completed ✅

**Changes to OpenGLWidget:**
- Added `#include "PlottingOpenGL/Renderers/AxisRenderer.hpp"`
- Added member: `std::unique_ptr<PlottingOpenGL::AxisRenderer> _axis_renderer`
- `initializeGL()` creates and initializes `_axis_renderer`
- `cleanup()` cleans up `_axis_renderer`
- Removed uniform location members: `m_projMatrixLoc`, `m_viewMatrixLoc`, `m_modelMatrixLoc`, `m_colorLoc`, `m_alphaLoc`, `m_dashedProjMatrixLoc`, `m_dashedViewMatrixLoc`, `m_dashedModelMatrixLoc`, `m_dashedResolutionLoc`, `m_dashedDashSizeLoc`, `m_dashedGapSizeLoc`
- `drawAxis()` now delegates to `_axis_renderer->renderAxis()`
- `drawGridLines()` now delegates to `_axis_renderer->renderGrid()`
- Removed `drawDashedLine()` method entirely
- Removed `LineParameters` struct from header

**Lines Removed:** ~80 lines of direct shader management and dashed line rendering

---

## Phase 6: Consolidate Member Variables ✅ COMPLETED

**Priority:** Low  
**Estimated Impact:** Improved readability, reduced cognitive load
**Status:** Completed December 28, 2025

### 6.1 Group Related State into Structs ✅

Created four state structs to organize related member variables:

**ThemeState** - Visual theme configuration:
```cpp
struct ThemeState {
    PlotTheme theme{PlotTheme::Dark};
    std::string background_color{"#000000"};
    std::string axis_color{"#FFFFFF"};
};
```

**GridState** - Grid overlay configuration:
```cpp
struct GridState {
    bool enabled{false};
    int spacing{100};
};
```

**SceneCacheState** - Cached rendering/hit-testing data:
```cpp
struct SceneCacheState {
    CorePlotting::RenderableScene scene;
    CorePlotting::LayoutResponse layout_response;
    std::map<size_t, std::string> rectangle_batch_key_map;
    std::map<size_t, std::string> glyph_batch_key_map;
    bool scene_dirty{true};
    bool layout_response_dirty{true};
};
```

**OpenGLResourceState** - OpenGL lifecycle resources:
```cpp
struct OpenGLResourceState {
    QOpenGLShaderProgram * program{nullptr};
    QOpenGLBuffer vbo;
    QOpenGLVertexArrayObject vao;
    QMatrix4x4 proj, view, model;
    bool initialized{false};
    QMetaObject::Connection ctx_about_to_be_destroyed_conn;
    ShaderSourceType shader_source_type{ShaderSourceType::Resource};
};
```

### 6.2 Migration Completed ✅

**Changes to OpenGLWidget.hpp:**
- Added struct definitions before `InteractionMode` enum
- Replaced 20+ scattered member variables with 4 grouped structs:
  - `_gl_state` (OpenGLResourceState)
  - `_theme_state` (ThemeState)
  - `_grid_state` (GridState)
  - `_cache_state` (SceneCacheState)
- Updated inline getter methods to use new struct members

**Changes to OpenGLWidget.cpp:**
- Updated all references from old variable names to new struct accessors:
  - `m_program` → `_gl_state.program`
  - `m_vbo`/`m_vao` → `_gl_state.vbo`/`_gl_state.vao`
  - `m_proj`/`m_view`/`m_model` → `_gl_state.proj`/`_gl_state.view`/`_gl_state.model`
  - `_gl_initialized` → `_gl_state.initialized`
  - `_ctxAboutToBeDestroyedConn` → `_gl_state.ctx_about_to_be_destroyed_conn`
  - `m_shaderSourceType` → `_gl_state.shader_source_type`
  - `m_background_color`/`m_axis_color` → `_theme_state.background_color`/`_theme_state.axis_color`
  - `_plot_theme` → `_theme_state.theme`
  - `_grid_lines_enabled`/`_grid_spacing` → `_grid_state.enabled`/`_grid_state.spacing`
  - `_cached_scene` → `_cache_state.scene`
  - `_cached_layout_response` → `_cache_state.layout_response`
  - `_scene_dirty`/`_layout_response_dirty` → `_cache_state.scene_dirty`/`_cache_state.layout_response_dirty`
  - `_rectangle_batch_key_map`/`_glyph_batch_key_map` → `_cache_state.rectangle_batch_key_map`/`_cache_state.glyph_batch_key_map`

**Benefits:**
- Related variables are now co-located in logical groups
- Easier to understand state dependencies
- Improved code navigation and maintenance
- Clearer initialization patterns

---

## Current Module Structure

```
DataViewer_Widget/
├── OpenGLWidget.hpp/cpp                 # Main widget (~1400 lines, down from ~1900)
├── DataViewerInputHandler.hpp/cpp       # ✅ Mouse events, gestures (Phase 1)
├── DataViewerInteractionManager.hpp/cpp # ✅ Interaction state machine (Phase 1)
├── DataViewerSelectionManager.hpp/cpp   # ✅ EntityId selection (Phase 2)
├── DataViewerTooltipController.hpp/cpp  # ✅ Tooltip timer + display (Phase 2)
├── TimeSeriesDataStore.hpp/cpp          # ✅ Series storage + display options (Phase 3)
├── DataViewerCoordinates.hpp/cpp        # ✅ Coordinate transforms (Phase 4)
├── SceneBuildingHelpers.hpp/cpp         # (existing) Batch building
├── SVGExporter.hpp/cpp                  # (existing) SVG export
├── SpikeSorterConfigLoader.hpp/cpp      # (existing) Spike sorter config
├── AnalogTimeSeries/                    # (existing) Analog display options
├── DigitalEvent/                        # (existing) Event display options
├── DigitalInterval/                     # (existing) Interval display options
└── REFACTORING_ROADMAP.md               # This document
```

---

## Migration Strategy

### Approach: Incremental Extraction

Each phase can be completed independently. Recommended order:

1. **Phase 1 (Input/Interaction)** ✅ COMPLETED - Highest impact, most complex
2. **Phase 2 (Selection/Tooltip)** ✅ COMPLETED - Quick wins, simple extraction
3. **Phase 3 (Data Store)** ✅ COMPLETED - Medium complexity, significant cleanup
4. **Phase 4 (Coordinates)** ✅ COMPLETED - Low risk, reduces duplication
5. **Phase 5 (Axis Rendering)** ✅ COMPLETED - Extracted to PlottingOpenGL
6. **Phase 6 (Variable Grouping)** ✅ COMPLETED - Final polish, improved readability

### Testing Strategy

- Each extracted class should have its own unit test file
- Integration tests via existing `OpenGLWidget.test.cpp`
- Manual testing of interaction flows after each phase

### Compatibility Notes

- External API (`addAnalogTimeSeries()`, `setInteractionMode()`, etc.) should remain stable
- Signals (`entitySelectionChanged`, `interactionCompleted`, etc.) should remain stable
- Internal implementation can change freely

---

## Success Metrics

| Metric | Before | After Phase 1 | After Phase 2 | After Phase 3 | After Phase 4 | After Phase 5 | After Phase 6 | Target |
|--------|--------|---------------|---------------|---------------|---------------|---------------|---------------|--------|
| OpenGLWidget LOC | ~1900 | ~1700 | ~1620 | ~1500 | ~1400 | ~1350 | ~1350 | ~500 |
| Member variables | ~60 | ~55 | ~50 | ~45 | ~45 | ~35 | ~25 (4 structs) | ~20 |
| Public methods | ~50 | ~50 | ~50 | ~50 | ~50 | ~50 | ~50 | ~25 |
| Extracted classes | 0 | 2 | 4 | 5 | 6 | 7 | 7 | 6+ ✅ |
| Test coverage | Partial | Partial | Partial | Partial | Partial | Partial | Partial | Full per-component |

---

## Related Documents

- [CorePlotting DESIGN.md](../../../CorePlotting/DESIGN.md) - Library architecture
- [CorePlotting ROADMAP.md](../../../CorePlotting/ROADMAP.md) - Library migration history
- [PlottingOpenGL](../../../PlottingOpenGL/) - OpenGL rendering layer

---

*Last Updated: December 28, 2025*
