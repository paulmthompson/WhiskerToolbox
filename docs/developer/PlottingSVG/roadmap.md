# PlottingSVG Implementation Roadmap

Step-by-step plan for building the `PlottingSVG` library and integrating SVG export into plot widgets.

## Phase 1: Create PlottingSVG Library Foundation

### Step 1.1: Create directory structure and CMakeLists.txt

Create `src/PlottingSVG/` with the following layout:

```
src/PlottingSVG/
├── CMakeLists.txt
├── SVGDocument.hpp
├── SVGDocument.cpp
├── SVGSceneRenderer.hpp
├── SVGSceneRenderer.cpp
├── SVGUtils.hpp
├── SVGUtils.cpp
├── Renderers/
│   ├── SVGPolyLineRenderer.hpp
│   ├── SVGPolyLineRenderer.cpp
│   ├── SVGGlyphRenderer.hpp
│   ├── SVGGlyphRenderer.cpp
│   ├── SVGRectangleRenderer.hpp
│   └── SVGRectangleRenderer.cpp
└── Decorations/
    ├── SVGDecoration.hpp
    ├── SVGAxisRenderer.hpp
    ├── SVGAxisRenderer.cpp
    ├── SVGScalebar.hpp
    └── SVGScalebar.cpp
```

Create `CMakeLists.txt` as a static library with dependencies on `CorePlotting` and `glm::glm` only. No Qt dependency. Model on `src/PlottingOpenGL/CMakeLists.txt` but remove all Qt/OpenGL linkage.

Register in `src/CMakeLists.txt` by adding `add_subdirectory(PlottingSVG)` **outside** the `if (ENABLE_UI)` block, placed after `add_subdirectory(CorePlotting)`.

**Files to create:**
- `src/PlottingSVG/CMakeLists.txt`

**Files to modify:**
- `src/CMakeLists.txt` — add `add_subdirectory(PlottingSVG)`

**Verify:** `cmake --preset linux-clang-release` configures without errors.

---

### Step 1.2: Implement SVGUtils (shared utilities)

Migrate `transformVertexToSVG()` and `colorToSVGHex()` from `src/CorePlotting/Export/SVGPrimitives.cpp` into `src/PlottingSVG/SVGUtils.hpp/.cpp` under the `PlottingSVG` namespace.

These are pure math functions with no dependencies beyond `<glm/glm.hpp>` and `<string>`.

**Source reference:** `src/CorePlotting/Export/SVGPrimitives.cpp` lines 10-45

**Functions to migrate:**
- `transformVertexToSVG(glm::vec4, glm::mat4 mvp, int w, int h) → glm::vec2`
- `colorToSVGHex(glm::vec4) → std::string`

**Files to create:**
- `src/PlottingSVG/SVGUtils.hpp`
- `src/PlottingSVG/SVGUtils.cpp`

**Verify:** Build compiles.

---

### Step 1.3: Implement per-batch SVG renderers

Migrate the three batch rendering functions from `CorePlotting/Export/SVGPrimitives.cpp` into dedicated renderer classes in `src/PlottingSVG/Renderers/`.

Each renderer is a stateless class with a single `render()` method that takes a batch, view/projection matrices, and canvas dimensions, returning a vector of SVG element strings.

#### SVGPolyLineRenderer
- Migrate from `CorePlotting::renderPolyLineBatchToSVG()`
- Input: `RenderablePolyLineBatch`, view matrix, projection matrix, canvas size
- Output: `vector<string>` of `<polyline>` elements
- Handles per-line colors, stroke width, line join/cap

**Source reference:** `src/CorePlotting/Export/SVGPrimitives.cpp` — `renderPolyLineBatchToSVG()`

#### SVGGlyphRenderer
- Migrate from `CorePlotting::renderGlyphBatchToSVG()`
- Input: `RenderableGlyphBatch`, view matrix, projection matrix, canvas size
- Output: `vector<string>` of glyph SVG elements
- Handles all glyph types: Tick (`<line>`), Circle (`<circle>`), Square (`<rect>`), Cross (two `<line>` elements)
- Per-glyph colors and opacity

**Source reference:** `src/CorePlotting/Export/SVGPrimitives.cpp` — `renderGlyphBatchToSVG()`

#### SVGRectangleRenderer
- Migrate from `CorePlotting::renderRectangleBatchToSVG()`
- Input: `RenderableRectangleBatch`, view matrix, projection matrix, canvas size
- Output: `vector<string>` of `<rect>` elements
- Handles per-rectangle colors with fill-opacity

