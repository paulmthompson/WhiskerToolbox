# Media_Widget Refactoring Roadmap

## Current Situation

Media_Widget was the first visualization widget in WhiskerToolbox. It is built on a
QGraphicsScene backend (`Media_Window`) with bespoke rendering, interaction, and
configuration code for every data type it displays (media, lines, points, masks, tensors,
intervals). Since its creation, the project has developed a mature, modular plotting
stack:

- **CorePlotting** — API-agnostic mappers, scene graph, hit testing, interaction controllers
- **PlottingOpenGL** — GPU-accelerated batch renderers (glyphs, polylines, rectangles)
- **Plots/Common** — Reusable Qt widgets (GlyphStyleControls, LineStyleControls,
  ColormapControls, PlotTooltipManager, axis widgets, PlotInteractionHelpers)
- **AutoParamWidget** — Schema-driven auto-generated parameter forms
- **ParameterSchema** — Compile-time schema extraction from reflect-cpp structs

Media_Widget duplicates significant portions of this infrastructure. Adding new features
is difficult because the rendering pipeline, interaction handling, and UI controls are
tightly coupled.

### The Immediate Pain Point: Plotting Without Media

The motivating feature — plotting data (masks, lines, points) when **no media is loaded**
— is blocked not by a hard technical limitation, but by entanglement. The canvas size is
already independent of media resolution (driven by QGraphicsView widget size), and
`_clearMedia()` just fills black. The real issues are:

1. The `_plotMediaData()` method has interleaved concerns: media rendering,
   transparency mask application, colormap processing, and multi-media compositing
2. The overlay rendering methods (`_plotLineData`, `_plotMaskData`, etc.) use
   aspect-ratio scaling factors derived from media dimensions for coordinate conversion
3. There's no explicit "canvas-only mode" — the widget treats media absence as
   "nothing to show" rather than "show overlays on blank canvas"

---

## Consolidation Opportunities

### Tier 1 — Quick Wins (Low Risk, High Value)

These can be done independently, in any order, without touching the rendering pipeline.

#### 1.1 Replace MediaPoint_Widget Styling with GlyphStyleControls

**Status: ✅ Complete**

Replaced bespoke `ColorPicker_Widget`, point size slider/spinbox, and marker shape
combo box with an embedded `GlyphStyleControls` + `GlyphStyleState` instance.
`PointMarkerShape` enum was removed; `PointDisplayOptions::marker_shape` now uses
`CorePlotting::GlyphType` directly. The rendering code in `Media_Window::_plotPointData()`
was updated to switch on `CorePlotting::GlyphType` values. The `Tick` glyph type (vertical
line) replaces the old `X` marker shape.

#### 1.2 Replace MediaLine_Widget Styling with LineStyleControls

**Status: ✅ Complete**

Replaced bespoke `ColorPicker_Widget`, line thickness slider/spinbox with an embedded
`LineStyleControls` + `LineStyleState` instance. The color, alpha, and thickness
controls are now provided by the reusable `LineStyleControls` widget from
`Plots/Common/LineStyleControls/`. Line-specific controls (show_points, edge_snapping,
segment controls, position_marker) remain unchanged as domain-specific features.

#### 1.3 Replace MediaMask_Widget Styling with Reusable Controls

**What exists:**
- Bespoke color picker and alpha slider (same pattern as points/lines)
- Mask-specific toggles: `show_bounding_box`, `show_outline`, `use_as_transparency`

**Approach:**
- For color + alpha: embed `LineStyleControls` (it provides color + alpha, thickness can
  be hidden/ignored) or create a simpler `ColorAlphaControls` widget if that feels cleaner
- The mask-specific checkboxes remain as-is

#### 1.4 Convert Selection Widget Parameters to ParameterSchema/AutoParamWidget

**What exists (5 bespoke selection widgets):**

