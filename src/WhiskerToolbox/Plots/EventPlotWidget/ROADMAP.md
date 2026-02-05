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
- ❌ No OpenGL rendering implementation
- ❌ No hit testing / selection
- ❌ No zoom/pan controls
- ❌ No trial sorting
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

## Signal/Slot Design: Consolidated Patterns

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

### Recommended Pattern: EventOptionsRegistry

Follow `SeriesOptionsRegistry` (DataViewer) and `DisplayOptionsRegistry` (MediaWidget):

```cpp
// EventOptionsRegistry.hpp
class EventOptionsRegistry {
public:
    explicit EventOptionsRegistry(EventPlotStateData* data, QObject* signal_parent);
    
    // Type-safe accessors
    void set(QString const& key, EventPlotOptions const& options);
    [[nodiscard]] EventPlotOptions const* get(QString const& key) const;
    void remove(QString const& key);
    
    // Query
    [[nodiscard]] QStringList keys() const;
    [[nodiscard]] QStringList visibleKeys() const;
    
signals:
    // Single signal for all option changes
    void optionsChanged(QString const& key);
    void optionsRemoved(QString const& key);
};
```

### Consolidated State Signals

```cpp
// EventPlotState.hpp signals section
signals:
    // === Category-consolidated signals ===
    
    /// Emitted when any alignment property changes
    void alignmentChanged();
    
    /// Emitted when any view state property changes (zoom, pan, bounds)
    void viewStateChanged();
    
    /// Emitted when event options change (forwarded from registry)
    void eventOptionsChanged(QString const& event_key);
    void eventOptionsRemoved(QString const& event_key);
    
    /// Emitted when global plot options change (axis labels, grid, etc.)
    void globalOptionsChanged();
    
    /// Emitted when sort configuration changes
    void sortConfigChanged();
    
    /// Emitted when color configuration changes
    void colorConfigChanged();
    
    /// Emitted when pin state changes
    void pinnedChanged(bool pinned);
    
    /// Generic catch-all (for serialization triggers)
    void stateChanged();
```

### View/Properties Widget Connection

```cpp
// In EventPlotOpenGLWidget
connect(_state.get(), &EventPlotState::alignmentChanged,
        this, &EventPlotOpenGLWidget::_rebuildGatheredData);
        
connect(_state.get(), &EventPlotState::viewStateChanged,
        this, &EventPlotOpenGLWidget::_updateViewTransform);
        
connect(_state.get(), &EventPlotState::eventOptionsChanged,
        this, [this](QString const& key) {
            _updateGlyphAppearance(key);
            update();  // Request repaint
        });

// In EventPlotPropertiesWidget
connect(_state.get(), &EventPlotState::eventOptionsChanged,
        this, &EventPlotPropertiesWidget::_refreshEventOptionsUI);
```

### Variant Alternative (MediaWidget Style)

If we need more flexibility, use a variant approach:

```cpp
using EventPlotOptionVariant = std::variant<
    EventPlotOptions,      // Per-event glyph options
    EventPlotViewState,    // Zoom, pan, bounds
    EventPlotSortConfig,   // Sorting settings
    EventPlotColorConfig   // Coloring settings
>;

// Single signal with variant payload
void optionChanged(QString const& key, EventPlotOptionVariant const& option);
```

**Recommendation**: Start with the registry pattern for per-event options (like DataViewer)
and consolidated category signals for other state. This balances simplicity and flexibility.

---

## Phase 1: Core Rendering Infrastructure

**Goal**: Establish OpenGL rendering pipeline using CorePlotting patterns.

### 1.1 Create EventPlotOpenGLWidget

Create an OpenGL widget following DataViewer's OpenGLWidget pattern:

```
EventPlotWidget/
├── Rendering/
│   ├── EventPlotOpenGLWidget.hpp
│   ├── EventPlotOpenGLWidget.cpp
│   ├── EventPlotSceneBuilder.hpp    # Wraps CorePlotting RasterMapper
│   └── EventPlotSceneBuilder.cpp
```

**Key Components**:
- `EventPlotOpenGLWidget`: QOpenGLWidget subclass
- Uses `PlottingOpenGL::SceneRenderer` for rendering
- Uses `CorePlotting::RasterMapper` for coordinate mapping
- Uses `CorePlotting::RowLayoutStrategy` for Y positioning

**Data Flow**:
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

### 1.2 Integrate GatherResult

Use `GatherResult<DigitalEventSeries>` from benchmark patterns:

```cpp
// In EventPlotOpenGLWidget
auto gathered = gather(_event_series, _alignment_intervals);

// Build scene with row layouts
for (size_t trial = 0; trial < gathered.size(); ++trial) {
    auto layout = RasterMapper::makeRowLayout(trial, gathered.size(), 
                                               trial_key, y_min, y_max);
    auto mapped = RasterMapper::mapEventsRelative(
        *gathered[trial], layout, *time_frame, 
        gathered.intervalAt(trial).start);
    // Add to scene builder...
}
```

### 1.3 State Extensions for View Settings

Add to `EventPlotStateData`:

