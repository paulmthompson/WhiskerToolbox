# EventPlotWidget (Raster Plot) Development Roadmap

## Overview

This document outlines the development plan for transforming EventPlotWidget into a full-featured raster plot visualization widget. The widget displays neural spike/event data aligned to trial events, supporting advanced features like trial sorting, event coloring, and cross-widget linking.

## Current State

EventPlotWidget has:
- ✅ Basic EditorState architecture (`EventPlotState`, `EventPlotStateData`)
- ✅ PlotAlignmentWidget integration for alignment settings
- ✅ Properties panel structure (`EventPlotPropertiesWidget`)
- ✅ Glyph type enum (Tick, Circle, Square)
- ✅ Basic event options (color, thickness, glyph type)
- ✅ **OpenGL rendering infrastructure (Phase 1 complete)**
- ✅ **View state with X/Y zoom support**
- ✅ **GatherResult integration for trial alignment**
- ✅ **Consolidated signal architecture**
- ✅ **Zoom/pan mouse interaction with separated X/Y control (Phase 2)**
- ✅ **Single-click hit testing using SceneHitTester (Phase 2)**
- ✅ **Performance-optimized view transforms (Phase 2)**
- ✅ **Built-in trial sorting (by first event latency, by event count) (Phase 4)**
- ❌ No external feature sorting (future DataTransform v2 integration)
- ❌ No cross-widget linking

## Architecture Reference

### Design Patterns to Follow

| Component | Reference Implementation | Key Files |
|-----------|--------------------------|-----------|
| State Management | DataViewerState | [DataViewerState.hpp](../../DataViewer_Widget/Core/DataViewerState.hpp) |
| OpenGL Rendering | DataViewer OpenGLWidget | [OpenGLWidget.hpp](../../DataViewer_Widget/Rendering/OpenGLWidget.hpp) |
| Scene Building | CorePlotting RasterMapper | [RasterMapper.hpp](/src/CorePlotting/Mappers/RasterMapper.hpp) |
| Hit Testing | SceneHitTester | [SceneHitTester.hpp](/src/CorePlotting/Interaction/SceneHitTester.hpp) |
| Selection | DataViewerSelectionManager | [DataViewerSelectionManager.hpp](../../DataViewer_Widget/Interaction/DataViewerSelectionManager.hpp) |
| Polygon Selection | PolygonInteractionController | [PolygonInteractionController.hpp](/src/CorePlotting/Interaction/PolygonInteractionController.hpp) |
| Data Gathering | GatherResult | [GatherResult.hpp](/src/DataManager/utils/GatherResult.hpp) |

### Key Libraries

- **CorePlotting**: API-independent layout, mapping, and interaction logic
- **PlottingOpenGL**: OpenGL rendering implementation (SceneRenderer, glyph batches)
- **DataManager**: Data types (DigitalEventSeries), GatherResult, transforms

---

## Signal/Slot Design: Consolidated Patterns ✅

**Status**: Implemented in [EventPlotState.hpp](Core/EventPlotState.hpp)

**Problem**: With multiple event series and many per-event options, individual signals 
would create excessive complexity.

### Signal Complexity Estimate

| Category | Properties | Individual Signals | With Pattern |
|----------|------------|-------------------|--------------|
| Alignment | 4 | 4 | 1 |
| View State | 6 | 6 | 1 |
| Per-Event Options | N × 8 | 8N | 1 per key |
| Global Options | 10 | 10 | 1 |
| Sorting | 3 | 3 | 1 |
| Coloring | 5 | 5 | 1 |
| Linking/Pin | 2 | 2 | 1 |

**With 5 events**: 78 individual signals → **~12 consolidated signals**

### Implementation

The consolidated signal pattern uses category-level signals:
- `alignmentChanged()` - any alignment property
- `viewStateChanged()` - zoom, pan, bounds
- `plotEventAdded/Removed/OptionsChanged(key)` - per-event options

See [EventPlotState.hpp](Core/EventPlotState.hpp) for the complete signal definitions.