| Widget | Parameters |
|--------|-----------|
| `LineAddSelectionWidget` | edge_snapping (bool), smoothing_mode (enum), polynomial_order (int), edge_threshold (double), edge_search_radius (double) |
| `LineEraseSelectionWidget` | eraser_radius (int), show_circle (bool) |
| `LineSelectSelectionWidget` | selection_threshold (float) |
| `LineDrawAllFramesSelectionWidget` | Complex multi-step workflow (start/complete/apply buttons) — **not a good candidate** |
| `LineNoneSelectionWidget` | No parameters — just an info label |

**Good candidates:** LineAddSelectionWidget, LineEraseSelectionWidget,
LineSelectSelectionWidget

These have simple scalar/bool/enum parameters that map directly to ParameterSchema +
AutoParamWidget. The same pattern already works for media processing options.

**Not a good candidate:** LineDrawAllFramesSelectionWidget has a stateful workflow
(start drawing → accumulate points → complete → apply). This is genuinely interactive,
not just parameter configuration.

**Approach:**
1. Define reflect-cpp structs for each (e.g., `LineAddParams`, `LineEraseParams`,
   `LineSelectParams`)
2. Replace each widget body with `AutoParamWidget` + connect `parametersChanged()` to
   the existing signal emissions

**Estimated scope per widget:** ~40-60 lines removed, ~20 lines added

#### 1.5 Convert MediaInterval StyleWidgets to AutoParamWidget

**What exists:**
- `BoxIntervalStyle_Widget` — box_size (int), frame_range (int), location (enum)
- `BorderIntervalStyle_Widget` — border_thickness (int)

**Approach:** Same as 1.4 — define reflect-cpp params structs, use AutoParamWidget.

#### 1.6 Convert MediaTensor_Widget to AutoParamWidget

**What exists:**
- channel (int), color (string), alpha (int)

**Approach:** Same as above. TensorDisplayOptions already has the reflect-cpp struct
(`display_channel`); just need to wire AutoParamWidget to it.

---

### Tier 2 — Medium Effort (Enable No-Media Mode)

These changes address the immediate pain point of rendering without media.

#### 2.1 Decouple Canvas Coordinate System from Media

**Current state:**

The coordinate mapping has three interacting pieces:

1. **Canvas pixel dimensions** (`_canvasWidth`, `_canvasHeight`): fixed at 640×480 by
   default. This is the QGraphicsScene / QImage backing size. Media frames are scaled
   *to* this size.
2. **Canvas coordinate system**: the logical resolution that overlay data coordinates
   are interpreted against. Currently this is implicitly the active media's native
   resolution (e.g., 640×480 for a 640×480 video). `getXAspect()` = `_canvasWidth /
   media.getWidth()` and `getYAspect()` = `_canvasHeight / media.getHeight()`.
3. **Per-data ImageSize**: `LineData`, `PointData`, and `MaskData` each carry an
   `ImageSize` (from `RaggedTimeSeries`). If defined (not -1,‑1), the overlay rendering
   substitutes this for the media resolution, computing `xAspect = _canvasWidth /
   data.getImageSize().width`. If undefined (-1,‑1), the data trusts the media
   resolution.

**Key behaviors to preserve (and formalize):**

- **MediaData is always "correct":** its native resolution defines the authoritative
  canvas coordinate system when loaded. (Note: `MediaData` defaults to 640×480 when
  uninitialized — it never uses -1,‑1 — so an empty media object silently produces a
  coordinate system that may not match loaded overlay data.)
- **ImageSize -1,‑1 means "undefined":** data objects with undefined image size defer
  to the canvas coordinate system (currently media).
- **Scaling is "stretch to fill", not "spatial subset":** when `PointData` has
  ImageSize 256×256 and media is 640×480, the points are scaled up so the 256×256 grid
  fills the full canvas. The implicit assumption is that the lower-resolution data
  represents the same spatial extent at coarser sampling, **not** a 256×256 crop of
  the 640×480 field of view. There is currently no way to express the alternative
  interpretation.

**Problems this creates for no-media mode:**

