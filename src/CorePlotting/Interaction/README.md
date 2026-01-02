# CorePlotting Interaction System

This module provides a unified system for interactive glyph creation and modification across all plotting widgets.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Widget Layer                                    │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │  OpenGLWidget                                                           ││
│  │  • Mode state (Normal, CreateInterval, CreateLine, etc.)                ││
│  │  • Routes mouse events to active controller                             ││
│  │  • On complete: receives DataCoordinates, updates DataManager           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
┌───────────────────────────────────┐  ┌──────────────────────────────────────┐
│  CorePlotting::Interaction        │  │  CorePlotting::RenderableScene       │
│  ┌─────────────────────────────┐  │  │  ┌────────────────────────────────┐  │
│  │ IGlyphInteractionController │  │  │  │ Existing batches               │  │
│  │ • Works in canvas coords    │  │  │  │ • poly_line_batches            │  │
│  │ • Tracks geometric state    │  │  │  │ • rectangle_batches            │  │
│  │ • Produces GlyphPreview     │──┼──▶  │ • glyph_batches                │  │
│  └─────────────────────────────┘  │  │  ├────────────────────────────────┤  │
│  ┌─────────────────────────────┐  │  │  │ Preview layer                  │  │
│  │ Concrete Controllers:       │  │  │  │ • Rendered on top of scene     │  │
│  │ • RectangleController       │  │  │  │ • Canvas coordinates           │  │
│  │ • LineController            │  │  │  └────────────────────────────────┘  │
│  │ • PointController           │  │  │  ┌────────────────────────────────┐  │
│  │ • PolygonController         │  │  │  │ Inverse Coordinate API         │  │
│  └─────────────────────────────┘  │  │  │ • canvasToWorld(x, y)          │  │
└───────────────────────────────────┘  │  │ • previewToDataCoords(...)     │  │
                                       │  └────────────────────────────────┘  │
                                       └──────────────────────────────────────┘
                                                        │
                                                        ▼
                                       ┌──────────────────────────────────────┐
                                       │  PlottingOpenGL::SceneRenderer       │
                                       │  • Renders main batches              │
                                       │  • Renders preview layer on top      │
                                       └──────────────────────────────────────┘
```

## Key Design Decisions

1. **Canvas Coordinates**: Controllers work entirely in canvas (pixel) coordinates. The Scene handles all coordinate transformations via inverse mappers.

2. **Widget-Driven Mode**: The widget manages interaction modes (Normal, CreateInterval, etc.) and pre-processes hit test results for modification operations.

3. **Layout at Conversion Time**: Series layout transforms are passed to `previewToDataCoords()` at conversion time, keeping the Scene stateless.

4. **Screen-Space Preview Rendering**: `PreviewRenderer` handles canvas→NDC conversion internally using orthographic projection.

## Files

| File | Description |
|------|-------------|
| `GlyphPreview.hpp` | Preview geometry struct (canvas coordinates) |
| `IGlyphInteractionController.hpp` | Abstract controller interface |
| `RectangleInteractionController.hpp` | Intervals and selection boxes |
| `LineInteractionController.hpp` | Line drawing |
| `DataCoordinates.hpp` | Output type for data-space coordinates |
| `HitTestResult.hpp` | Hit testing result type |
| `SceneHitTester.hpp` | Hit testing utilities |

## See Also

- [ROADMAP.md](ROADMAP.md) - Detailed implementation plan
- [../DESIGN.md](../DESIGN.md) - Overall CorePlotting architecture

## Usage Example

```cpp
// 1. Widget enters interaction mode
void OpenGLWidget::setInteractionMode(InteractionMode mode) {
    _interaction_mode = mode;
    if (mode == InteractionMode::CreateInterval) {
        _active_controller = std::make_unique<RectangleInteractionController>();
    }
}

// 2. Mouse press starts interaction (canvas coordinates)
void OpenGLWidget::mousePressEvent(QMouseEvent* event) {
    if (_active_controller && _interaction_mode != InteractionMode::Normal) {
        _active_controller->start(event->pos().x(), event->pos().y(),
                                  findTargetSeries(), std::nullopt);
        update();
    }
}

// 3. Mouse move updates preview
void OpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    if (_active_controller && _active_controller->isActive()) {
        _active_controller->update(event->pos().x(), event->pos().y());
        update();
    }
}

// 4. Paint renders preview
void OpenGLWidget::paintGL() {
    _scene_renderer->render();  // Main scene
    
    if (_active_controller && _active_controller->isActive()) {
        _scene_renderer->renderPreview(_active_controller->getPreview(),
                                       width(), height());
    }
}

// 5. Mouse release completes and commits
void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (_active_controller && _active_controller->isActive()) {
        _active_controller->complete();
        
        // Scene converts canvas → data coordinates
        auto data_coords = _cached_scene.previewToDataCoords(
            _active_controller->getPreview(),
            width(), height(),
            _cached_layout_response.findLayout(_active_controller->getSeriesKey()));
        
        // Commit to DataManager
        commitToDataManager(data_coords);
        
        _active_controller.reset();
        _interaction_mode = InteractionMode::Normal;
    }
}
```