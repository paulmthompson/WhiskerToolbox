# DataViewer Widget Refactoring Roadmap

## Overview

This document outlines the planned refactoring of `OpenGLWidget` to address "god class" symptoms and improve separation of concerns. The goal is to reduce the widget from ~1900 lines to ~500 lines by extracting cohesive subsystems.

**Current State:** OpenGLWidget handles too many responsibilities directly:
- OpenGL lifecycle management
- Series data storage
- Input handling (mouse events, gestures)
- Interaction state machine (interval creation, edge dragging)
- Coordinate transformations
- Selection management
- Tooltip management
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

## Phase 1: Extract Input & Interaction Handling

**Priority:** High  
**Estimated Impact:** ~400 lines removed from OpenGLWidget

### 1.1 Create DataViewerInputHandler

Extract mouse event processing into a dedicated handler.

**New File:** `DataViewerInputHandler.hpp/cpp`

```cpp
class DataViewerInputHandler : public QObject {
    Q_OBJECT
public:
    struct Context {
        const CorePlotting::TimeSeriesViewState* view_state;
        const CorePlotting::LayoutResponse* layout;
        const CorePlotting::RenderableScene* scene;
        int widget_width, widget_height;
    };
    
    void setContext(Context ctx);
    void handleMousePress(QMouseEvent* event);
    void handleMouseMove(QMouseEvent* event);
    void handleMouseRelease(QMouseEvent* event);
    void handleDoubleClick(QMouseEvent* event);
    void handleLeave();
    
signals:
    void panStarted();
    void panDelta(float normalized_dy);
    void panEnded();
    void clicked(float time_coord, float canvas_y, const QString& series_info);
    void hoverCoordinates(float time, float canvas_y, const QString& info);
    void entityClicked(EntityId id, bool ctrl_pressed);
    void intervalEdgeHovered(bool is_edge);
    void doubleClicked(float canvas_x, float canvas_y);
    void requestRepaint();
};
```

**Responsibilities moved:**
- Pan gesture detection and delta calculation
- Hover coordinate emission
- Entity click detection (via hit testing)
- Cursor shape management (edge hover detection)
- Tooltip timer triggering

### 1.2 Create DataViewerInteractionManager

Extract the interaction state machine for interval creation and modification.

**New File:** `DataViewerInteractionManager.hpp/cpp`

```cpp
class DataViewerInteractionManager : public QObject {
    Q_OBJECT
public:
    void setCoordinateContext(/* time axis params, view state */);
    
    // Mode management
    void setMode(InteractionMode mode);
    InteractionMode mode() const;
    bool isActive() const;
    void cancel();
    
    // Controller lifecycle
    void startIntervalCreation(const QString& series_key, float canvas_x, float canvas_y,
                               const glm::vec4& fill_color, const glm::vec4& stroke_color);
    void startEdgeDrag(const CorePlotting::HitTestResult& hit_result,
                       const glm::vec4& fill_color, const glm::vec4& stroke_color);
    void update(float canvas_x, float canvas_y);
    void complete();
    
    // Preview access (for rendering)
    std::optional<CorePlotting::Interaction::GlyphPreview> getPreview() const;
    
signals:
    void modeChanged(InteractionMode mode);
    void interactionCompleted(const CorePlotting::Interaction::DataCoordinates& coords);
    void previewUpdated();
    void cursorChanged(Qt::CursorShape cursor);
};
```

**Responsibilities moved:**
- `setInteractionMode()`, `isInteractionActive()`, `cancelActiveInteraction()`
- `startIntervalCreationUnified()`, `startIntervalEdgeDragUnified()`
- `commitInteraction()` (coordinate conversion part)
- `drawInteractionPreview()` trigger logic
- `_glyph_controller` ownership
- `_interaction_mode`, `_interaction_series_key` state

### 1.3 Migration Steps

1. Create header/source files for both classes
2. Move relevant member variables and methods
3. Add signal/slot connections in OpenGLWidget constructor
4. Update mouse event handlers to delegate to InputHandler
5. Connect InteractionManager signals to OpenGLWidget slots
6. Remove migrated code from OpenGLWidget
7. Update tests

---

## Phase 2: Extract Selection & Tooltip Management

**Priority:** Medium  
**Estimated Impact:** ~150 lines removed

### 2.1 Create DataViewerSelectionManager

**New File:** `DataViewerSelectionManager.hpp/cpp`

```cpp
class DataViewerSelectionManager : public QObject {
    Q_OBJECT
public:
    void select(EntityId id);
    void deselect(EntityId id);
    void toggle(EntityId id);
    void clear();
    bool isSelected(EntityId id) const;
    const std::unordered_set<EntityId>& selectedEntities() const;
    
signals:
    void selectionChanged(EntityId id, bool selected);
    void selectionCleared();
};
```

**Responsibilities moved:**
- `selectEntity()`, `deselectEntity()`, `toggleEntitySelection()`, `clearEntitySelection()`
- `isEntitySelected()`, `getSelectedEntities()`
- `_selected_entities` storage

### 2.2 Create DataViewerTooltipController

**New File:** `DataViewerTooltipController.hpp/cpp`

```cpp
class DataViewerTooltipController : public QObject {
    Q_OBJECT
public:
    explicit DataViewerTooltipController(QWidget* parent);
    
    void scheduleTooltip(const QPoint& pos);
    void cancel();
    void setSeriesInfoProvider(std::function<std::optional<SeriesInfo>(float, float)> provider);
    
private slots:
    void showTooltip();
    
private:
    QTimer* _timer;
    QPoint _hover_pos;
    std::function<std::optional<SeriesInfo>(float, float)> _info_provider;
};
```