- The program launches with empty `MediaData` whose size defaults to 640×480. If overlay
  data (e.g., masks at 512×512) is loaded without media, `getXAspect()` returns 1.0
  (because no *visible* media → fallback), which silently misaligns data that expects
  coordinate mapping.
- There is no explicit "canvas coordinate system" object — it is scattered across
  `getXAspect()`, `getYAspect()`, and per-overlay ad-hoc scaling logic, making it
  impossible for the user to inspect or override.
- There is no mechanism for a data object to declare whether it should be stretched to
  fill the canvas or whether it represents a spatial subset (crop / ROI) of the canvas
  coordinates.

**Approach:**

1. **Introduce `CanvasCoordinateSystem`** — a small struct holding the logical
   `{ width, height }` that defines the coordinate space overlays are plotted into.
   This replaces the implicit "ask the active media for its resolution" pattern.

2. **Resolution priority chain** — the canvas coordinate system should be determined
   by the first source that provides a valid (non -1,‑1) resolution, in this order:
   1. **User override** (explicit manual setting via UI / state — highest priority)
   2. **Active media** (`MediaData::getImageSize()`, if visible and not default-only)
   3. **First loaded data object with defined ImageSize** (e.g., a MaskData at 512×512)
   4. **Fallback default** (640×480, matching the current behavior)

   When media is loaded/unloaded or data objects are added/removed, the coordinate
   system should be re-evaluated unless there is an active user override.

3. **Per-data coordinate mapping mode** — add a per-data-object setting (likely in the
   display options, e.g., `PointDisplayOptions`, `LineDisplayOptions`,
   `MaskDisplayOptions`) that controls how the data's ImageSize relates to the canvas:

   | Mode | Meaning | Scaling |
   |------|---------|---------|
   | `ScaleToCanvas` (default) | Data covers the full canvas extent at its own resolution. Equivalent to current behavior. | `xAspect = canvasPixelWidth / data.imageSize.width` |
   | `SubsetOfCanvas` | Data occupies a spatial sub-region. Coordinates are in canvas-coordinate-system units already. | `xAspect = canvasPixelWidth / canvasCoordSystem.width` (same as media-based scaling) |

   `ScaleToCanvas` preserves backward compatibility: data at 256×256 is stretched to
   fill a 640×480 canvas. `SubsetOfCanvas` allows, e.g., a 256×256 point cloud that
   lives in the top-left quadrant of a 640×480 canvas.

   When ImageSize is -1,‑1, the mode is moot — the data uses the canvas coordinate
   system directly (equivalent to `SubsetOfCanvas` with matching dimensions).

4. **Replace `getXAspect()` / `getYAspect()`** — these methods currently encode the
   full priority chain in an ad-hoc way. Replace them with a single method (or free
   function) that takes `CanvasCoordinateSystem`, the data's `ImageSize`, and the
   mapping mode, returning the x/y scale factors. All `_plotXxxData()` methods call
   this instead of computing their own scaling.