**Future**: Consider `EventOptionsRegistry` pattern (like `SeriesOptionsRegistry` in DataViewer) 
if per-event option complexity grows.

---

## Phase 1: Core Rendering Infrastructure ✅

**Status**: Complete

**Goal**: Establish OpenGL rendering pipeline using CorePlotting patterns.

### Implementation

| Component | File |
|-----------|------|
| OpenGL Widget | [EventPlotOpenGLWidget.hpp](Rendering/EventPlotOpenGLWidget.hpp) |
| View State | [EventPlotState.hpp](Core/EventPlotState.hpp) - `EventPlotViewState` struct |
| Scene Building | Uses `CorePlotting::RasterMapper` and `PlottingOpenGL::SceneRenderer` |

### Data Flow

```
EventPlotState (alignment settings)
        ↓
GatherResult<DigitalEventSeries> (trial-aligned views)
        ↓
RasterMapper::mapTrials() → MappedElement ranges
        ↓
SceneBuilder::addGlyphs() → RenderableScene
        ↓
SceneRenderer::render()
```

### Key Features Implemented

- **EventPlotOpenGLWidget**: QOpenGLWidget subclass with PlottingOpenGL integration
- **EventPlotViewState**: Serializable view settings (x/y bounds, zoom factors, glyph size)
- **GatherResult integration**: Trial alignment using `gather()` function
- **State signals**: `viewStateChanged()`, `alignmentChanged()` for UI updates
- **Coordinate transforms**: World ↔ screen coordinate conversion

---

## Phase 2: Interaction - Zoom, Pan, Selection ✅

**Status**: Complete (with descoped items noted)

**Goal**: Enable user interaction with the plot.

### 2.1 Separated X/Y Zoom ✅

**Implemented** independent zoom for time (X) and trial (Y) axes.

**Zoom Modes**:
- Mouse wheel: X-zoom only (time-focused exploration)
- Shift+wheel: Y-zoom only (trial-focused exploration)  
- Ctrl+wheel: Uniform zoom (both axes)

**Performance Optimization**: Zoom and pan operations now only update the projection 
matrix via `viewStateChanged()` signal - they do NOT trigger `stateChanged()` which 
would cause expensive scene rebuilds. This separation between "view transform" and 
"data bounds" is documented in `EventPlotViewState`.

**Key Implementation Files**:
- [EventPlotOpenGLWidget.cpp](Rendering/EventPlotOpenGLWidget.cpp) - `handleZoom()`, `wheelEvent()`
- [EventPlotState.cpp](Core/EventPlotState.cpp) - `setXZoom()`, `setYZoom()`, `setPan()`

### 2.2 Hit Testing & Single-Click Selection ✅

**Implemented** using CorePlotting's SceneHitTester for point queries.

**Selection Features**:
- Single click: Select event (emits `eventSelected()` signal)
- Click-vs-drag detection (5 pixel threshold)
- Double-click: Navigate to event time (emits `eventDoubleClicked()`)

**Key Implementation Files**:
- [EventPlotOpenGLWidget.hpp](Rendering/EventPlotOpenGLWidget.hpp) - `eventSelected()` signal
- [EventPlotOpenGLWidget.cpp](Rendering/EventPlotOpenGLWidget.cpp) - `handleClickSelection()`, `findEventNear()`

### 2.3 Polygon Selection 🔜 (Descoped)

**Descoped** per project decision - not needed for current use cases. Single-click 
selection is sufficient for the raster plot widget.

If needed in the future, can integrate `PolygonInteractionController`:

```cpp
// Interaction mode enum (future)
enum class EventPlotInteractionMode {
    Normal,        // Pan, zoom, single-click select
    PolygonSelect  // Lasso selection mode
};
```

### 2.4 Double-Click → Time Navigation ✅

**Implemented** via `eventDoubleClicked()` signal which is connected to time navigation 
in the UI layer.

---

## Phase 3: Visual Customization

**Goal**: Full control over glyph appearance.

### 3.1 Glyph Options in State

