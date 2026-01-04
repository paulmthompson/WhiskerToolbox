# Selection Handlers Refactoring Roadmap

## Overview

This document describes the incremental migration of Analysis Dashboard selection handlers to use the CorePlotting interaction system and PlottingOpenGL rendering infrastructure.

### Goals

1. **Remove ~500 lines of duplicated OpenGL code** from individual handlers
2. **Unify interfaces** via `IGlyphInteractionController`
3. **Separate concerns** - Controllers handle state, PreviewRenderer handles rendering
4. **Enable code reuse** across DataViewer, Analysis Dashboard, and future widgets
5. **Improve testability** - Controllers are Qt-independent

### Current Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Selection Handlers (Current)                  │
├─────────────────────────────────────────────────────────────────┤
│  LineSelectionHandler                                            │
│  ├── Own VAO/VBO management                                      │
│  ├── Own shader loading (via ShaderManager)                      │
│  ├── State tracking (_is_drawing_line)                          │
│  ├── World + Screen coordinate storage                          │
│  └── render() with direct GL calls                              │
├─────────────────────────────────────────────────────────────────┤
│  PolygonSelectionHandler                                         │
│  ├── Own VAO/VBO management (vertex + line buffers)             │
│  ├── Own shader loading                                          │
│  ├── State tracking (_is_polygon_selecting)                     │
│  └── render() with direct GL calls                              │
├─────────────────────────────────────────────────────────────────┤
│  PointSelectionHandler                                           │
│  └── Minimal (no rendering, just stores position)               │
├─────────────────────────────────────────────────────────────────┤
│  NoneSelectionHandler                                            │
│  └── No-op implementation                                        │
└─────────────────────────────────────────────────────────────────┘
```

### Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                  CorePlotting (Qt-Independent)                   │
├─────────────────────────────────────────────────────────────────┤
│  IGlyphInteractionController                                     │
│  ├── start(canvas_x, canvas_y, series_key)                      │
│  ├── update(canvas_x, canvas_y)                                 │
│  ├── complete() / cancel()                                       │
│  ├── isActive()                                                  │
│  └── getPreview() → GlyphPreview                                │
├─────────────────────────────────────────────────────────────────┤
│  Concrete Controllers:                                           │
│  ├── LineInteractionController                                   │
│  ├── PolygonInteractionController                               │
│  └── RectangleInteractionController                             │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                PlottingOpenGL (Shared Rendering)                 │
├─────────────────────────────────────────────────────────────────┤
│  PreviewRenderer                                                 │
│  ├── initialize() - Creates VAO/VBO/shaders once                │
│  ├── render(GlyphPreview, width, height)                        │
│  └── Handles all preview types (rect, line, polygon, point)     │
└─────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Infrastructure Setup

**Objective:** Add CorePlotting/PlottingOpenGL preview infrastructure to Analysis Dashboard widgets without changing existing functionality.

### 1.1 Add PreviewRenderer to SpatialOverlayWidget

> **STATUS: COMPLETED** - PreviewRenderer added to `BasePlotOpenGLWidget` (better location since all plot widgets inherit from it)

**Files modified:**
- `BasePlotOpenGLWidget.hpp`
- `BasePlotOpenGLWidget.cpp`

**Changes:**
```cpp
// Header additions
#include "PlottingOpenGL/Renderers/PreviewRenderer.hpp"

class BasePlotOpenGLWidget {
protected:
    // Preview rendering for selection handlers (Phase 1 of refactoring roadmap)
    PlottingOpenGL::PreviewRenderer _preview_renderer;
};
```

```cpp
// In initializeGL():
if (!_preview_renderer.initialize()) {
    qWarning() << "BasePlotOpenGLWidget: Failed to initialize PreviewRenderer";
}

// In destructor:
if (_opengl_resources_initialized && context() && context()->isValid()) {
    makeCurrent();
    _preview_renderer.cleanup();
    doneCurrent();
}
```

### 1.2 Verify PreviewRenderer Works

> **STATUS: READY FOR TESTING** - Test code added behind `#define PREVIEW_RENDERER_TEST_ENABLED`