**Source reference:** `src/CorePlotting/Export/SVGPrimitives.cpp` — `renderRectangleBatchToSVG()`

**Files to create:**
- `src/PlottingSVG/Renderers/SVGPolyLineRenderer.hpp` / `.cpp`
- `src/PlottingSVG/Renderers/SVGGlyphRenderer.hpp` / `.cpp`
- `src/PlottingSVG/Renderers/SVGRectangleRenderer.hpp` / `.cpp`

**Verify:** Build compiles. Write unit tests for each renderer class (Step 1.7).

---

### Step 1.4: Implement SVGDocument

Create the SVG XML document assembly class. Manages the SVG header (XML declaration, `<svg>` tag with viewBox), background rect, named `<g>` layer groups, and the closing `</svg>` tag.

Migrate and improve the document assembly logic from `CorePlotting::buildSVGDocument()`.

**API:**
```cpp
SVGDocument(int width, int height);
void setBackground(std::string const & hex_color);
void setDescription(std::string const & desc);
void addElements(std::string const & layer_name, std::vector<std::string> const & elements);
[[nodiscard]] std::string build() const;
```

**Improvements over existing `buildSVGDocument()`:**
- Named `<g>` layers for logical grouping (e.g., `<g id="intervals">`, `<g id="events">`, `<g id="decorations">`)
- Configurable description text (default: "WhiskerToolbox Export")
- Layer ordering preserved by insertion order

**Source reference:** `src/CorePlotting/Export/SVGPrimitives.cpp` — `buildSVGDocument()`

**Files to create:**
- `src/PlottingSVG/SVGDocument.hpp`
- `src/PlottingSVG/SVGDocument.cpp`

**Verify:** Build compiles.

---

### Step 1.5: Implement SVGSceneRenderer

Create the main public API class that orchestrates batch rendering and document assembly. This is the primary class consumers interact with.

**Responsibilities:**
1. Accept a `RenderableScene` (by const reference)
2. Configure canvas size and background color
3. Accept decorations via `addDecoration()`
4. Render by:
   a. Creating an `SVGDocument`
   b. Iterating scene batches in render order (rectangles → polylines → glyphs)
   c. Calling the appropriate per-batch renderer for each batch
   d. Adding elements to named document layers
   e. Calling each decoration's `render()` and adding to a "decorations" layer
   f. Returning `SVGDocument::build()` result

**API:**
```cpp
void setScene(CorePlotting::RenderableScene const & scene);
void setCanvasSize(int width, int height);
void setBackgroundColor(std::string const & hex_color);
void addDecoration(std::unique_ptr<SVGDecoration> decoration);
void clearDecorations();
[[nodiscard]] std::string render() const;
[[nodiscard]] bool renderToFile(std::filesystem::path const & path) const;
```

**Files to create:**
- `src/PlottingSVG/SVGSceneRenderer.hpp`
- `src/PlottingSVG/SVGSceneRenderer.cpp`

**Verify:** Build compiles. Can render a trivial `RenderableScene` to a valid SVG string.

---

### Step 1.6: Implement Decorations

#### SVGDecoration interface

```cpp
class SVGDecoration {
public:
    virtual ~SVGDecoration() = default;
    [[nodiscard]] virtual std::vector<std::string>
    render(int canvas_width, int canvas_height) const = 0;
};
```

#### SVGScalebar
- Migrate from `CorePlotting::createScalebarSVG()`
- Constructor takes: scalebar length (time units), time range start/end
- Configurable: position (default: bottom-right), bar thickness, tick height, label font size, colors
- Produces: `<line>` for bar and ticks, `<text>` for label

**Source reference:** `src/CorePlotting/Export/SVGPrimitives.cpp` — `createScalebarSVG()`

#### SVGAxisRenderer
- New implementation (no existing code to migrate)
- Constructor takes: axis range (min, max), tick interval, label text
- Configuration: position (Left or Bottom), font size, tick length, colors
- Produces: `<line>` for axis spine and ticks, `<text>` for tick labels and axis title

**Files to create:**
- `src/PlottingSVG/Decorations/SVGDecoration.hpp`
- `src/PlottingSVG/Decorations/SVGScalebar.hpp` / `.cpp`
- `src/PlottingSVG/Decorations/SVGAxisRenderer.hpp` / `.cpp`

**Verify:** Build compiles.

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

After PlottingSVG is built and tested, remove the original SVG code from CorePlotting.