```cpp
struct EventPlotViewState {
    // X-axis (time) bounds
    double x_min = -500.0;  // ms before alignment
    double x_max = 500.0;   // ms after alignment
    
    // Zoom factors (independent)
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    
    // Global glyph settings
    double glyph_size = 3.0;
};
```

**Files to modify**:
- [EventPlotState.hpp](Core/EventPlotState.hpp): Add view state struct
- [EventPlotStateData (in EventPlotState.hpp)]: Add serializable view settings

---

## Phase 2: Interaction - Zoom, Pan, Selection

**Goal**: Enable user interaction with the plot.

### 2.1 Separated X/Y Zoom

Implement independent zoom for time (X) and trial (Y) axes:

**Zoom Modes**:
- Mouse wheel: X-zoom only (time-focused exploration)
- Shift+wheel: Y-zoom only (trial-focused exploration)  
- Ctrl+wheel: Uniform zoom (both axes)

**Implementation**:
```cpp
// In input handler
void handleWheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ShiftModifier) {
        adjustYZoom(zoomFactor);
    } else if (event->modifiers() & Qt::ControlModifier) {
        adjustXZoom(zoomFactor);
        adjustYZoom(zoomFactor);
    } else {
        adjustXZoom(zoomFactor);  // Default: X only
    }
}
```

### 2.2 Hit Testing & Entity Selection

Use CorePlotting's SceneHitTester for point queries:

```cpp
// On mouse click
auto hit = _hit_tester.hitTest(world_x, world_y, _scene, _layout);
if (hit.hasHit() && hit.hit_type == HitType::DigitalEvent) {
    EntityId entity = hit.entity_id.value();
    _selection_manager.handleEntityClick(entity, ctrl_pressed);
}
```

**Selection Features**:
- Single click: Select event
- Ctrl+click: Multi-select
- Double-click: Navigate to event time (emit `timePositionSelected`)

### 2.3 Polygon Selection

Integrate `PolygonInteractionController` for lasso selection:

```cpp
// Interaction mode enum
enum class EventPlotInteractionMode {
    Normal,      // Pan, zoom, single-click select
    PolygonSelect  // Lasso selection mode
};
```

**State additions**:
```cpp
// In EventPlotStateData
std::string interaction_mode = "Normal";  // Serializable mode
```

### 2.4 Double-Click → Time Navigation

Emit signal to EditorRegistry for time synchronization:

```cpp
void EventPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    auto hit = hitTestAt(event->pos());
    if (hit.hasHit()) {
        // Get absolute time of the event
        TimePosition position = computeAbsoluteTime(hit, trial_index);
        emit timePositionSelected(position);
    }
}
```

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

## Phase 4: Trial Sorting (Design Exploration)

**Goal**: Allow trials to be reordered based on computed features.

> **Note**: This is an exploratory phase. The exact integration with DataTransform v2 
> needs further design work.

### 4.1 Sort Key Concept

```cpp
// Conceptual - needs refinement
struct TrialSortConfig {
    std::string sort_mode;  // "none", "computed", "group"
    
    // For computed sorting
    std::optional<std::string> sort_transform_key;  // DataManager key of transform output
    bool ascending = true;
    
    // For group sorting
    std::optional<std::string> group_key;  // EntityGroup in EntityRegistry
};
```

### 4.2 Integration with DataTransform v2

The sorting feature should leverage the DataTransform v2 architecture:

**Option A: Pre-computed sort values**
- User creates a transform that computes per-trial values
- Transform output is an AnalogTimeSeries where index = trial, value = sort key
- EventPlotWidget reads this series for ordering

**Option B: Inline transform specification** (future)
- EventPlotState stores a transform pipeline specification
- Widget executes pipeline at render time
- More flexible but higher complexity

### 4.3 GatherResult Sorting Support

`GatherResult` already has `reorder()` and `sortBy()` methods:

```cpp
// Sort by spike count (descending)
auto sorted = gathered.sortBy([](auto const& trial) {
    return -static_cast<int>(trial->size());  // Negative for descending
});
```

**Phase 4 Deliverable**: Document the design pattern without full implementation.
The sorting UI can have a placeholder that shows "Coming Soon" until the
DataTransform integration is finalized.

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

### MVP (Minimum Viable Product)

1. **Signal Design**: Define consolidated signals and EventOptionsRegistry pattern
2. **Phase 1.1-1.3**: Basic rendering with OpenGL + GatherResult
3. **Phase 2.1**: X/Y zoom
4. **Phase 3.1-3.2**: Fixed color/glyph configuration
5. **Phase 2.2**: Hit testing (no selection persistence yet)

### Iteration 2

6. **Phase 2.2-2.3**: Full selection (single, multi, polygon)
7. **Phase 2.4**: Double-click time navigation
8. **Phase 3.3**: Axis labels

### Iteration 3

9. **Phase 6**: Cross-widget linking (using existing SelectionContext)
10. **Phase 7**: Factor shared components

### Future (Design First)

11. **Phase 4**: Trial sorting
12. **Phase 5**: Feature-based coloring

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