**Test:** Uncomment `#define PREVIEW_RENDERER_TEST_ENABLED` at the top of `BasePlotOpenGLWidget.cpp` to enable test rendering.

```cpp
// In BasePlotOpenGLWidget.cpp (top of file):
// Uncomment this line to enable test rendering:
// #define PREVIEW_RENDERER_TEST_ENABLED

// In renderOverlays():
#ifdef PREVIEW_RENDERER_TEST_ENABLED
    if (_preview_renderer.isInitialized()) {
        CorePlotting::Interaction::GlyphPreview test_preview;
        test_preview.type = CorePlotting::Interaction::GlyphPreview::Type::Line;
        test_preview.line_start = {100.0f, 100.0f};
        test_preview.line_end = {300.0f, 200.0f};
        test_preview.stroke_color = {1.0f, 0.0f, 0.0f, 1.0f};  // Red line
        test_preview.stroke_width = 3.0f;
        _preview_renderer.render(test_preview, width(), height());
    }
#endif
```

**Acceptance Criteria:**
- [x] PreviewRenderer initializes without errors
- [x] Test line renders correctly in screen coordinates (verified 2026-01-04)
- [x] No interference with existing selection handler rendering

---

## Phase 2: LineSelectionHandler Migration

**Objective:** Replace `LineSelectionHandler` internals with `LineInteractionController` while maintaining the same external behavior.

> **STATUS: COMPLETED** (2026-01-04)

**Changes Made:**
- Added `LineInteractionController` member to `LineSelectionHandler`
- Removed all OpenGL resources (VAO, VBO, shader program)
- Removed `QOpenGLFunctions_4_1_Core` inheritance
- Replaced `render(mvp_matrix)` with `getPreview()` returning `GlyphPreview`
- Added `isActive()` method delegating to controller
- Updated `BasePlotOpenGLWidget::renderOverlays()` to use `PreviewRenderer` for `LineSelectionHandler`
- Updated mouse event handlers to pass screen coordinates to controller

### 2.1 Add LineInteractionController

**Files to modify:**
- `LineSelectionHandler.hpp`
- `LineSelectionHandler.cpp`

**Strategy:** Composition - keep `LineSelectionHandler` as a facade but delegate to `LineInteractionController`.

```cpp
// LineSelectionHandler.hpp additions
#include "CorePlotting/Interaction/LineInteractionController.hpp"

class LineSelectionHandler : protected QOpenGLFunctions_4_1_Core {
private:
    // NEW: CorePlotting controller
    CorePlotting::Interaction::LineInteractionController _controller;
    
    // DEPRECATED (remove in Phase 2.3):
    // QOpenGLBuffer _line_vertex_buffer;
    // QOpenGLVertexArrayObject _line_vertex_array_object;
    // bool _is_drawing_line;
    // Point2D<float> _line_start_point_world;
    // Point2D<float> _line_end_point_world;
};
```

### 2.2 Delegate to Controller

**API Mapping:**

| Old Method | New Implementation |
|------------|-------------------|
| `startLineSelection(world_x, world_y)` | Store world coords, call `_controller.start(canvas_x, canvas_y, "line")` |
| `updateLineEndPoint(world_x, world_y)` | `_controller.update(canvas_x, canvas_y)` |
| `completeLineSelection()` | `_controller.complete()` |
| `cancelLineSelection()` | `_controller.cancel()` |
| `_is_drawing_line` | `_controller.isActive()` |

**Note:** The controller works in canvas coordinates. You'll need to track the widget reference or pass canvas coords from mouse events.

### 2.3 Replace render() Implementation

**Before:**
```cpp
void LineSelectionHandler::render(QMatrix4x4 const & mvp_matrix) {
    // 50+ lines of OpenGL code
    _line_shader_program->bind();
    _line_vertex_array_object.bind();
    glDrawArrays(GL_LINES, 0, 2);
    // ...
}
```