5. **UI for canvas coordinate system** — expose the resolved canvas coordinate system
   in the Media_Widget UI (read-only display showing source: "from media", "from
   MaskData 'whiskers'", "manual override"). Provide width/height spinboxes for
   manual override. This can live in MediaWidgetState as a serializable field.

**Status: ✅ Complete**

**Sub-tasks:**
- [x] Define `CanvasCoordinateSystem` struct — `src/CorePlotting/Layout/CanvasCoordinateSystem.hpp`
- [x] Define `CoordinateMappingMode` enum (`ScaleToCanvas`, `SubsetOfCanvas`) — same file
- [x] Add `CoordinateMappingMode` field to `LineDisplayOptions`, `PointDisplayOptions`,
      `MaskDisplayOptions` (default: `ScaleToCanvas`) — `DisplayOptions.hpp`
- [x] Add `CanvasCoordinateSystem` to `MediaWidgetState` (with optional user override) — `MediaWidgetStateData.hpp`, `MediaWidgetState.hpp/.cpp`
- [x] Implement resolution priority chain in `Media_Window::resolveCanvasCoordinateSystem()` —
      called at the top of each `UpdateCanvas()`; priority: user override → active media →
      first data object with defined ImageSize → 640×480 fallback
- [x] Replace `getXAspect()` / `getYAspect()` with unified scaling function —
      `computeScalingFactors()` / `computeInverseScalingFactors()` free functions in
      `CanvasCoordinateSystem.hpp`; `getXAspect()`/`getYAspect()` kept as backward-compatible
      wrappers (used by `MediaLine_Widget.cpp`)
- [x] Update all `_plotXxxData()` and hit-testing methods to use new scaling function —
      `_plotLineData`, `_plotPointData`, `_plotMaskData` (bounding box), `_findLineAtPosition`,
      `_findPointAtPosition` (**bug fix**: was ignoring ImageSize entirely), `_findMaskAtPosition`,
      `updateTemporaryLine` (**bug fix**: fetched ImageSize but never used it)
- [x] Add UI controls in Media_Widget for viewing/overriding canvas coordinate system —
      `CanvasCoord_Widget` in `Media_Widget/UI/`, integrated into `MediaPropertiesWidget` as a
      collapsible section; shows resolved width×height and source, with manual override
      via QCheckBox + QSpinBox pair
- [x] Unit tests for coordinate mapping (pure math, no Qt dependency) —
      `tests/CorePlotting/CanvasCoordinateSystem.test.cpp`, 19 test cases / 44 assertions

**This is the key unlock** — once the canvas coordinate system is an explicit,
inspectable, overridable entity, overlay rendering becomes independent of media
presence, and plotting without media becomes trivial.

#### 2.2 Add Canvas-Only Mode to Media_Window

**Completed prerequisite:** `Media_Widget::setDataManager()` now calls `updateMedia()` at
the end of setup, wiring the QGraphicsScene to the QGraphicsView immediately on widget
creation. Previously this was only called from `MainWindow::loadData()`, which meant
overlays were invisible until the user loaded a media file. The legacy loop in
`MainWindow::loadData()` has been removed.

**Remaining approach:**
1. Add a `bool _has_media` flag or derive from whether any media data key is registered
2. In `UpdateCanvas()`: if no media, skip `_plotMediaData()` but still render all overlays
3. Allow background color configuration (currently hardcoded black)
4. Support user-specified canvas dimensions via state when no media is loaded

**Estimated scope:** ~50 lines — mostly guard changes in `UpdateCanvas()` and
`_plotMediaData()`, plus a state field for "background_color"

#### 2.3 Extract Coordinate Mapping to Standalone Utility

**Current state:**
- `getXAspect()`, `getYAspect()`, `_getMediaScaledPosition()` are methods on
  `Media_Window`
- Each overlay type has its own scaling logic scattered through 1800+ lines of
  `Media_Window.cpp`

**Approach:**
1. Create a free-function utility:
   `CanvasPoint mapToCanvas(DataPoint, CanvasCoordinateSystem, DataBounds)`
2. Use it in all `_plotXxxData()` methods
3. This also makes it testable without Qt (unit tests for coordinate math)

---

### Tier 3 — Significant Effort (Leverage CorePlotting)

These integrate Media_Widget's rendering with the modern stack.

#### 3.1 Use CorePlotting Mappers for Data → Geometry Conversion

**Current state:**
- `_plotLineData()` manually iterates `LineData`, creates `QPainterPath`, scales coordinates
- `_plotPointData()` manually creates QGraphicsItems per marker shape
- `_plotMaskData()` manually iterates Mask2D pixels, creates QImages

**CorePlotting already has:**
- `SpatialMapper::mapPointsAtTime()` → `MappedElement` (position + entity_id)
- `SpatialMapper::mapLinesAtTime()` → `MappedLineView` (vertices + entity_id)
- `MaskContourMapper::mapMaskContoursAtTime()` → `OwningLineView` (boundary polylines)
- `HeatmapMapper::toRectangleBatch()` → colored rectangles

**Approach:**
1. Replace the data traversal in each `_plotXxxData()` with CorePlotting mapper calls
2. Convert mapper output → QGraphicsItems (not OpenGL — we're staying on QGraphicsScene
   for now)
3. This decouples data interpretation from rendering and gives us entity tracking for free

**Value:** Immediate bugfix potential — the mappers handle cross-TimeFrame queries
correctly, which the bespoke code may not. Also enables hit testing via SceneHitTester.

#### 3.2 Use SceneHitTester for Entity Selection

**Current state:**
- `_findLineAtPosition()` — manual perpendicular distance calculation
- `_findPointAtPosition()` — manual Euclidean distance
- `_findMaskAtPosition()` — manual pixel membership check
- Each is a separate method with hardcoded thresholds

**CorePlotting already has:**
- `SceneHitTester` — multi-strategy hit testing (QuadTree for discrete elements,
  containment for rectangles/masks)
- Returns `HitTestResult` with entity_id and hit priority

**Approach:**
1. After mapping data with CorePlotting mappers (3.1), build a `RenderableScene` with
   spatial index
2. Replace `_findXxxAtPosition()` methods with `SceneHitTester::hitTest()`
3. Unifies all selection to one code path

#### 3.3 Use CorePlotting Interaction Controllers

**Current state:**
- Media_Window handles mouse events directly with mode-specific behavior
- Drawing modes accumulate `_drawing_points`
- Hover circle is manually managed

**CorePlotting already has:**
- `LineInteractionController` — free-form line drawing tool
- `IGlyphInteractionController` — brush/circle tools
- `GlyphPreview` — visual feedback for tools (hover circles, selection highlights)

**Approach:**
1. Create thin adapter between Media_Window mouse events and CorePlotting interaction
   controllers
2. This may not be worth doing unless Media_Widget moves to the OpenGL stack (Tier 4)

---

### Tier 4 — Long-Term (Architectural, Optional)

These are larger refactors that would fully modernize Media_Widget.

#### 4.1 Create PlottingQt Library (QGraphicsScene Backend for CorePlotting)

**Rationale:**
- PlottingOpenGL implements CorePlotting's `RenderableScene` → GPU rendering
- A `PlottingQt` library would implement the same → QGraphicsScene rendering
- This would let Media_Widget use CorePlotting's full pipeline without switching to OpenGL

**What it would contain:**
- `QtSceneRenderer`: takes `RenderableScene`, produces QGraphicsItems
- `QtGlyphRenderer`: maps glyph instances to QGraphicsEllipseItem/RectItem/etc
- `QtPolyLineRenderer`: maps polyline batches to QGraphicsPathItem
- `QtRectangleRenderer`: maps rectangle batches to QGraphicsRectItem

**Value:** Any code that renders to QGraphicsScene would benefit, not just Media_Widget.

**Alternative:** Skip this entirely and move Media_Widget to a QOpenGLWidget like all
other plot widgets. The tradeoffs:
- **Pro:** Unifies rendering stack; faster for large datasets; shader-based effects
- **Con:** Significant rewrite; QGraphicsScene has useful built-in features (item
  selection, drag handles, z-ordering) that would need reimplementation; video frame
  upload to texture is additional work

#### 4.2 Move Media_Widget to OpenGL Rendering

**If** 4.1 is skipped (no PlottingQt), the alternative is:
1. Replace QGraphicsScene with a QOpenGLWidget subclass
2. Video frames become GL textures
3. All overlays rendered via PlottingOpenGL renderers
4. Interaction via CorePlotting controllers + SceneHitTester

This is the "nuclear option" — most work but cleanest result. Only pursue if the
QGraphicsScene approach becomes a performance bottleneck.

---

## Unique Media_Widget Features (No Equivalent Elsewhere)

These capabilities exist only in Media_Widget and should be preserved during any refactor:

1. **Video/Image frame rendering** — no other widget renders media frames
2. **Multi-media compositing** — additive blending for multi-channel microscopy
3. **Transparency masks** — mask-as-alpha applied to media
4. **Line editing** — interactive point add/erase/select/move/draw-across-frames
5. **Mask painting** — brush-based paint/erase with real-time preview and undo
6. **Edge-snapped line drawing** — Canny edge detection + nearest edge point
7. **Polynomial smoothing** — fit polynomial to drawn line
8. **Digital interval indicators** — Box grid and border-flash display at frame-level
9. **Media processing pipeline** — contrast, gamma, sharpen, CLAHE, bilateral, median,
   magic eraser, colormap (recently converted to AutoParamWidget)
10. **Hover circle / temporary line** — interactive drawing previews

Items 1-3 are intrinsic to media display. Items 4-8 are interaction tools that could
potentially be abstracted but are not duplicated elsewhere. Items 9-10 are UI conveniences.

---

## Suggested Execution Order

```
Phase 1 — Quick Wins (can be done in any order, independently)
├── 1.1  Replace point styling → GlyphStyleControls
├── 1.2  Replace line styling → LineStyleControls
├── 1.3  Replace mask styling → reusable color/alpha controls
├── 1.4  Selection widgets → AutoParamWidget (Add, Erase, Select)
├── 1.5  Interval style widgets → AutoParamWidget
└── 1.6  Tensor widget → AutoParamWidget

Phase 2 — Enable No-Media Mode
├── 2.1  Decouple canvas coordinate system from media ← KEY PREREQUISITE
├── 2.2  Add canvas-only mode
└── 2.3  Extract coordinate mapping utility

Phase 3 — Leverage CorePlotting (incremental, per data type)
├── 3.1  Use SpatialMapper for points/lines, MaskContourMapper for masks
├── 3.2  Replace entity selection with SceneHitTester
└── 3.3  Use interaction controllers (optional)

Phase 4 — Architectural (only if needed)
├── 4.1  Create PlottingQt library  ──OR──
└── 4.2  Move to OpenGL rendering
```

Phase 1 reduces boilerplate and proves the consolidation pattern. Phase 2 solves the
immediate feature request. Phase 3 removes duplicated logic. Phase 4 is aspirational.

---

## Dependency Graph

```
1.1, 1.2, 1.3  ──(independent)──  no prerequisites
1.4, 1.5, 1.6  ──(independent)──  no prerequisites (AutoParamWidget already exists)

2.1  ← required by 2.2 and 2.3; no code prerequisites, but benefits from 1.3 (mask styling)
     being done first to reduce churn in MaskDisplayOptions
2.2  ← 2.1 (canvas-only mode is trivial once coordinate system is explicit)
2.3  ← 2.1

3.1  ← no prerequisites (CorePlotting mappers exist)
3.2  ← 3.1 (needs mapped data in RenderableScene for hit testing)
3.3  ← 3.1 (adapts interaction controllers to mapped data)

4.1 or 4.2 ← 3.1 (makes sense only after mappers are integrated)
```

---

## Risk Assessment

| Change | Risk | Reasoning |
|--------|------|-----------|
| 1.1-1.3 Style widget replacement | **Low** | Pure UI swap; rendering unchanged |
| 1.4-1.6 AutoParamWidget conversion | **Low** | Same pattern as media processing; proven |
| 2.1 Coordinate decoupling | **Low** | Complete. All sub-tasks done: struct, priority chain, scaling functions, rendering/hit-testing updates, UI controls (CanvasCoord_Widget), and unit tests (19 test cases). |
| 2.2 Canvas-only mode | **Low** | Additive; just new guards in UpdateCanvas() |
| 2.3 Coordinate utility extraction | **Low** | Pure refactor; testable |
| 3.1 CorePlotting mappers | **Medium** | Changes data traversal; need to verify visual equivalence |
| 3.2 SceneHitTester | **Medium** | Changes selection behavior; need to verify thresholds |
| 3.3 Interaction controllers | **High** | Mouse event flow is complex; risk of breaking line/mask editing |
| 4.x Rendering backend change | **High** | Fundamental rewrite; months of work |
