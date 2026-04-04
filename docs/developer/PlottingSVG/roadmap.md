# PlottingSVG Implementation Roadmap

Step-by-step plan for building the `PlottingSVG` library and integrating SVG export into plot widgets.

## Phase 1: Create PlottingSVG Library Foundation

### Steps 1.1–1.6: Core Library (Completed)

All foundational components are implemented and verified:

| Step | Component | Summary |
|------|-----------|---------|
| 1.1 | Directory + CMake | Static `PlottingSVG` target linking `CorePlotting` and `glm::glm` (no Qt/OpenGL). Registered in `src/CMakeLists.txt` outside `if (ENABLE_UI)`. |
| 1.2 | `SVGUtils` | `transformVertexToSVG()` (MVP→canvas pixels) and `colorToSVGHex()` (RGBA→`#RRGGBB`). |
| 1.3 | Per-batch renderers | `SVGPolyLineRenderer`, `SVGGlyphRenderer`, `SVGRectangleRenderer` — stateless `render()` methods producing SVG element strings. Legacy `SVGExport.hpp/.cpp` provides flat `buildSVGDocument` / `createScalebarSVG` for DataViewer. `CorePlotting/Export/SVGPrimitives` removed. |
| 1.4 | `SVGDocument` | UTF-8 SVG 1.1 assembly with `<?xml>`, `<svg>` viewBox, background `<rect>`, `<desc>`, and ordered `<g id="…">` layers. |
| 1.5 | `SVGSceneRenderer` | Main entry point: binds `RenderableScene`, iterates batches (rectangles→polylines→glyphs) into named layers, appends decorations, delegates to `SVGDocument::build()`. Includes `renderToFile()`. |
| 1.6 | Decorations | `SVGDecoration` interface; `SVGScalebar` (bar + ticks + label, configurable corner/padding/style); `SVGAxisRenderer` (spine + uniform ticks + title, Bottom/Left positions). |

---

### Step 1.7: Write tests

Create `tests/PlottingSVG/` with a `CMakeLists.txt` and test files.

**Test files:**
- `SVGUtils.test.cpp` — coordinate transform and color conversion
- `SVGGlyphRenderer.test.cpp` — each glyph type produces expected SVG elements
- `SVGPolyLineRenderer.test.cpp` — polyline points and styling
- `SVGRectangleRenderer.test.cpp` — rectangle coordinates after MVP
- `SVGDocument.test.cpp` — document structure: XML header, viewBox, background, layers
- `SVGSceneRenderer.test.cpp` — full scene rendering with mixed batch types, correct layer ordering
- `SVGScalebar.test.cpp` — scalebar element positions and label

**Test strategy:**
- Build known `RenderableScene` objects with predetermined geometry
- Use identity or simple orthographic MVP matrices so expected SVG coordinates are easy to calculate
- Verify SVG output contains expected substrings (element tags, attribute values)
- Verify document structure (starts with `<?xml`, contains `<svg`, ends with `</svg>`)

**Files to create:**
- `tests/PlottingSVG/CMakeLists.txt`
- `tests/PlottingSVG/SVGUtils.test.cpp`
- `tests/PlottingSVG/SVGGlyphRenderer.test.cpp`
- `tests/PlottingSVG/SVGPolyLineRenderer.test.cpp`
- `tests/PlottingSVG/SVGRectangleRenderer.test.cpp`
- `tests/PlottingSVG/SVGDocument.test.cpp`
- `tests/PlottingSVG/SVGSceneRenderer.test.cpp`
- `tests/PlottingSVG/SVGScalebar.test.cpp`

**Files to modify:**
- `tests/CMakeLists.txt` — add `add_subdirectory(PlottingSVG)`

**Verify:** `ctest --preset linux-clang-release -R "PlottingSVG"` — all tests pass.

---

### Step 1.8: Update CorePlotting — remove Export/SVGPrimitives

### Step 1.8: Remove SVGPrimitives from CorePlotting (Completed)

`CorePlotting/Export/SVGPrimitives.hpp/.cpp` removed outright — no forwarding header needed. `DataViewer_Widget` links `PlottingSVG` and includes `PlottingSVG/SVGExport.hpp` directly.

---

## Phase 2: EventPlot SVG Export

### Step 2.1: Add `exportToSVG()` to EventPlotOpenGLWidget

The widget already caches `_scene` (`RenderableScene`), `_view_matrix`, and `_projection_matrix`. Add a method that creates a `PlottingSVG::SVGSceneRenderer`, sets the cached scene, and returns the SVG string.

```cpp
[[nodiscard]] QString exportToSVG() const;
```

**Implementation details:**
- No scene rebuild — exports the current cached scene (what the user sees on screen)
- Sets canvas size to 1920×1080 (or configurable)
- Sets background color from `_state->getBackgroundColor()`
- Optionally add a center-line decoration at t=0 (vertical line through the raster plot)
- Optionally add axis decorations from state axis options

**Files to modify:**
- `src/WhiskerToolbox/Plots/EventPlotWidget/Rendering/EventPlotOpenGLWidget.hpp` — add `exportToSVG()` declaration
- `src/WhiskerToolbox/Plots/EventPlotWidget/Rendering/EventPlotOpenGLWidget.cpp` — implement `exportToSVG()`
- `src/WhiskerToolbox/Plots/EventPlotWidget/CMakeLists.txt` — add `PlottingSVG` to `target_link_libraries`

**Verify:** Build compiles. Calling `exportToSVG()` on a widget with loaded data returns a non-empty SVG string.

---

### Step 2.2: Add "Export SVG" button to EventPlotPropertiesWidget