**After:**
```cpp
CorePlotting::Interaction::GlyphPreview LineSelectionHandler::getPreview() const {
    return _controller.getPreview();
}

// Rendering moves to widget:
// _preview_renderer.render(_line_handler->getPreview(), width(), height());
```

### 2.4 Remove Deprecated OpenGL Code

Once migration is validated:
- [x] Remove `_line_vertex_buffer`, `_line_vertex_array_object`
- [x] Remove `initializeOpenGLResources()` / `cleanupOpenGLResources()`
- [x] Remove `updateLineBuffer()`
- [x] Remove direct `glDrawArrays()` calls
- [x] Remove shader program pointer `_line_shader_program`

### 2.5 Update LineSelectionRegion Creation

**Current:** Creates `LineSelectionRegion` from stored world coordinates

**New:** Use controller's preview and optionally `RenderableScene::previewToDataCoords()`:

```cpp
void LineSelectionHandler::completeLineSelection() {
    _controller.complete();
    
    auto preview = _controller.getPreview();
    // Convert preview.line_start/line_end from canvas to world
    // (widget provides conversion or we store world coords separately)
    
    auto line_region = std::make_unique<LineSelectionRegion>(
        _line_start_point_world, _line_end_point_world);
    // ...
}
```

**Acceptance Criteria:**
- [x] Line selection works identically to before (verified 2026-01-04)
- [x] Line renders via PreviewRenderer
- [x] OpenGL resource cleanup successful (no leaks)
- [x] All existing tests pass

---

## Phase 3: PolygonSelectionHandler Migration

**Objective:** Replace `PolygonSelectionHandler` with `PolygonInteractionController`.

> **STATUS: COMPLETED** (2026-01-04)

**Changes Made:**
- Added `PolygonInteractionController` member to `PolygonSelectionHandler`
- Removed all OpenGL resources (VAO, VBO, shader program)
- Removed `QOpenGLFunctions_4_1_Core` inheritance
- Replaced `render(mvp_matrix)` with `getPreview()` returning `GlyphPreview`
- Added `isActive()` method delegating to controller
- Added `mouseMoveEvent()` to update cursor position for preview line
- Updated `BasePlotOpenGLWidget::renderOverlays()` to use `PreviewRenderer` for `PolygonSelectionHandler`
- Updated mouse event handlers to pass screen coordinates to controller
- Supports click-near-first-vertex auto-close feature

### 3.1 Add PolygonInteractionController

```cpp
// PolygonSelectionHandler.hpp
#include "CorePlotting/Interaction/PolygonInteractionController.hpp"

class PolygonSelectionHandler {
private:
    CorePlotting::Interaction::PolygonInteractionController _controller;
};
```

### 3.2 Delegate to Controller

**API Mapping:**

| Old Method | New Implementation |
|------------|-------------------|
| `startPolygonSelection(x, y)` | `_controller.start(canvas_x, canvas_y, "polygon")` |
| `addPolygonVertex(x, y)` | `_controller.addVertex(canvas_x, canvas_y)` |
| `completePolygonSelection()` | `_controller.complete()` |
| `cancelPolygonSelection()` | `_controller.cancel()` |
| `_is_polygon_selecting` | `_controller.isActive()` |
| `_polygon_vertices` | `_controller.getVertices()` |

### 3.3 Handle Cursor Position Preview

`PolygonInteractionController` supports showing a preview line from the last vertex to current cursor:

```cpp
void PolygonSelectionHandler::mouseMoveEvent(QMouseEvent* event, ...) {
    if (_controller.isActive()) {
        _controller.updateCursorPosition(event->pos().x(), event->pos().y());
    }
}
```

### 3.4 Replace render() and Remove OpenGL Code

Same pattern as LineSelectionHandler:
- [x] Remove `_polygon_vertex_buffer`, `_polygon_line_buffer`
- [x] Remove `_polygon_vertex_array_object`, `_polygon_line_array_object`
- [x] Remove `updatePolygonBuffers()`
- [x] Widget calls `_preview_renderer.render(_polygon_handler->getPreview(), ...)`