**Option A (recommended for this step):** Leave a forwarding header at `src/CorePlotting/Export/SVGPrimitives.hpp` that includes the new PlottingSVG headers and has `[[deprecated]]` attributes on the old function declarations. This prevents breaking any in-flight code during the transition.

**Option B (later cleanup):** Remove `Export/SVGPrimitives.hpp` and `Export/SVGPrimitives.cpp` entirely once all consumers are migrated.

**Files to modify:**
- `src/CorePlotting/Export/SVGPrimitives.hpp` — replace with forwarding/deprecation header
- `src/CorePlotting/Export/SVGPrimitives.cpp` — remove (forwarding header handles it)
- `src/CorePlotting/CMakeLists.txt` — remove `Export/SVGPrimitives.cpp` from sources, add `PlottingSVG` dependency if using forwarding approach

**Verify:** Full build succeeds. All existing tests pass. DataViewer SVG export still works (it still includes the forwarding header).

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

## Phase 3: Refactor DataViewer SVGExporter

### Step 3.1: Update SVGExporter to use PlottingSVG

Replace the direct call to `CorePlotting::buildSVGDocument()` in `SVGExporter::exportToSVG()` with `PlottingSVG::SVGSceneRenderer::render()`. The existing `buildScene()` method stays unchanged — it already produces a `RenderableScene`.

**Before:**
```cpp
CorePlotting::SVGExportParams params;
params.canvas_width = svg_width_;
std::string svg_content = CorePlotting::buildSVGDocument(scene, params);
// ... manual string insertion for scalebar
```

**After:**
```cpp
PlottingSVG::SVGSceneRenderer renderer;
renderer.setScene(scene);
renderer.setCanvasSize(svg_width_, svg_height_);
renderer.setBackgroundColor(gl_widget_->getBackgroundColor());
if (scalebar_enabled_) {
    renderer.addDecoration(
        std::make_unique<PlottingSVG::SVGScalebar>(scalebar_length_, start_time, end_time));
}
std::string svg_content = renderer.render();
```

**Files to modify:**
- `src/WhiskerToolbox/DataViewer_Widget/Rendering/SVGExporter.cpp` — replace `buildSVGDocument()` call
- `src/WhiskerToolbox/DataViewer_Widget/Rendering/SVGExporter.hpp` — update includes
- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt` — add `PlottingSVG` dependency (if not already inherited)

**Verify:** DataViewer SVG export produces identical (or visually equivalent) output to the pre-refactor version.

---

### Step 3.2: Remove CorePlotting forwarding header

Once no consumers directly include `CorePlotting/Export/SVGPrimitives.hpp`, remove the forwarding header entirely.

**Files to remove:**
- `src/CorePlotting/Export/SVGPrimitives.hpp`

**Files to modify:**
- `src/CorePlotting/CMakeLists.txt` — remove any remaining references to the Export/ directory

**Verify:** Full build succeeds. `grep -r "SVGPrimitives" src/` returns no hits outside PlottingSVG.

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

- [ ] **Phase 1.1:** Directory structure + CMakeLists.txt + register in build
- [ ] **Phase 1.2:** SVGUtils (coordinate transform, color conversion)
- [ ] **Phase 1.3:** Per-batch renderers (PolyLine, Glyph, Rectangle)
- [ ] **Phase 1.4:** SVGDocument (XML assembly with layers)
- [ ] **Phase 1.5:** SVGSceneRenderer (main entry point)
- [ ] **Phase 1.6:** Decorations (SVGScalebar, SVGAxisRenderer)
- [ ] **Phase 1.7:** Tests (all renderer classes + document + scene)
- [ ] **Phase 1.8:** Remove SVGPrimitives from CorePlotting (forwarding header)
- [ ] **Phase 2.1:** EventPlotOpenGLWidget::exportToSVG()
- [ ] **Phase 2.2:** Export button in EventPlotPropertiesWidget
- [ ] **Phase 2.3:** EventPlotWidget::handleExportSVG()
- [ ] **Phase 2.4:** Wire signals in EventPlotWidgetRegistration
- [ ] **Phase 2.5:** Manual validation
- [ ] **Phase 3.1:** Refactor DataViewer SVGExporter to use PlottingSVG
- [ ] **Phase 3.2:** Remove CorePlotting forwarding header
- [ ] **Phase 4.1:** Shared ExportWidget (optional, deferred)
- [ ] **Phase 5:** Extend to PSTHWidget, LinePlotWidget, etc.
