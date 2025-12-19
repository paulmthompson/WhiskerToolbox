# Interaction System Refactor Roadmap

## Overview

This document outlines the phased implementation plan for the unified glyph interaction system. The goal is to create a clean, reusable architecture for creating and modifying visual elements (intervals, lines, rectangles, points) across different widgets (DataViewer, Analysis Dashboard, etc.).

## Design Decisions

Based on discussion, the following architectural decisions have been made:

1. **Coordinate System**: Controllers work in **canvas coordinates** (pixels). The Scene handles all coordinate transformations.

2. **Modification Initiation**: For modifying existing glyphs (e.g., edge dragging), the **widget pre-processes** the `HitTestResult` and tells the controller which entity/edge is being modified via the `start()` parameters.

3. **Series Layout Access**: Layout transforms are **passed at conversion time** to `previewToDataCoords()`. This keeps the Scene stateless regarding layout and avoids synchronization issues.

4. **Preview Rendering**: `PreviewRenderer` handles canvas→NDC conversion internally using `glOrtho(0, width, height, 0, -1, 1)`. This keeps `GlyphPreview` in intuitive canvas coordinates.

---

## Phase 1: Core Infrastructure (Foundation)

**Goal**: Establish the base types and interfaces without breaking existing functionality.

### 1.1 Finalize GlyphPreview (✅ Started)

- [x] Create `GlyphPreview.hpp` with basic structure
- [ ] Add `IGlyphInteractionController` to separate header
- [ ] Add namespace `CorePlotting::Interaction`
- [ ] Add documentation comments

**Files**:
- `Interaction/GlyphPreview.hpp` (update)
- `Interaction/IGlyphInteractionController.hpp` (new)

### 1.2 DataCoordinates Type

Create the output type for coordinate conversion results.

```cpp
// Interaction/DataCoordinates.hpp
namespace CorePlotting::Interaction {

struct DataCoordinates {
    std::string series_key;
    std::optional<EntityId> entity_id;
    bool is_modification{false};  // true if modifying existing, false if creating new
    
    // Type-specific coordinates (in data space)
    struct IntervalCoords { 
        int64_t start; 
        int64_t end; 
    };
    struct LineCoords { 
        float x1, y1, x2, y2; 
    };
    struct PointCoords { 
        float x, y; 
    };
    struct RectCoords { 
        float x, y, width, height; 
    };
    
    std::variant<std::monostate, IntervalCoords, LineCoords, PointCoords, RectCoords> coords;
    
    // Convenience accessors
    [[nodiscard]] bool hasCoords() const;
    [[nodiscard]] bool isInterval() const;
    [[nodiscard]] IntervalCoords const& asInterval() const;
    // ... etc
};

} // namespace
```

**Files**:
- `Interaction/DataCoordinates.hpp` (new)

### 1.3 InverseCoordinateTransform Utilities

Add inverse coordinate transform functions to CoordinateTransform module.

```cpp
// CoordinateTransform/InverseTransform.hpp
namespace CorePlotting {

/// Canvas pixel X/Y → World coordinates (requires VP matrices)
[[nodiscard]] glm::vec2 canvasToWorld(
    float canvas_x, float canvas_y,
    int viewport_width, int viewport_height,
    glm::mat4 const& view_matrix,
    glm::mat4 const& projection_matrix);

/// World X → TimeFrameIndex (for time-series X axis)
[[nodiscard]] int64_t worldXToTimeIndex(float world_x);  // Simple rounding

/// World Y → Data Y (apply inverse of LayoutTransform)
[[nodiscard]] float worldYToDataY(float world_y, LayoutTransform const& y_transform);

} // namespace
```

**Files**:
- `CoordinateTransform/InverseTransform.hpp` (new)
- `CoordinateTransform/InverseTransform.cpp` (new)

---

## Phase 2: Concrete Controllers

**Goal**: Implement reusable interaction controllers for each primitive type.

### 2.1 RectangleInteractionController

Handles both interval creation and rectangle selection boxes.

```cpp
// Interaction/RectangleInteractionController.hpp

class RectangleInteractionController : public IGlyphInteractionController {
public:
    // Configuration
    struct Config {
        float min_width{1.0f};   // Minimum rectangle width in canvas pixels
        float min_height{1.0f}; // Minimum height (0 = full height for intervals)
        bool constrain_to_x_axis{true};  // If true, height spans full canvas (interval mode)
        bool allow_edge_drag{true};      // If true, supports modification of existing
    };
    
    explicit RectangleInteractionController(Config config = {});
    
    // For modification: specify which edge is being dragged
    enum class DragEdge { None, Left, Right, Top, Bottom };
    void startEdgeDrag(float canvas_x, float canvas_y,
                       std::string series_key,
                       EntityId entity_id,
                       DragEdge edge,
                       glm::vec4 original_bounds);  // Original rectangle in canvas coords
    
    // IGlyphInteractionController interface
    void start(float canvas_x, float canvas_y,
               std::string series_key,
               std::optional<EntityId> existing = std::nullopt) override;
    void update(float canvas_x, float canvas_y) override;
    void complete() override;
    void cancel() override;
    
    [[nodiscard]] bool isActive() const override;
    [[nodiscard]] GlyphPreview getPreview() const override;
    [[nodiscard]] std::string const& getSeriesKey() const override;
    [[nodiscard]] std::optional<EntityId> getEntityId() const override;
    
private:
    Config _config;
    // State...
};
```