**Acceptance Criteria:**
- [x] Polygon selection works identically (click-to-add-vertex)
- [x] Vertices render as points, edges render as lines
- [x] Closure line renders in different color
- [x] Enter/Return completes polygon
- [x] Escape cancels polygon

---

## Phase 4: Unify Handler Interface

**Objective:** Create a common interface for all selection handlers that mirrors `IGlyphInteractionController`.

> **STATUS: COMPLETED** (2026-01-04)

**Changes Made:**
- Created `ISelectionHandler` interface with unified API for all selection handlers
- Updated `LineSelectionHandler`, `PolygonSelectionHandler`, `PointSelectionHandler`, `NoneSelectionHandler` to implement `ISelectionHandler`
- Replaced `SelectionVariant` (std::variant) with `SelectionHandler` (unique_ptr<ISelectionHandler>)
- Removed all `std::visit` calls from widget code - now uses direct method calls
- Updated visualizer `applySelection()` methods to use `dynamic_cast` instead of `std::holds_alternative`

### 4.1 Define ISelectionHandler Interface

```cpp
// ISelectionHandler.hpp (new file)
#ifndef ISELECTIONHANDLER_HPP
#define ISELECTIONHANDLER_HPP

#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "SelectionModes.hpp"

#include <QVector2D>
#include <functional>
#include <memory>

class QMouseEvent;
class QKeyEvent;

class ISelectionHandler {
public:
    using NotificationCallback = std::function<void()>;
    
    virtual ~ISelectionHandler() = default;
    
    virtual void setNotificationCallback(NotificationCallback callback) = 0;
    virtual void clearNotificationCallback() = 0;
    
    // Event handling
    virtual void mousePressEvent(QMouseEvent* event, QVector2D const& world_pos) = 0;
    virtual void mouseMoveEvent(QMouseEvent* event, QVector2D const& world_pos) = 0;
    virtual void mouseReleaseEvent(QMouseEvent* event, QVector2D const& world_pos) = 0;
    virtual void keyPressEvent(QKeyEvent* event) = 0;
    
    // State
    virtual bool isActive() const = 0;
    virtual void deactivate() = 0;
    
    // Preview for rendering (new!)
    virtual CorePlotting::Interaction::GlyphPreview getPreview() const = 0;
    
    // Result
    virtual std::unique_ptr<SelectionRegion> const& getActiveSelectionRegion() const = 0;
};

#endif // ISELECTIONHANDLER_HPP
```

### 4.2 Update Handlers to Implement Interface

- `LineSelectionHandler : public ISelectionHandler`
- `PolygonSelectionHandler : public ISelectionHandler`
- `PointSelectionHandler : public ISelectionHandler`
- `NoneSelectionHandler : public ISelectionHandler`

### 4.3 Replace SelectionVariant

```cpp
// Before:
using SelectionVariant = std::variant<
    std::unique_ptr<LineSelectionHandler>,
    std::unique_ptr<NoneSelectionHandler>,
    std::unique_ptr<PolygonSelectionHandler>,
    std::unique_ptr<PointSelectionHandler>>;

// After:
using SelectionHandler = std::unique_ptr<ISelectionHandler>;
```

**Acceptance Criteria:**
- [x] All handlers implement `ISelectionHandler`
- [x] Widget code simplified (no more `std::visit` needed)
- [x] `getPreview()` works for all handler types

---

## Phase 5: Widget Integration Cleanup

**Objective:** Simplify widget code now that handlers have unified interface.

### 5.1 Centralize Preview Rendering

```cpp
void SpatialOverlayWidget::paintGL() {
    // ... scene rendering ...
    
    // Unified preview rendering
    if (_selection_handler && _selection_handler->isActive()) {
        auto preview = _selection_handler->getPreview();
        if (preview.isValid()) {
            _preview_renderer.render(preview, width(), height());
        }
    }
}
```

### 5.2 Simplify Event Routing

```cpp
void SpatialOverlayWidget::mousePressEvent(QMouseEvent* event) {
    QVector2D world_pos = screenToWorld(event->pos());
    _selection_handler->mousePressEvent(event, world_pos);
    update();
}
```

### 5.3 Remove Handler-Specific render() Calls