Extended `EventPlotOptions`:

```cpp
struct EventPlotOptions {
    std::string event_key;
    
    // Glyph appearance
    EventGlyphType glyph_type = EventGlyphType::Tick;
    double glyph_size = 3.0;        // Size in pixels
    double tick_height = 0.8;       // For Tick type: fraction of row height
    std::string hex_color = "#000000";
    double alpha = 1.0;
    
    // Border (for Circle/Square)
    bool show_border = false;
    std::string border_color = "#000000";
    double border_width = 1.0;
};
```

### 3.2 Properties Panel Updates

Update `EventPlotPropertiesWidget` to expose all options:
- Glyph type combo box (already exists)
- Size spinbox
- Color picker button (already exists)
- Alpha slider
- Border checkbox + color

### 3.3 Axis Labels

Add axis labeling support:

```cpp
struct EventPlotAxisOptions {
    std::string x_label = "Time (ms)";
    std::string y_label = "Trial";
    bool show_x_axis = true;
    bool show_y_axis = true;
    bool show_grid = false;
};
```

Use `PlottingOpenGL::AxisRenderer` for rendering.

---

## Phase 4: Trial Sorting ✅ (Built-in Modes)

**Status**: Built-in sorting modes implemented. External feature sorting remains for future DataTransform v2 integration.

**Goal**: Allow trials to be reordered based on computed features.

### 4.1 Implemented: Built-in Sorting Modes ✅

The following sorting modes are available via UI combo box:

```cpp
/**
 * @brief Enumeration for trial sorting modes
 * 
 * Built-in modes compute sort keys directly from the GatherResult data.
 * External mode is reserved for future DataTransform v2 integration.
 */
enum class TrialSortMode {
    TrialIndex,         ///< No sorting - display in original trial order (default)
    FirstEventLatency,  ///< Sort by latency to first positive event (ascending)
    EventCount          ///< Sort by total number of events (descending)
    // Future: External - sort by external feature from DataManager
};
```

**Key Implementation Files**:
- [EventPlotState.hpp](Core/EventPlotState.hpp) - `TrialSortMode` enum, `sorting_mode` in state data
- [EventPlotOpenGLWidget.cpp](Rendering/EventPlotOpenGLWidget.cpp) - `applySorting()` method
- [EventPlotPropertiesWidget.ui](UI/EventPlotPropertiesWidget.ui) - Sorting combo box

### 4.2 Sorting Algorithm Details

**First Event Latency (Ascending)**:
- Finds the first event at or after alignment time (t=0) for each trial
- Trials are ordered by this latency (smallest first)
- Trials with no positive events are placed at the end

**Event Count (Descending)**:
- Counts total events in each trial's window
- Trials are ordered by count (most events first)

### 4.3 Future: External Feature Sorting (Design Only)

For user-computed sort keys via DataTransform v2:

**Option A: Pre-computed sort values**
- User creates a transform that computes per-trial values
- Transform output is an AnalogTimeSeries where index = trial, value = sort key
- EventPlotWidget reads this series for ordering

**Option B: Inline transform specification** (future)
- EventPlotState stores a transform pipeline specification
- Widget executes pipeline at render time
- More flexible but higher complexity

### 4.4 GatherResult Sorting Support

`GatherResult` provides `reorder()` method used by `applySorting()`:

```cpp
// In EventPlotOpenGLWidget::rebuildScene()
auto sorting_mode = _state->getSortingMode();
if (sorting_mode != TrialSortMode::TrialIndex) {
    gathered = applySorting(gathered, sorting_mode);
}

// applySorting computes sort indices and calls:
return gathered.reorder(sort_indices);
```

---

## Phase 5: Event Coloring by Feature (Design Exploration)

**Goal**: Color events based on associated values from external data.

### 5.1 Color Mapping Concept

```cpp
struct EventColorConfig {
    std::string color_mode;  // "fixed", "by_feature"
    
    // For fixed coloring
    std::string hex_color = "#000000";
    
    // For feature-based coloring
    std::optional<std::string> feature_data_key;  // AnalogTimeSeries key
    std::string colormap = "viridis";  // Colormap name
    double color_min = 0.0;
    double color_max = 1.0;
    bool use_auto_range = true;
};
```