**Files**:
- `Interaction/RectangleInteractionController.hpp` (new)
- `Interaction/RectangleInteractionController.cpp` (new)

### 2.2 LineInteractionController

For drawing selection lines (as in Analysis Dashboard) or creating line annotations.

```cpp
// Interaction/LineInteractionController.hpp

class LineInteractionController : public IGlyphInteractionController {
public:
    struct Config {
        float min_length{1.0f};  // Minimum line length in canvas pixels
        bool snap_to_axis{false}; // If true, constrain to horizontal/vertical
        float snap_angle_threshold{15.0f}; // Degrees from axis to snap
    };
    
    explicit LineInteractionController(Config config = {});
    
    // IGlyphInteractionController interface...
    
private:
    Config _config;
    glm::vec2 _start_point{0};
    glm::vec2 _end_point{0};
    // ...
};
```

**Files**:
- `Interaction/LineInteractionController.hpp` (new)
- `Interaction/LineInteractionController.cpp` (new)

### 2.3 PointInteractionController (Future)

For placing single points (event markers, annotation points).

### 2.4 PolygonInteractionController (Future)

For drawing freeform selection regions.

---

## Phase 3: Preview Rendering (PlottingOpenGL)

**Goal**: Add preview rendering capability to SceneRenderer.

### 3.1 PreviewRenderer

```cpp
// PlottingOpenGL/Renderers/PreviewRenderer.hpp

class PreviewRenderer {
public:
    PreviewRenderer();
    ~PreviewRenderer();
    
    [[nodiscard]] bool initialize();
    void cleanup();
    [[nodiscard]] bool isInitialized() const;
    
    /// Render preview geometry in canvas coordinates
    void render(CorePlotting::Interaction::GlyphPreview const& preview,
                int viewport_width, int viewport_height);

private:
    // Shaders for 2D screen-space rendering
    std::unique_ptr<QOpenGLShaderProgram> m_fill_shader;
    std::unique_ptr<QOpenGLShaderProgram> m_stroke_shader;
    
    // Geometry buffers
    QOpenGLVertexArrayObject m_rect_vao;
    QOpenGLBuffer m_rect_vbo;
    QOpenGLVertexArrayObject m_line_vao;
    QOpenGLBuffer m_line_vbo;
    
    bool m_initialized{false};
    
    void renderRectangle(glm::vec4 const& bounds, glm::vec4 const& fill_color,
                         glm::vec4 const& stroke_color, float stroke_width,
                         glm::mat4 const& projection);
    void renderLine(glm::vec2 const& start, glm::vec2 const& end,
                    glm::vec4 const& color, float width,
                    glm::mat4 const& projection);
};
```

**Files**:
- `PlottingOpenGL/Renderers/PreviewRenderer.hpp` (new)
- `PlottingOpenGL/Renderers/PreviewRenderer.cpp` (new)

### 3.2 Integrate into SceneRenderer

```cpp
// Additions to SceneRenderer
class SceneRenderer {
public:
    // ... existing methods ...
    
    /// Render a preview overlay after the main scene
    void renderPreview(CorePlotting::Interaction::GlyphPreview const& preview,
                       int viewport_width, int viewport_height);

private:
    PreviewRenderer m_preview_renderer;
};
```

**Files**:
- `PlottingOpenGL/SceneRenderer.hpp` (modify)
- `PlottingOpenGL/SceneRenderer.cpp` (modify)

### 3.3 Preview Shaders

Simple 2D shaders for screen-space rendering.

**Files**:
- `PlottingOpenGL/Shaders/preview_fill.vert` (new)
- `PlottingOpenGL/Shaders/preview_fill.frag` (new)
- `PlottingOpenGL/Shaders/preview_stroke.vert` (new)
- `PlottingOpenGL/Shaders/preview_stroke.frag` (new)

---

## Phase 4: Scene Coordinate Conversion

**Goal**: Add inverse coordinate transform API to RenderableScene.

### 4.1 Scene Coordinate Methods

```cpp
// Additions to RenderableScene (SceneGraph/RenderablePrimitives.hpp)

struct RenderableScene {
    // ... existing members ...
    
    /// Convert canvas coordinates to world coordinates
    [[nodiscard]] glm::vec2 canvasToWorld(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height) const;
    
    /// Convert a GlyphPreview to DataCoordinates for a specific series
    /// @param preview The preview geometry in canvas coordinates
    /// @param viewport_width/height Current viewport dimensions
    /// @param y_transform The Y-axis transform for the target series (from LayoutResponse)
    /// @return DataCoordinates ready for committing to DataManager
    [[nodiscard]] Interaction::DataCoordinates previewToDataCoords(
        Interaction::GlyphPreview const& preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform) const;
};
```