Add an "Export" section to the properties panel with:
- "Export SVG" QPushButton

When clicked, emit a signal:
```cpp
signals:
    void exportSVGRequested();
```

**Files to modify:**
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.hpp` — add signal declaration
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.cpp` — add button and connect click to signal
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.ui` — add button to UI layout (or add programmatically)

**Verify:** Build compiles. Button appears in properties panel.

---

### Step 2.3: Add `handleExportSVG()` to EventPlotWidget

Add a file dialog + write method to the top-level EventPlotWidget, following the pattern in `DataViewer_Widget::exportToSVG()`:

```cpp
void EventPlotWidget::handleExportSVG() {
    QString fileName = AppFileDialog::getSaveFileName(
        this, "export_event_plot_svg", tr("Export Event Plot to SVG"),
        tr("SVG Files (*.svg);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QString svg = _gl_widget->exportToSVG();
    QFile file(fileName);
    // ... write and show success/error message
}
```

**Files to modify:**
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotWidget.hpp` — add `handleExportSVG()` slot
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotWidget.cpp` — implement `handleExportSVG()`

**Verify:** Build compiles.

---

### Step 2.4: Wire signals in EventPlotWidgetRegistration

Connect the properties widget signal to the view widget slot:

```cpp
QObject::connect(props, &EventPlotPropertiesWidget::exportSVGRequested,
                 view, &EventPlotWidget::handleExportSVG);
```

**Source pattern:** `src/WhiskerToolbox/DataViewer_Widget/DataViewerWidgetRegistration.cpp` lines 106-108

**Files to modify:**
- `src/WhiskerToolbox/Plots/EventPlotWidget/EventPlotWidgetRegistration.cpp`

**Verify:** Full build succeeds. Clicking "Export SVG" in EventPlot properties opens file dialog and produces valid SVG file.

---

### Step 2.5: Manual validation

1. Load a dataset with `DigitalEventSeries` data
2. Open an EventPlot widget, configure alignment and display options
3. Click "Export SVG" in the properties panel
4. Open the resulting SVG in a web browser and/or Inkscape
5. Verify:
   - Glyph positions match the on-screen raster plot
   - Colors and glyph types (tick, circle, square) are correct
   - Background color matches
   - Center line at t=0 is present (if enabled)
   - Multiple overlaid event series are rendered with correct per-series colors

---

## Phase 3: Refactor DataViewer SVGExporter (Completed)

`DataViewer_Widget` links `PlottingSVG` and uses `PlottingSVG::buildSVGDocument` / `createScalebarSVG` from `SVGExport.hpp` (legacy flat API). `CorePlotting/Export/SVGPrimitives` removed entirely — no forwarding header. Optional future migration to `SVGSceneRenderer`-only path.

---

## Phase 4: Shared Export Widget (Optional — defer until 3rd widget)

### Step 4.1: Create Plots/Common/ExportWidget/

Create a reusable Qt widget containing:
- "Export SVG" QPushButton
- Canvas width/height spin boxes (default 1920×1080)
- "Include scalebar" checkbox + length spin box
- "Include axes" checkbox

Emits:
```cpp
signals:
    void exportRequested(ExportConfig const & config);
```

Where `ExportConfig` is a plain struct with canvas size, scalebar settings, axis settings.

**Files to create:**
- `src/WhiskerToolbox/Plots/Common/ExportWidget/ExportWidget.hpp`
- `src/WhiskerToolbox/Plots/Common/ExportWidget/ExportWidget.cpp`

**Trigger:** Implement when a third widget (PSTHWidget or LinePlotWidget) needs SVG export. Until then, per-widget export buttons are simple enough.

---

## Phase 5: Extend to Additional Widgets

Once PlottingSVG is proven with EventPlot and DataViewer, extend to other widgets. Priority order:

| Widget | Complexity | Notes |
|--------|-----------|-------|
| PSTHWidget | Low | Already produces `RenderableScene` — same pattern as EventPlot |
| LinePlotWidget | Medium | Trial-aligned lines + summary statistics |
| OnionSkinViewWidget | Medium | Spatial overlay with alpha fading |
| TemporalProjectionViewWidget | Medium | Full spatial overlay |
| HeatmapWidget | High | Needs `SVGHeatmapRenderer` for cell grids — new batch type or `<rect>` grid |
| ScatterPlotWidget | Medium | Glyph-based — straightforward |
| SpectrogramWidget | High | Raster image data — may need SVG `<image>` element |

For each widget, the pattern is identical to Phase 2:
1. Add `exportToSVG()` method to the OpenGL widget
2. Add export button to properties widget
3. Wire signal in registration

---

## Summary Checklist

- [x] **Phase 1.1–1.6:** Core library foundation (directory, CMake, SVGUtils, renderers, SVGDocument, SVGSceneRenderer, decorations)
- [x] **Phase 1.7:** Tests (all renderer classes + document + scene)
- [x] **Phase 1.8:** Remove SVGPrimitives from CorePlotting
- [x] **Phase 2.1:** EventPlotOpenGLWidget::exportToSVG()
- [x] **Phase 2.2:** Export button in EventPlotPropertiesWidget
- [x] **Phase 2.3:** EventPlotWidget::handleExportSVG()
- [x] **Phase 2.4:** Wire signals in EventPlotWidgetRegistration
- [x] **Phase 2.5:** Manual validation
- [x] **Phase 3:** Refactor DataViewer SVGExporter to use PlottingSVG
- [ ] **Phase 4.1:** Shared ExportWidget (optional, deferred)
- [ ] **Phase 5:** Extend to PSTHWidget, LinePlotWidget, etc.
