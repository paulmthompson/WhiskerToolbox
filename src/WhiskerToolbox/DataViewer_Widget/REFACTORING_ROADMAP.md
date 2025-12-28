# DataViewer Widget Refactoring Roadmap

## Overview

This document outlines the planned refactoring of `OpenGLWidget` to address "god class" symptoms and improve separation of concerns. The goal is to reduce the widget from ~1900 lines to ~500 lines by extracting cohesive subsystems.

**Current State:** OpenGLWidget handles too many responsibilities directly:
- OpenGL lifecycle management
- ~~Series data storage~~ ✅ Extracted (Phase 3)
- ~~Input handling (mouse events, gestures)~~ ✅ Extracted (Phase 1)
- ~~Interaction state machine (interval creation, edge dragging)~~ ✅ Extracted (Phase 1)
- Coordinate transformations
- ~~Selection management~~ ✅ Extracted (Phase 2)
- ~~Tooltip management~~ ✅ Extracted (Phase 2)
- Layout orchestration
- Scene building
- Axis/grid rendering
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

## Phase 5: Extract Axis/Grid Rendering

**Priority:** Low  
**Estimated Impact:** ~100 lines removed, consistent rendering path

### 5.1 Option A: Add AxisRenderer to PlottingOpenGL

**New File:** `PlottingOpenGL/AxisRenderer.hpp/cpp`

```cpp
namespace PlottingOpenGL {

struct AxisConfig {
    float x_position;          // X position of vertical axis
    float y_min, y_max;        // Y range
    glm::vec3 color;
    float alpha{1.0f};
};

struct GridConfig {
    int64_t time_start, time_end;
    int64_t spacing;
    float y_min, y_max;
    float dash_length{3.0f};
    float gap_length{3.0f};
};

class AxisRenderer {
public:
    bool initialize();
    void cleanup();
    
    void renderAxis(const AxisConfig& config, const glm::mat4& mvp);
    void renderGridLines(const GridConfig& config, const glm::mat4& mvp,
                         int viewport_width, int viewport_height);
    
private:
    ShaderProgram* _solid_shader{nullptr};
    ShaderProgram* _dashed_shader{nullptr};
    GLuint _vao{0}, _vbo{0};
};

} // namespace PlottingOpenGL
```

### 5.2 Option B: Integrate into SceneRenderer

Extend `SceneRenderer` to accept axis/grid configuration:

```cpp
struct AxisOverlay {
    std::optional<AxisConfig> axis;
    std::optional<GridConfig> grid;
};

void SceneRenderer::renderOverlays(const AxisOverlay& overlays, const glm::mat4& vp);
```

**Responsibilities moved:**
- `drawAxis()`, `drawGridLines()`, `drawDashedLine()`
- Shader uniform locations for axes/dashed programs
- `_grid_lines_enabled`, `_grid_spacing` state

---

## Phase 6: Consolidate Member Variables

**Priority:** Low  
**Estimated Impact:** Improved readability, reduced cognitive load

### 6.1 Group Related State into Structs

```cpp
// In OpenGLWidget.hpp

struct ShaderState {
    QOpenGLShaderProgram* axes_program{nullptr};
    QOpenGLShaderProgram* dashed_program{nullptr};
    struct Locations {
        int proj{-1}, view{-1}, model{-1}, color{-1}, alpha{-1};
    } axes_locs;
    struct DashedLocations {
        int proj{-1}, view{-1}, model{-1}, resolution{-1}, dash{-1}, gap{-1};
    } dashed_locs;
};

struct ThemeState {
    PlotTheme theme{PlotTheme::Dark};
    std::string background_color{"#000000"};
    std::string axis_color{"#FFFFFF"};
};

struct GridState {
    bool enabled{false};
    int spacing{100};
};

struct CacheState {
    CorePlotting::RenderableScene scene;
    CorePlotting::LayoutResponse layout_response;
    std::map<size_t, std::string> rectangle_batch_key_map;
    std::map<size_t, std::string> glyph_batch_key_map;
    bool scene_dirty{true};
    bool layout_dirty{true};
};
```

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
5. **Phase 5 (Axis Rendering)** - Can be done alongside PlottingOpenGL work
6. **Phase 6 (Variable Grouping)** - Final polish

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

| Metric | Before | After Phase 1 | After Phase 2 | After Phase 3 | After Phase 4 | Target |
|--------|--------|---------------|---------------|---------------|---------------|--------|
| OpenGLWidget LOC | ~1900 | ~1700 | ~1620 | ~1500 | ~1400 | ~500 |
| Member variables | ~60 | ~55 | ~50 | ~45 | ~45 | ~20 |
| Public methods | ~50 | ~50 | ~50 | ~50 | ~50 | ~25 |
| Extracted classes | 0 | 2 | 4 | 5 | 6 | 6+ |
| Test coverage | Partial | Partial | Partial | Partial | Partial | Full per-component |

---

## Related Documents

- [CorePlotting DESIGN.md](../../../CorePlotting/DESIGN.md) - Library architecture
- [CorePlotting ROADMAP.md](../../../CorePlotting/ROADMAP.md) - Library migration history
- [PlottingOpenGL](../../../PlottingOpenGL/) - OpenGL rendering layer

---

*Last Updated: December 28, 2025*