### 5.2 Colormap Infrastructure

Add to CorePlotting or shared utility:

```cpp
namespace CorePlotting::Colormaps {
    // Returns RGBA for value in [0, 1]
    glm::vec4 viridis(float t);
    glm::vec4 plasma(float t);
    glm::vec4 inferno(float t);
    // ...
}
```

### 5.3 Feature Sampling

For each event, sample the feature value:

```cpp
// Pseudocode for continuous feature coloring
for (auto const& event : trial_events->view()) {
    float feature_value = feature_series->getValueAtTime(event.time());
    float normalized = (feature_value - color_min) / (color_max - color_min);
    glm::vec4 color = colormap(std::clamp(normalized, 0.0f, 1.0f));
    // Add glyph with this color...
}
```

**Phase 5 Deliverable**: Design document only. Implementation depends on 
efficient feature sampling infrastructure.

---

## Phase 6: Cross-Widget Linking

**Goal**: Enable PSTH/Raster/Heatmap to synchronize on active unit selection.

### 6.1 Use Existing SelectionContext (No Modifications Needed)

Following the DataInspector pattern, we use the existing `selectionChanged` signal:

```cpp
// In EventPlotWidget/Properties constructor
connect(_selection_context, &SelectionContext::selectionChanged,
        this, &EventPlotWidget::_onSelectionChanged);

void EventPlotWidget::_onSelectionChanged(SelectionSource const& source) {
    // Ignore if pinned
    if (_state && _state->isPinned()) {
        return;
    }
    
    // Ignore if change came from us
    if (_state && source.editor_instance_id.toString() == _state->getInstanceId()) {
        return;
    }
    
    // Check if selected data is a DigitalEventSeries
    if (_selection_context) {
        auto const selected = _selection_context->primarySelectedData();
        if (selected.isValid()) {
            auto type = _data_manager->getType(selected.toString().toStdString());
            if (type == DM_DataType::DigitalEventSeries) {
                // Update the displayed event series
                updateDisplayedEventSeries(selected.toString());
            }
        }
    }
}
```

### 6.2 Widget Pin/Link Mode

Each plot widget (EventPlot, PSTH, Heatmap) supports pinning:

```cpp
// In EventPlotState
bool _pinned = false;

// Accessors
void setPinned(bool pinned);
[[nodiscard]] bool isPinned() const;

signals:
    void pinnedChanged(bool pinned);
```

**UI**: A pin button in properties panel (like DataInspector).

### 6.3 Heatmap → Raster/PSTH Flow

```
HeatmapWidget row click
        ↓
SelectionContext::setSelectedData("unit_42", source)
        ↓
EventPlotWidget (if not pinned) receives selectionChanged
        ↓
Checks type == DigitalEventSeries
        ↓
Updates plot with new event series
```

### 6.4 State Serialization

Add to all aligned plot states:

```cpp
// In EventPlotStateData, PSTHStateData, HeatmapStateData
bool pinned = false;  // Whether to ignore SelectionContext changes
```

---

## Phase 7: Shared Components Factoring

**Goal**: Extract reusable components for PSTH, Heatmap, and ScatterPlot.

### 7.1 Aligned Plot Base Components

Create shared infrastructure:

```
Plots/
├── Common/
│   ├── AlignedPlotDataProvider.hpp   # GatherResult-based data provision
│   ├── AlignedPlotDataProvider.cpp
│   ├── PlotLinkingController.hpp     # Cross-widget linking logic
│   ├── PlotLinkingController.cpp
│   ├── GlyphSelectionManager.hpp     # Shared selection logic
│   ├── GlyphSelectionManager.cpp
│   ├── PlotViewState.hpp             # Common view state (zoom, pan)
│   └── ColormapRegistry.hpp          # Shared colormap utilities
```

### 7.2 AlignedPlotDataProvider

