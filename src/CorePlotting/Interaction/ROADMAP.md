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

## Phase 1: Core Infrastructure (Foundation) ✅ COMPLETE

**Goal**: Establish the base types and interfaces without breaking existing functionality.

### 1.1 Finalize GlyphPreview ✅

- [x] Create `GlyphPreview.hpp` with basic structure
- [x] Add `IGlyphInteractionController` to separate header
- [x] Add namespace `CorePlotting::Interaction`
- [x] Add documentation comments
- [x] Add factory methods and convenience accessors

**Files**:
- `Interaction/GlyphPreview.hpp` ✅
- `Interaction/IGlyphInteractionController.hpp` ✅

### 1.2 DataCoordinates Type ✅

Created the output type for coordinate conversion results with:
- Type-specific coordinate structs (IntervalCoords, LineCoords, PointCoords, RectCoords)
- std::variant for type-safe storage
- Factory methods for common use cases
- Safe accessors (both throwing and optional-returning)

**Files**:
- `Interaction/DataCoordinates.hpp` ✅

### 1.3 InverseCoordinateTransform Utilities ✅

Added inverse coordinate transform functions:
- `canvasToNDC()` / `ndcToCanvas()` - Canvas ↔ NDC
- `ndcToWorld()` / `worldToNDC()` - NDC ↔ World (with VP matrices)
- `canvasToWorld()` / `worldToCanvas()` - Combined transforms
- `worldXToTimeIndex()` / `timeIndexToWorldX()` - X-axis conversion
- `worldYToDataY()` / `dataYToWorldY()` - Y-axis with LayoutTransform
- `canvasToData()` - Full pipeline with result struct

**Files**:
- `CoordinateTransform/InverseTransform.hpp` ✅

### 1.4 RenderableScene Preview Support ✅

Added `active_preview` field to `RenderableScene` for interactive preview rendering.

**Files**:
- `SceneGraph/RenderablePrimitives.hpp` ✅ (modified)

---

## Phase 2: Concrete Controllers ✅ COMPLETE

**Goal**: Implement reusable interaction controllers for each primitive type.

### 2.1 RectangleInteractionController ✅

Handles both interval creation and rectangle selection boxes.

Features implemented:
- **Creation mode**: Click-drag to create new rectangle
- **Edge drag mode**: Modify existing rectangle by dragging edges
- **Interval mode** (`constrain_to_x_axis = true`): Full-height rectangles for DigitalIntervalSeries
- **Selection box mode** (`constrain_to_x_axis = false`): User-defined width and height
- Ghost rendering support for modification preview
- Configurable min width/height constraints

**Files**:
- `Interaction/RectangleInteractionController.hpp` ✅
- `Interaction/RectangleInteractionController.cpp` ✅

### 2.2 LineInteractionController ✅

For drawing selection lines (as in Analysis Dashboard) or creating line annotations.

Features implemented:
- **Creation mode**: Click-drag to create new line
- **Endpoint drag mode**: Modify existing line by dragging endpoints
- **Axis snapping**: Optional snap to horizontal/vertical when near axis
- **Horizontal/vertical only** constraints
- Ghost rendering support for modification preview
- Configurable min length and snap angle threshold

**Files**:
- `Interaction/LineInteractionController.hpp` ✅
- `Interaction/LineInteractionController.cpp` ✅

### 2.3 PointInteractionController (Future)

For placing single points (event markers, annotation points).

### 2.4 PolygonInteractionController (Future)

For drawing freeform selection regions.

---

## Phase 3: Preview Rendering (PlottingOpenGL) ✅ COMPLETE

**Goal**: Add preview rendering capability to SceneRenderer.

### 3.1 PreviewRenderer ✅

Implemented screen-space renderer for interactive preview geometry:
- Renders `GlyphPreview` directly in canvas/pixel coordinates using orthographic projection
- Supports all preview types: Rectangle, Line, Point, Polygon
- Renders "ghost" of original geometry when modifying existing elements
- Alpha blending for semi-transparent previews
- Configurable fill color, stroke color, and stroke width
- Embedded shaders (no external shader files needed)

**Files**:
- `PlottingOpenGL/Renderers/PreviewRenderer.hpp` ✅
- `PlottingOpenGL/Renderers/PreviewRenderer.cpp` ✅

### 3.2 Integrate into SceneRenderer ✅

Added preview rendering capability to SceneRenderer:
- `PreviewRenderer` member initialized/cleaned up with other renderers
- `renderPreview(GlyphPreview const&, int viewport_width, int viewport_height)` method
- Access via `previewRenderer()` for fine-grained control

**Files**:
- `PlottingOpenGL/SceneRenderer.hpp` ✅ (modified)
- `PlottingOpenGL/SceneRenderer.cpp` ✅ (modified)
- `PlottingOpenGL/CMakeLists.txt` ✅ (modified)

### 3.3 Preview Shaders ✅

Shaders embedded in PreviewRenderer.hpp (PreviewShaders namespace):
- Simple vertex shader transforms 2D positions via orthographic matrix
- Simple fragment shader applies uniform color
- Same shaders used for fill (triangles) and stroke (lines)

---

## Phase 4: Scene Coordinate Conversion ✅ COMPLETE

**Goal**: Add inverse coordinate transform API to RenderableScene.

### 4.1 Scene Coordinate Methods ✅

Implemented coordinate conversion methods on RenderableScene:

- `canvasToWorld(canvas_x, canvas_y, viewport_width, viewport_height)` - Convert canvas pixels to world coordinates using the scene's view/projection matrices

- `previewToDataCoords(preview, viewport_width, viewport_height, y_transform, series_key, entity_id)` - Main conversion method for interactive operations. Converts GlyphPreview to DataCoordinates suitable for DataManager

- `previewToIntervalCoords(rect_preview, viewport_width, viewport_height)` - Specialized conversion for intervals (only uses X coordinates)

- `previewToLineCoords(line_preview, viewport_width, viewport_height, y_transform)` - Converts line endpoints to data space

- `previewToPointCoords(point_preview, viewport_width, viewport_height, y_transform)` - Converts single point to data space

**Files**:
- `SceneGraph/RenderablePrimitives.hpp` ✅ (modified)
- `SceneGraph/RenderablePrimitives.cpp` ✅ (new)
- `Interaction/DataCoordinates.hpp` ✅ (added `<cmath>` include)
- `CorePlotting/CMakeLists.txt` ✅ (added RenderablePrimitives.cpp)

**Usage Example**:
```cpp
// After interaction completes:
auto preview = controller.getPreview();
auto y_transform = layout_response.getSeriesTransform(series_key);
auto data_coords = scene.previewToDataCoords(
    preview, width(), height(), y_transform, series_key, entity_id);
commitToDataManager(data_coords);
```

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