The individual `handler->render(mvp_matrix)` calls are replaced by the unified preview rendering.

---

## Phase 6: Consider DataCoordinates Integration (Optional)

**Objective:** Use CorePlotting's `DataCoordinates` for type-safe coordinate handling.

### 6.1 Replace SelectionRegion with DataCoordinates

If `SelectionRegion` is only used for coordinate storage, consider using:

```cpp
// Instead of:
auto line_region = std::make_unique<LineSelectionRegion>(start, end);

// Use:
auto data_coords = CorePlotting::Interaction::DataCoordinates::createLine(
    "selection", start.x, start.y, end.x, end.y);
```

### 6.2 Integrate with RenderableScene Coordinate Conversion

If your widget uses `RenderableScene`, leverage its `previewToDataCoords()`:

```cpp
void onSelectionComplete() {
    auto preview = _selection_handler->getPreview();
    auto data_coords = _scene.previewToDataCoords(
        preview, width(), height(), y_transform, series_key);
    
    if (data_coords.isLine()) {
        auto const& line = data_coords.asLine();
        // Use line.x1, line.y1, line.x2, line.y2
    }
}
```

---

## Migration Checklist

### Phase 1: Infrastructure
- [x] Add `PreviewRenderer` to widget (added to `BasePlotOpenGLWidget`)
- [x] Verify test preview renders correctly ✓

### Phase 2: LineSelectionHandler
- [x] Add `LineInteractionController` member
- [x] Delegate state management to controller
- [x] Replace `render()` with `getPreview()`
- [x] Remove OpenGL resources
- [x] Verify functionality unchanged ✓ (2026-01-04)

### Phase 3: PolygonSelectionHandler
- [x] Add `PolygonInteractionController` member
- [x] Delegate state management to controller
- [x] Handle cursor position preview
- [x] Replace `render()` with `getPreview()`
- [x] Remove OpenGL resources
- [x] Verify functionality unchanged ✓ (2026-01-04)

### Phase 4: Interface Unification
- [x] Create `ISelectionHandler` interface
- [x] Update all handlers to implement interface
- [x] Replace `SelectionVariant` with `SelectionHandler`

### Phase 5: Widget Cleanup
- [ ] Centralize preview rendering
- [ ] Simplify event routing
- [ ] Remove handler-specific render calls

### Phase 6: DataCoordinates (Optional)
- [ ] Evaluate if `SelectionRegion` can be replaced
- [ ] Integrate with `RenderableScene` if applicable

---

## Risk Mitigation

### Backward Compatibility
Each phase can be completed independently. If issues arise:
1. The old code path can be restored by removing the controller delegation
2. Feature flags can gate new vs old behavior during testing

### Testing Strategy
1. **Manual testing** after each phase - verify visual appearance unchanged
2. **Regression testing** - ensure existing selection workflows work
3. **Memory testing** - verify no OpenGL resource leaks after cleanup

### Rollback Plan
Git branches for each phase allow easy rollback:
```bash
git checkout -b feature/selection-phase-1
git checkout -b feature/selection-phase-2
# etc.
```

---

## Timeline Estimate

| Phase | Effort | Dependencies |
|-------|--------|--------------|
| Phase 1 | 1-2 hours | None |
| Phase 2 | 3-4 hours | Phase 1 |
| Phase 3 | 3-4 hours | Phase 1 |
| Phase 4 | 2-3 hours | Phase 2, 3 |
| Phase 5 | 1-2 hours | Phase 4 |
| Phase 6 | 2-4 hours | Phase 5 (optional) |

**Total: ~12-19 hours** (excluding Phase 6)

---

## References

- [CorePlotting/DESIGN.md](../../CorePlotting/DESIGN.md) - Architecture overview
- [CorePlotting/Interaction/ROADMAP.md](../../CorePlotting/Interaction/ROADMAP.md) - Interaction system design
- [PlottingOpenGL/Renderers/PreviewRenderer.hpp](../../PlottingOpenGL/Renderers/PreviewRenderer.hpp) - Preview rendering API
