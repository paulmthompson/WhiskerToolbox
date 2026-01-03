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