**Files**:
- `SceneGraph/RenderablePrimitives.hpp` (modify)
- `SceneGraph/RenderablePrimitives.cpp` (new - for implementations)

---

## Phase 5: Widget Integration (DataViewer)

**Goal**: Refactor OpenGLWidget to use the new interaction system.

### 5.1 Add Interaction Mode Enum

```cpp
// In OpenGLWidget.hpp or separate header
enum class InteractionMode {
    Normal,           // Default: pan, select, etc.
    CreateInterval,   // Click-drag to create new interval
    CreateLine,       // Click-drag to draw a line
    ModifyInterval,   // Edge dragging (triggered by hit test)
    // Future: CreatePoint, CreatePolygon, etc.
};
```

### 5.2 Integrate Controller

```cpp
// OpenGLWidget additions
class OpenGLWidget : public QOpenGLWidget {
    // ...
    
    // Interaction state
    InteractionMode _interaction_mode{InteractionMode::Normal};
    std::unique_ptr<IGlyphInteractionController> _active_controller;
    
    // Mode management
    void setInteractionMode(InteractionMode mode);
    [[nodiscard]] InteractionMode interactionMode() const;
    
    // Commit interaction result to DataManager
    void commitInteractionResult(Interaction::DataCoordinates const& coords);
    
signals:
    void interactionModeChanged(InteractionMode mode);
    void interactionCompleted(Interaction::DataCoordinates const& coords);
};
```

### 5.3 Refactor Mouse Event Handling

Replace the current manual state tracking (`_is_creating_new_interval`, `_interval_drag_controller`) with the unified controller system.

### 5.4 Remove Legacy Code

- Remove `drawDraggedInterval()` (replaced by PreviewRenderer)
- Remove `drawNewIntervalBeingCreated()` (replaced by PreviewRenderer)
- Remove `_is_creating_new_interval` and related state variables
- Keep `IntervalDragController` temporarily for reference, then deprecate

**Files**:
- `WhiskerToolbox/DataViewer_Widget/OpenGLWidget.hpp` (modify)
- `WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp` (modify)

---

## Phase 6: Analysis Dashboard Integration (Future)

**Goal**: Port LineSelectionHandler to use the new system.

### 6.1 Replace LineSelectionHandler

The current `LineSelectionHandler` in Analysis Dashboard can be replaced with:
- `LineInteractionController` for state management
- `PreviewRenderer` for rendering
- Widget handles mode switching and result processing

### 6.2 Add Box Selection

Use `RectangleInteractionController` with `constrain_to_x_axis = false` for rectangular selection regions.

---

## Migration Strategy

### Backward Compatibility

During migration, both old and new systems can coexist:
1. Add new controller system alongside existing code
2. Widget checks `_active_controller` first, falls back to legacy
3. Gradually migrate each interaction type
4. Remove legacy code once all migrations complete

### Testing Checkpoints

After each phase, verify:
- [ ] Existing functionality unchanged
- [ ] No performance regression
- [ ] Unit tests pass (add new tests for new code)
- [ ] Manual testing of interaction flows

---

## File Summary

### New Files (CorePlotting/Interaction)
- `IGlyphInteractionController.hpp`
- `DataCoordinates.hpp`
- `RectangleInteractionController.hpp/.cpp`
- `LineInteractionController.hpp/.cpp`

### New Files (CorePlotting/CoordinateTransform)
- `InverseTransform.hpp/.cpp`

### New Files (PlottingOpenGL)
- `Renderers/PreviewRenderer.hpp/.cpp`
- `Shaders/preview_*.vert/.frag`

### Modified Files
- `CorePlotting/Interaction/GlyphPreview.hpp` (namespace, cleanup)
- `CorePlotting/SceneGraph/RenderablePrimitives.hpp` (add coordinate methods)
- `PlottingOpenGL/SceneRenderer.hpp/.cpp` (add preview rendering)
- `WhiskerToolbox/DataViewer_Widget/OpenGLWidget.hpp/.cpp` (integration)

---

## Timeline Estimate

| Phase | Estimated Effort | Dependencies |
|-------|------------------|--------------|
| Phase 1 | 1-2 days | None |
| Phase 2 | 2-3 days | Phase 1 |
| Phase 3 | 2-3 days | Phase 1 |
| Phase 4 | 1 day | Phase 1 |
| Phase 5 | 3-4 days | Phases 1-4 |
| Phase 6 | 2-3 days | Phase 5 |

**Total**: ~2-3 weeks for full migration

---

## Open Questions

1. **Undo/Redo**: Should the interaction system integrate with an undo stack? If so, `DataCoordinates` could include the information needed to reverse the operation.

2. **Multi-select**: Should controllers support selecting/modifying multiple glyphs simultaneously? (e.g., shift-click to extend selection)

3. **Keyboard Modifiers**: Should modifier keys (Shift, Ctrl, Alt) affect controller behavior, or should the widget handle all modifier logic?

4. **Touch/Gesture Support**: Future consideration for tablet/touch interfaces.