Common data preparation pattern:

```cpp
class AlignedPlotDataProvider {
public:
    // Configure from alignment state
    void setAlignmentState(PlotAlignmentState* state);
    void setDataManager(std::shared_ptr<DataManager> dm);
    
    // Get gathered data for an event series
    GatherResult<DigitalEventSeries> gatherEvents(std::string const& key);
    
    // Sorting (when implemented)
    void setSortConfig(TrialSortConfig const& config);
    
signals:
    void dataChanged();  // When alignment settings change
};
```

### 7.3 Relationship to Other Widgets

| Widget | Uses GatherResult | Uses PlotLinking | Uses GlyphSelection |
|--------|------------------|------------------|---------------------|
| EventPlotWidget | ✅ | ✅ | ✅ |
| PSTHWidget | ✅ (aggregate) | ✅ | ❌ |
| HeatmapWidget | ✅ (aggregate) | ✅ (source) | ✅ (rows) |
| ScatterPlotWidget | ❌ | ❌ | ✅ |

---

## Implementation Priority

### MVP (Minimum Viable Product) ✅

1. ✅ **Signal Design**: Consolidated signals implemented in EventPlotState
2. ✅ **Phase 1**: Basic rendering with OpenGL + GatherResult
3. ✅ **Phase 2.1**: X/Y zoom mouse interaction (wheel, Shift+wheel, Ctrl+wheel)
4. **Phase 3.1-3.2**: Fixed color/glyph configuration in properties panel
5. ✅ **Phase 2.2**: Hit testing with single-click selection

### Iteration 2

6. **Phase 2.2-2.3**: Full selection (~~single~~, ~~multi~~, polygon) - single done, multi/polygon descoped
7. ✅ **Phase 2.4**: Double-click time navigation
8. **Phase 3.3**: Axis labels
9. ✅ **Phase 4 (Built-in)**: Trial sorting by first event latency and event count

### Iteration 3

10. **Phase 6**: Cross-widget linking (using existing SelectionContext)
11. **Phase 7**: Factor shared components

### Future (Design First)

12. **Phase 4 (External)**: External feature sorting via DataTransform v2
13. **Phase 5**: Feature-based coloring

---

## Testing Strategy

### Unit Tests

- `EventPlotState` serialization round-trip
- `GatherResult` integration with test data
- `RasterMapper::mapEventsRelative` output validation

### Integration Tests

- OpenGL widget initialization
- Mouse interaction → selection state
- Alignment change → data rebuild

### Visual Tests

- Glyph rendering at various sizes
- Zoom behavior preservation
- Selection highlighting

---

## Dependencies

### External

- Qt6 (OpenGL widgets)
- glm (vector math)
- reflect-cpp (serialization)

### Internal

- CorePlotting (RasterMapper, RowLayoutStrategy, SceneHitTester)
- PlottingOpenGL (SceneRenderer, ShaderManager)
- DataManager (DigitalEventSeries, GatherResult)
- EditorState infrastructure

---

## Open Questions

1. **Colormap location**: Should colormaps live in CorePlotting or a shared utility library?

2. **Transform pipeline storage**: How to serialize DataTransform v2 pipeline specs 
   in widget state for sorting/coloring?

3. **Performance at scale**: What's the target trial count? Need benchmarks for 
   10K+ trials with 100+ events each.

4. **Multi-unit raster**: Should EventPlotWidget support multiple units overlaid, 
   or is that Heatmap's role?

5. **Selection persistence**: Should selected EntityIds be saved in state for 
   workspace restoration?

6. **Registry reuse**: Should we create a generic `PlotOptionsRegistry<T>` template 
   that can be reused across EventPlot, PSTH, and Heatmap widgets?

---

## Appendix: Benchmark Reference

See [RasterPlotViews.benchmark.cpp](/benchmark/RasterPlotViews.benchmark.cpp) for:
- GatherResult usage patterns
- View-based vs baseline performance comparison
- GPU buffer population patterns
- Transform pipeline integration examples