**Responsibilities moved:**
- `startTooltipTimer()`, `cancelTooltipTimer()`, `showSeriesInfoTooltip()`
- `findSeriesAtPosition()` (as a callback provider)
- `_tooltip_timer`, `_tooltip_hover_pos`

---

## Phase 3: Consolidate Series Data Storage

**Priority:** Medium  
**Estimated Impact:** ~200 lines removed, cleaner API

### 3.1 Create TimeSeriesDataStore

**New File:** `TimeSeriesDataStore.hpp/cpp`

```cpp
class TimeSeriesDataStore : public QObject {
    Q_OBJECT
public:
    // Add series
    void addAnalogSeries(const std::string& key, std::shared_ptr<AnalogTimeSeries> series,
                         const std::string& color = "");
    void addEventSeries(const std::string& key, std::shared_ptr<DigitalEventSeries> series,
                        const std::string& color = "");
    void addIntervalSeries(const std::string& key, std::shared_ptr<DigitalIntervalSeries> series,
                           const std::string& color = "");
    
    // Remove series
    void removeAnalogSeries(const std::string& key);
    void removeEventSeries(const std::string& key);
    void removeIntervalSeries(const std::string& key);
    void clearAll();
    
    // Accessors
    const auto& analogSeries() const;
    const auto& eventSeries() const;
    const auto& intervalSeries() const;
    
    // Display options access
    std::optional<NewAnalogTimeSeriesDisplayOptions*> getAnalogConfig(const std::string& key);
    std::optional<NewDigitalEventSeriesDisplayOptions*> getEventConfig(const std::string& key);
    std::optional<NewDigitalIntervalSeriesDisplayOptions*> getIntervalConfig(const std::string& key);
    
    // Layout helpers
    CorePlotting::LayoutRequest buildLayoutRequest(const SpikeSorterConfigMap& configs) const;
    void applyLayoutResponse(const CorePlotting::LayoutResponse& response);
    
    // Series lookup by type
    std::string findSeriesTypeByKey(const std::string& key) const;
    
signals:
    void seriesAdded(const QString& key, const QString& type);
    void seriesRemoved(const QString& key);
    void layoutDirty();
};
```

**Responsibilities moved:**
- `_analog_series`, `_digital_event_series`, `_digital_interval_series` maps
- All `add*Series()`, `remove*Series()`, `clearSeries()` methods
- `getAnalogConfig()`, `getDigitalEventConfig()`, `getDigitalIntervalConfig()`
- `buildLayoutRequest()`, `computeAndApplyLayout()` (apply part)
- Default color assignment logic from `TimeSeriesDefaultValues`

---

## Phase 4: Extract Coordinate Transforms

**Priority:** Medium  
**Estimated Impact:** ~100 lines consolidated, reduced duplication

### 4.1 Create DataViewerCoordinates

**New File:** `DataViewerCoordinates.hpp/cpp`

```cpp
class DataViewerCoordinates {
public:
    DataViewerCoordinates(const CorePlotting::TimeSeriesViewState& view_state,
                          int width, int height);
    
    // Canvas to world/time
    float canvasXToTime(float canvas_x) const;
    float canvasYToWorldY(float canvas_y) const;
    glm::vec2 canvasToWorld(float canvas_x, float canvas_y) const;
    
    // World/time to canvas
    float timeToCanvasX(float time) const;
    float worldYToCanvasY(float world_y) const;
    
    // Inverse transform for analog values
    float canvasYToAnalogValue(float canvas_y, const CorePlotting::LayoutTransform& y_transform) const;
    
    // Tolerance conversion
    float pixelToleranceToWorld(float pixels) const;
    
private:
    CorePlotting::TimeAxisParams _time_params;
    CorePlotting::YAxisParams _y_params;
};
```

**Responsibilities moved:**
- `canvasXToTime()`, `canvasYToAnalogValue()`
- Inline coordinate conversions in mouse handlers
- Tolerance calculations in hit testing

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

## Proposed Final Module Structure

```
DataViewer_Widget/
├── OpenGLWidget.hpp/cpp                 # Slim coordinator (~500 lines)
├── DataViewerInputHandler.hpp/cpp       # Mouse events, gestures
├── DataViewerInteractionManager.hpp/cpp # Interaction state machine
├── DataViewerSelectionManager.hpp/cpp   # EntityId selection
├── DataViewerTooltipController.hpp/cpp  # Tooltip timer + display
├── DataViewerCoordinates.hpp/cpp        # Coordinate transforms
├── TimeSeriesDataStore.hpp/cpp          # Series storage + display options
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

1. **Phase 1 (Input/Interaction)** - Highest impact, most complex
2. **Phase 2 (Selection/Tooltip)** - Quick wins, simple extraction
3. **Phase 3 (Data Store)** - Medium complexity, significant cleanup
4. **Phase 4 (Coordinates)** - Low risk, reduces duplication
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

| Metric | Before | Target |
|--------|--------|--------|
| OpenGLWidget LOC | ~1900 | ~500 |
| Member variables | ~60 | ~20 |
| Public methods | ~50 | ~25 |
| Cyclomatic complexity (paintGL path) | High | Low |
| Test coverage | Partial | Full per-component |

---

## Related Documents

- [CorePlotting DESIGN.md](../../../CorePlotting/DESIGN.md) - Library architecture
- [CorePlotting ROADMAP.md](../../../CorePlotting/ROADMAP.md) - Library migration history
- [PlottingOpenGL](../../../PlottingOpenGL/) - OpenGL rendering layer

---

*Last Updated: December 2025*
