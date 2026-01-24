# DataViewer_Widget State Extraction Roadmap

## Executive Summary

This document outlines a phased approach to extract serializable state from the `DataViewer_Widget` and `OpenGLWidget` hierarchy, following the same patterns successfully used in the `MediaWidgetState` refactoring. The goal is to create a comprehensive `DataViewerState` that:

1. Can be serialized to JSON using reflect-cpp
2. Inherits from `EditorState` base class
3. Uses a generic registry pattern to prevent API/signal explosion
4. Serves as a single source of truth for widget properties

**Current State**: No state object exists. Display options are stored in `TimeSeriesDataStore`, view state in `OpenGLWidget`, and UI preferences scattered across both widgets.

**Target State**: Full serialization of all display options, view state, theme settings, and interaction preferences via a unified `DataViewerState` class.

## Design Decision: Separated Data vs. Display Options

### Pattern Comparison

After analyzing the `Media_Window` pattern (which separates data access via `DataManager` from display options via `MediaWidgetState`), we've chosen a **hybrid approach** for DataViewer:

| Aspect | Collocated (Current TimeSeriesDataStore) | Separated (Media_Window style) |
|--------|------------------------------------------|-------------------------------|
| **Lookup cost** | 1 lookup per series | 2 lookups per series |
| **Performance impact** | Negligible | Negligible (<0.1% frame budget) |
| **Serialization** | Must filter non-serializable fields | State is directly serializable |
| **Testability** | Harder to test options without data | Can test state independently |

### Performance Analysis

For a typical scenario (20 series at 60fps):
- String hash + map lookup: ~50-100 cycles
- Two lookups per series per frame: ~2 microseconds total
- Frame budget at 60fps: 16,600 microseconds
- **Overhead: 0.012% of frame budget** — negligible

### Chosen Architecture

**TimeSeriesDataStore** keeps:
- `shared_ptr` to actual data series
- Computed layout transforms (from LayoutEngine)
- Render caches (vertex buffers)

**DataViewerState** holds:
- User-configurable display options (serializable)
- View state, theme, grid settings
- UI preferences

This separation provides:
1. **Clean serialization** — State contains only what needs saving
2. **Clear ownership** — DataStore owns runtime data, State owns preferences
3. **Independent testing** — State can be tested without data series
4. **Follows Media_Window precedent** — Consistent with existing patterns

## Current Architecture Analysis

### State Locations (After Phase 5)

```
DataViewer_Widget/
├── DataViewer_Widget.hpp
│   ├── _state (shared_ptr<DataViewerState>)  # ✅ OWNS STATE, shares with OpenGLWidget
│   ├── _saved_splitter_sizes                 # QList<int> - Qt-specific, not serialized
│   ├── _highlighted_available_feature        # QString - transient
│   └── _is_batch_add                         # Transient flag
│
├── OpenGLWidget.hpp
│   ├── _state (shared_ptr<DataViewerState>)  # ✅ SHARED STATE from DataViewer_Widget
│   │   └── All view state, theme, grid, UI prefs now in state
│   ├── _time (TimeFrameIndex)                # Current time position
│   └── _spike_sorter_configs                 # SpikeSorterConfigMap
│
├── TimeSeriesDataStore.hpp (in DataViewer namespace)
│   ├── _analog_series                        # map<key, AnalogSeriesEntry>
│   │   └── display_options: NewAnalogTimeSeriesDisplayOptions  # ⏳ TODO: Move to state (Phase 6)
│   │       ├── style (SeriesStyle)           # hex_color, alpha, line_thickness, is_visible
│   │       ├── layout_transform              # offset, gain (computed by layout engine)
│   │       ├── data_cache                    # cached statistics
│   │       └── scaling, gap_handling, etc.   # analog-specific settings
│   │
│   ├── _digital_event_series                 # map<key, DigitalEventSeriesEntry>
│   │   └── display_options: NewDigitalEventSeriesDisplayOptions  # ⏳ TODO: Move to state (Phase 6)
│   │
│   └── _digital_interval_series              # map<key, DigitalIntervalSeriesEntry>
│       └── display_options: NewDigitalIntervalSeriesDisplayOptions  # ⏳ TODO: Move to state (Phase 6)
│
└── DataViewerInteractionManager.hpp
    └── _mode (InteractionMode)               # Normal, CreateInterval, ModifyInterval, CreateLine
```

### State Categories

| Category | Description | Serializable? | Current Location |
|----------|-------------|---------------|------------------|
| **View State** | Time window, Y bounds, zoom, pan | ✅ Yes | OpenGLWidget._view_state |
| **Theme/Grid** | Theme, colors, grid settings | ✅ Yes | OpenGLWidget._theme_state, _grid_state |
| **Display Options** | Per-series visual settings | ✅ Partial | TimeSeriesDataStore |
| **UI Preferences** | Panel state, zoom mode | ✅ Yes | DataViewer_Widget |
| **Interaction Mode** | Current tool mode | ✅ Yes | DataViewerInteractionManager |
| **Transient State** | Active drag, hover, batch flags | ❌ No | Various |

### Display Options Structure (Current)

The existing display options use CorePlotting's separated concerns pattern:

```cpp
// From NewAnalogTimeSeriesDisplayOptions
struct NewAnalogTimeSeriesDisplayOptions {
    CorePlotting::SeriesStyle style;           // User-configurable visual style
    CorePlotting::LayoutTransform layout_transform;  // Computed by LayoutEngine
    CorePlotting::SeriesDataCache data_cache;  // Cached statistics
    // ... analog-specific fields
};
```

Key insight: `style` fields are serializable; `layout_transform` and `data_cache` are computed/transient.

### Target Architecture: Separated Concerns

After refactoring, responsibilities will be split:

```cpp
// TimeSeriesDataStore (Runtime — NOT serialized)
struct AnalogSeriesEntry {
    std::shared_ptr<AnalogTimeSeries> series;      // Data
    CorePlotting::LayoutTransform layout_transform; // Computed by LayoutEngine
    CorePlotting::SeriesDataCache data_cache;       // Cached statistics  
    AnalogVertexCache vertex_cache;                 // Render cache
    // NO style/user options — those are in DataViewerState
};

// DataViewerState (Serializable)
struct AnalogSeriesOptionsData {
    std::string hex_color = "#007bff";
    float alpha = 1.0f;
    int line_thickness = 1;
    bool is_visible = true;
    float user_scale_factor = 1.0f;
    // ... user-configurable fields only
};
```

---

## Target Architecture

```
DataViewerState (EditorState)
├── _data: DataViewerStateData               // Single serializable struct
├── _series_options: SeriesOptionsRegistry   // Generic for all 3 series types
├── View state accessors (8 methods)         // Time window, Y bounds, zoom, pan
├── Theme accessors (4 methods)              // Theme, background, axis color
├── Grid accessors (3 methods)               // Enabled, spacing
├── UI preference accessors (4 methods)      // Panel state, zoom mode
├── Interaction mode accessors (2 methods)   // Get/set mode
└── toJson() / fromJson()                    // reflect-cpp serialization

TimeSeriesDataStore (Runtime, NOT serialized)
├── _analog_series: map<key, AnalogSeriesEntry>
│   └── series, layout_transform, data_cache, vertex_cache
├── _digital_event_series: map<key, DigitalEventSeriesEntry>
│   └── series, layout_transform
├── _digital_interval_series: map<key, DigitalIntervalSeriesEntry>
│   └── series, layout_transform
└── Layout integration methods
```

### Rendering Flow (Post-Refactor)

```cpp
void OpenGLWidget::renderAnalogSeries() {
    for (auto& [key, entry] : _data_store->analogSeries()) {
        // Get user options from state (serializable)
        auto* opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(key);
        if (!opts || !opts->is_visible) continue;
        
        // Combine: data from store, style from state, layout from store
        renderSeries(entry.series.get(), entry.layout_transform, *opts);
    }
}
```

### SeriesOptionsRegistry Design

Following MediaWidget's `DisplayOptionsRegistry` pattern:

```cpp
class SeriesOptionsRegistry : public QObject {
    Q_OBJECT
public:
    // Generic type-safe API (3 types instead of 6)
    template<typename T> void set(QString const& key, T const& options);
    template<typename T> T const* get(QString const& key) const;
    template<typename T> T* getMutable(QString const& key);
    template<typename T> bool remove(QString const& key);
    template<typename T> bool has(QString const& key) const;
    template<typename T> QStringList keys() const;
    template<typename T> QStringList visibleKeys() const;

signals:
    void optionsChanged(QString const& key, QString const& type_name);
    void optionsRemoved(QString const& key, QString const& type_name);
    void visibilityChanged(QString const& key, QString const& type_name, bool visible);
    
private:
    DataViewerStateData* _data;  // Non-owning
};
```

### Serializable Display Options

State holds only user-configurable options (no computed fields):

```cpp
// Serializable analog series options (user-configurable only)
struct AnalogSeriesOptionsData {
    // Visual style (from SeriesStyle)
    std::string hex_color{"#007bff"};
    float alpha{1.0f};
    int line_thickness{1};
    bool is_visible{true};
    
    // Analog-specific user settings
    float user_scale_factor{1.0f};
    float y_offset{0.0f};
    std::string gap_handling{"AlwaysConnect"};  // Enum as string
    bool enable_gap_detection{false};
    float gap_threshold{5.0f};
    
    // NOT included (computed at runtime):
    // - layout_transform (from LayoutEngine)
    // - data_cache (intrinsic_scale, mean, std_dev)
    // - vertex_cache
};
```

---

## Phase 1: Define Serializable State Data Structures

**Goal**: Create reflect-cpp compatible state structs for DataViewer

**Estimated Duration**: 2-3 days

### Step 1.1: Create SeriesStyleData (shared base)

### Step 1.2: Create per-series-type options structs

### Step 1.3: Create ViewState struct

### Step 1.4: Create ThemeState and GridState structs

### Step 1.5: Create UIPreferences struct


### Step 1.7: Create main DataViewerStateData struct

### Files to Create

- `src/WhiskerToolbox/DataViewer_Widget/DataViewerStateData.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerStateData.test.cpp`

### Success Criteria

- [x] All structs compile and serialize to JSON correctly ✅ (2025-01-XX)
- [x] Round-trip serialization tests pass ✅ (173 assertions, 11 test cases)
- [x] Enum values serialize as strings ✅
- [x] JSON structure is flat where appropriate (via `rfl::Flatten`) ✅

### Implementation Notes

**Phase 1 COMPLETED**

Files created:
- [DataViewerStateData.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewerStateData.hpp) - All serializable state structs
- [DataViewerStateData.test.cpp](../../../tests/WhiskerToolbox/DataViewer_Widget/DataViewerStateData.test.cpp) - Comprehensive unit tests

Key design decisions:
1. Used `rfl::Flatten<SeriesStyleData>` for flat JSON structure (no "style" nesting)
2. All enums serialize as strings for human-readable JSON
3. Separated user-configurable options from computed fields (layout_transform, data_cache)
4. Maps keyed by series data key for per-series options

---

## Phase 2: Create SeriesOptionsRegistry

**Goal**: Create a type-safe generic registry for series display options

**Estimated Duration**: 3-4 days

### Step 2.1: Create SeriesOptionsRegistry class

### Step 2.2: Implement template specializations

Each series type gets explicit template specializations:


### Step 2.3: Unit tests

- set/get round-trip for all 3 types
- remove functionality
- keys() and visibleKeys()
- Signal emission verification
- Type safety verification

### Files to Create

- `src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.cpp`
- `tests/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.test.cpp`

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt`
- `tests/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt`

### Success Criteria

- [x] Type-safe template API works for all 3 series types ✅
- [x] Signals emitted correctly (optionsChanged, optionsRemoved, visibilityChanged) ✅
- [x] Non-template visibility methods work for runtime type dispatch ✅
- [x] Unit tests pass (93 assertions in 9 test cases) ✅

### Implementation Notes

**Phase 2 COMPLETED**

Files created:
- [SeriesOptionsRegistry.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.hpp) - Type-safe registry class
- [SeriesOptionsRegistry.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.cpp) - Template specializations
- [SeriesOptionsRegistry.test.cpp](../../../tests/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.test.cpp) - Comprehensive unit tests

Key implementation details:
1. Follows same pattern as `DisplayOptionsRegistry` in Media_Widget
2. 3 series types: Analog, DigitalEvent, DigitalInterval (vs 6 in MediaWidget)
3. Non-owning pointer to `DataViewerStateData` for direct map access
4. QSignalSpy-based signal verification in tests
5. Complete type isolation between series types (same key can exist for different types)

---

## Phase 3: Create DataViewerState Class

**Goal**: Create the EditorState subclass with typed accessors and signals

**Estimated Duration**: 3-4 days

### Step 3.1: Create DataViewerState class declaration

### Step 3.2: Implement DataViewerState

Implement all methods with appropriate signal emission.

### Step 3.3: Unit tests

- Serialization round-trip
- Signal emission for all setters
- Registry integration

### Files to Create

- `src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerState.cpp`
- `tests/WhiskerToolbox/DataViewer_Widget/DataViewerState.test.cpp`

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt`
- `tests/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt`

### Success Criteria

- [x] EditorState interface implemented (getTypeName, toJson, fromJson) ✅
- [x] All typed accessors for view, theme, grid, UI, interaction ✅
- [x] Consolidated signals (viewStateChanged, themeChanged, etc.) ✅
- [x] Registry signals forwarded to state signals ✅
- [x] Serialization round-trip preserves all state ✅
- [x] Unit tests pass (86 assertions in 10 test cases) ✅

### Implementation Notes

**Phase 3 COMPLETED**

Files created:
- [DataViewerState.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp) - EditorState subclass (~430 lines)
- [DataViewerState.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewerState.cpp) - Implementation (~290 lines)
- [DataViewerState.test.cpp](../../../tests/WhiskerToolbox/DataViewer_Widget/DataViewerState.test.cpp) - Comprehensive unit tests (~400 lines)

Key implementation details:
1. Inherits from EditorState base class
2. Follows same pattern as MediaWidgetState
3. Uses SeriesOptionsRegistry for per-series options (connected via signals)
4. Consolidated signals reduce signal count (~10 signals instead of per-property)
5. All setters use epsilon comparison for floats, emit signals only on change
6. fromJson() emits all appropriate signals for UI refresh

---

## Phase 4: Wire State to OpenGLWidget

**Goal**: OpenGLWidget uses state for view/theme/grid instead of local members

**Estimated Duration**: 3-4 days

### Step 4.1: Add state pointer to OpenGLWidget ✅

OpenGLWidget now holds a `shared_ptr<DataViewerState>` and connects to state signals:

```cpp
// OpenGLWidget.hpp
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    void setState(std::shared_ptr<DataViewerState> state);
    [[nodiscard]] std::shared_ptr<DataViewerState> state() const { return _state; }
    
private:
    std::shared_ptr<DataViewerState> _state;
};
```

**Implementation Notes:**
- State signals connected in constructor and `setState()`
- `viewStateChanged` → marks layout/scene dirty + calls `update()`
- `themeChanged`, `gridChanged` → calls `update()`
- State created in OpenGLWidget constructor if not provided

### Step 4.2: Move time window logic to state layer ✅

**COMPLETED (2026-01-23)**: Time window adjustment logic moved from OpenGLWidget to state:

**Added to TimeSeriesViewState** ([TimeRange.hpp](../../../src/CorePlotting/CoordinateTransform/TimeRange.hpp)):
```cpp
// Adjust width while preserving center
void adjustTimeWidth(int64_t delta);

// Set absolute width while preserving center  
int64_t setTimeWidth(int64_t width);
```

**Added to DataViewerState** ([DataViewerState.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp)):
```cpp
void adjustTimeWidth(int64_t delta);        // Emits viewStateChanged
int64_t setTimeWidth(int64_t width);        // Emits viewStateChanged
```

**Removed from OpenGLWidget**:
- `changeRangeWidth(int64_t range_delta)` - was a pass-through
- `setRangeWidth(int64_t range_width)` - was a pass-through

**Updated DataViewer_Widget** to call state directly:
```cpp
// Mouse wheel zoom
ui->openGLWidget->state()->adjustTimeWidth(range_delta);

// Spinbox width change
ui->openGLWidget->state()->setTimeWidth(value);
```

**Architectural Principle Enforced**: 
- UI modifies state directly
- State emits `viewStateChanged` signal
- OpenGLWidget reacts to signal and calls `update()`
- No intermediate pass-through methods in rendering widget

### Step 4.3: Wire theme and grid state (IN PROGRESS)

Theme and grid settings now go through state:

**DataViewer_Widget changes**:
```cpp
// Theme
ui->openGLWidget->state()->setTheme(theme);

// Grid
ui->openGLWidget->state()->setGridEnabled(enabled);
ui->openGLWidget->state()->setGridSpacing(spacing);

// Global zoom
ui->openGLWidget->state()->setGlobalZoom(scale);
```

**Removed from OpenGLWidget**:
- `setPlotTheme()` - theme goes through state
- `setGridLinesEnabled()` - grid goes through state
- `setGridSpacing()` - grid goes through state
- `setGlobalScale()` - zoom goes through state

### Step 4.4: Connect state signals ✅

State signals fully connected in OpenGLWidget constructor:

```cpp
connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
    _cache_state.layout_response_dirty = true;
    _cache_state.scene_dirty = true;
    update();
});
connect(_state.get(), &DataViewerState::themeChanged, this, [this]() {
    update();
});
connect(_state.get(), &DataViewerState::gridChanged, this, [this]() {
    update();
});
```

### Files Modified

- [src/CorePlotting/CoordinateTransform/TimeRange.hpp](../../../src/CorePlotting/CoordinateTransform/TimeRange.hpp) - Added time width methods
- [src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp) - Added time width methods
- [src/WhiskerToolbox/DataViewer_Widget/DataViewerState.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewerState.cpp) - Implementation
- [src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.hpp) - Removed pass-throughs, added state()
- [src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp) - Removed implementations
- [src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.hpp) - Removed PlotTheme forward decl
- [src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp) - Call state directly

### Success Criteria

- [x] State pointer in OpenGLWidget with setState()/state() accessors ✅
- [x] Time window adjustments go through state (adjustTimeWidth, setTimeWidth) ✅
- [x] Theme changes go through state (setTheme) ✅
- [x] Grid changes go through state (setGridEnabled, setGridSpacing) ✅
- [x] Global zoom goes through state (setGlobalZoom) ✅
- [x] State signals trigger OpenGLWidget updates ✅
- [x] All DataViewer widget tests pass ✅
- [x] Vertical spacing removed (layout engine handles spacing automatically) ✅
- [x] setYBounds and setVerticalPanOffset already in state and used correctly ✅

### Implementation Notes

**Phase 4 COMPLETED (2026-01-23)**

- All view state, theme, grid settings now flow through DataViewerState
- OpenGLWidget reacts to state signals and triggers updates
- No more pass-through methods in OpenGLWidget
- y_spacing removed from codebase (layout engine handles this automatically)

---

## Phase 5: Wire State to DataViewer_Widget

**Goal**: DataViewer_Widget uses state for UI preferences

**Estimated Duration**: 2-3 days

### Step 5.1: Add state to DataViewer_Widget ✅

```cpp
// DataViewer_Widget.hpp
class DataViewer_Widget : public QWidget {
public:
    void setState(std::shared_ptr<DataViewerState> state);
    [[nodiscard]] std::shared_ptr<DataViewerState> state() const { return _state; }
    
private:
    std::shared_ptr<DataViewerState> _state;
};
```

### Step 5.2: Wire UI preferences ✅

- `_zoom_scaling_mode` → `_state->zoomScalingMode()`
- `_properties_panel_collapsed` → `_state->propertiesPanelCollapsed()`

### Step 5.3: Pass state to OpenGLWidget ✅

```cpp
DataViewer_Widget::DataViewer_Widget(...) {
    _state = std::make_shared<DataViewerState>();
    ui->openGLWidget->setState(_state);
}
```

### Files Modified

- [src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.hpp) - Added state, removed local members
- [src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp) - Create state, share with OpenGLWidget, use state for all settings

### Success Criteria

- [x] State pointer in DataViewer_Widget with setState()/state() accessors ✅
- [x] DataViewer_Widget creates state and shares with OpenGLWidget ✅
- [x] `_zoom_scaling_mode` replaced with `_state->zoomScalingMode()` ✅
- [x] `_properties_panel_collapsed` replaced with `_state->propertiesPanelCollapsed()` ✅
- [x] All state modifications go through `_state` (not `ui->openGLWidget->state()`) ✅
- [x] setState() restores UI from state for workspace loading ✅
- [x] All tests pass (879 assertions in 59 test cases) ✅

### Implementation Notes

**Phase 5 COMPLETED (2026-01-23)**

Key changes:
1. DataViewer_Widget now owns the state and creates it in constructor
2. State is shared with OpenGLWidget via `setState()` call
3. Removed `_zoom_scaling_mode` local member (now uses `_state->zoomScalingMode()`)
4. Removed `_properties_panel_collapsed` local member (now uses `_state->propertiesPanelCollapsed()`)
5. Removed duplicate `ZoomScalingMode` enum from header (uses `DataViewerZoomScalingMode` from state)
6. All UI-to-state modifications use `_state->` directly, not `ui->openGLWidget->state()`
7. `setState()` method restores UI elements from state (for workspace restore)

---

## Phase 6: Refactor TimeSeriesDataStore

**Goal**: Remove display options from TimeSeriesDataStore; state becomes source of truth for user options

**Estimated Duration**: 4-5 days

**Status**: ✅ **COMPLETED (2026-01-24)**

This phase separates concerns between runtime data and serializable options.

### Step 6.1: Refactor TimeSeriesDataStore entry structs ✅

Remove style/options fields, keep only data and computed state:

```cpp
// Before
struct AnalogSeriesEntry {
    std::shared_ptr<AnalogTimeSeries> series;
    std::unique_ptr<NewAnalogTimeSeriesDisplayOptions> display_options;  // REMOVE
    mutable AnalogVertexCache vertex_cache;
};

// After
struct AnalogSeriesEntry {
    std::shared_ptr<AnalogTimeSeries> series;
    CorePlotting::LayoutTransform layout_transform;  // Computed
    CorePlotting::SeriesDataCache data_cache;        // Computed  
    mutable AnalogVertexCache vertex_cache;          // Render cache
    // Style comes from DataViewerState at render time
};
```

### Step 6.2: Update add/remove methods ✅

When adding a series:
1. TimeSeriesDataStore stores data + initializes computed fields
2. DataViewerState creates default options (if not present)

```cpp
void OpenGLWidget::addAnalogTimeSeries(std::string const& key, 
                                        std::shared_ptr<AnalogTimeSeries> series,
                                        std::string const& color) {
    // Add to data store (no options)
    _data_store->addAnalogSeries(key, series);
    
    // Create default options in state if not present
    if (!_state->seriesOptions().has<AnalogSeriesOptionsData>(QString::fromStdString(key))) {
        AnalogSeriesOptionsData opts;
        opts.hex_color = color.empty() ? DefaultColors::getColorForIndex(count) : color;
        opts.is_visible = true;
        _state->seriesOptions().set(QString::fromStdString(key), opts);
    }
}
```

### Step 6.3: Update rendering to combine data + options ✅

```cpp
void OpenGLWidget::renderAnalogSeries() {
    for (auto const& [key, entry] : _data_store->analogSeries()) {
        auto* opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(
            QString::fromStdString(key));
        
        if (!opts || !opts->is_visible) continue;
        
        // Build SeriesStyle from serializable options
        CorePlotting::SeriesStyle style;
        style.hex_color = opts->hex_color;
        style.alpha = opts->alpha;
        style.line_thickness = opts->line_thickness;
        style.is_visible = opts->is_visible;
        
        renderAnalogWithStyle(entry, style, *opts);
    }
}
```

### Step 6.4: Update layout application ✅

Layout engine results go to TimeSeriesDataStore entries, not to state:

```cpp
void TimeSeriesDataStore::applyLayoutResponse(CorePlotting::LayoutResponse const& response) {
    for (auto const& [key, layout] : response.series_layouts) {
        if (auto it = _analog_series.find(key); it != _analog_series.end()) {
            it->second.layout_transform = layout.transform;
        }
        // ... similar for events and intervals
    }
}
```

### Step 6.5: Handle key synchronization ✅

When a series is removed from DataStore, also remove from state:

```cpp
void OpenGLWidget::removeAnalogTimeSeries(std::string const& key) {
    _data_store->removeAnalogSeries(key);
    _state->seriesOptions().remove<AnalogSeriesOptionsData>(QString::fromStdString(key));
}
```

### Files Modified

- [src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.hpp) - Removed display_options from entries
- [src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.cpp) - Updated add/remove methods, buildLayoutRequest
- [src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp) - Updated rendering to use state for style
- [src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp) - Updated color/visibility handling
- [src/WhiskerToolbox/DataViewer_Widget/SVGExporter.hpp](../../../src/WhiskerToolbox/DataViewer_Widget/SVGExporter.hpp) - Rewritten to take separate parameters
- [src/WhiskerToolbox/DataViewer_Widget/SVGExporter.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/SVGExporter.cpp) - Updated to use state for visibility
- [src/WhiskerToolbox/DataViewer_Widget/AnalogViewer_Widget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/AnalogViewer_Widget.cpp) - Fixed accessor patterns
- [src/WhiskerToolbox/DataViewer_Widget/EventViewer_Widget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/EventViewer_Widget.cpp) - Fixed accessor patterns
- [src/WhiskerToolbox/DataViewer_Widget/IntervalViewer_Widget.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/IntervalViewer_Widget.cpp) - Fixed accessor patterns
- [src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.test.cpp](../../../src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.test.cpp) - Updated all tests

### Success Criteria

- [x] TimeSeriesDataStore entries contain only data + computed state (layout_transform, data_cache, vertex_cache) ✅
- [x] User-configurable options moved to DataViewerState ✅
- [x] Rendering combines data from store with options from state ✅
- [x] Layout engine results applied to store entries, not state ✅
- [x] Key synchronization between store and state ✅
- [x] All tests passing (840 assertions in 59 test cases) ✅

### Implementation Notes

**Phase 6 COMPLETED (2026-01-24)**

Key changes:
1. **TimeSeriesDataStore refactored**: Removed `display_options` from all entry structs (AnalogSeriesEntry, DigitalEventSeriesEntry, DigitalIntervalSeriesEntry)
2. **Entry structs now contain**: 
   - Data: `shared_ptr<Series>` to actual time series
   - Computed: `layout_transform` (from LayoutEngine), `data_cache` (intrinsic_scale, etc.), `vertex_cache`
   - **NO user options**: color, visibility, user_scale_factor, etc. now in state
3. **Two-lookup pattern at render time**:
   - Data from `TimeSeriesDataStore.getAnalogSeriesMap()[key]`
   - Options from `DataViewerState.seriesOptions().get<AnalogSeriesOptionsData>(key)`
4. **Accessor patterns**:
   - Use `getMutable<T>()` for modification, `get<T>()` for const access
   - Style fields use accessor methods: `hex_color()`, `alpha()`, `get_is_visible()`
   - Non-style fields: direct member access (`user_scale_factor`, `y_offset`, etc.)
5. **Test infrastructure**: Created `TestHelpers` namespace with helper functions for accessing options and layout transforms
6. **SVGExporter rewritten**: Now takes separate layout_transform, data_cache, and options parameters
7. **All viewer widgets updated**: AnalogViewer_Widget, EventViewer_Widget, IntervalViewer_Widget now use `getMutable<>()` pattern

**Architectural Win**: Complete separation of concerns:
- **Runtime/Computed** (TimeSeriesDataStore): data, layout, cache — NOT serialized
- **User Preferences** (DataViewerState): style, visibility, user settings — serialized to JSON

---

## Phase 7: Final Cleanup and Testing

**Goal**: Remove duplicate state, comprehensive testing

**Estimated Duration**: 2-3 days

### Step 7.1: Audit remaining duplicate state

Review all members in OpenGLWidget and DataViewer_Widget that are now in state.

### Step 7.2: Integration tests

- Serialize → deserialize → verify all settings restored
- Multiple DataViewer instances with independent state
- State modification from external source (future properties panel)

### Step 7.3: Documentation

- Update widget documentation
- Add architecture diagram

---

## Summary Timeline

| Phase | Duration | Status | Deliverables |
|-------|----------|--------|--------------|
| 1: State Data Structures | 2-3 days | ✅ **COMPLETED** | DataViewerStateData.hpp, unit tests |
| 2: SeriesOptionsRegistry | 3-4 days | ✅ **COMPLETED** | Generic registry, template specializations |
| 3: DataViewerState Class | 3-4 days | ✅ **COMPLETED** | EditorState subclass, accessors, signals |
| 4: Wire OpenGLWidget | 3-4 days | ✅ **COMPLETED** | View/theme/grid from state, y_spacing removed |
| 5: Wire DataViewer_Widget | 2-3 days | ✅ **COMPLETED** | UI preferences from state, shared state |
| 6: Refactor TimeSeriesDataStore | 4-5 days | ✅ **COMPLETED** | Remove options, state as source of truth |
| 7: Cleanup & Testing | 2-3 days | ⏳ **PENDING** | Integration tests, documentation |
| **Total** | **~3-4 weeks** | **~85% Complete** | **Full state extraction** |

### Recent Progress (2026-01-24)

**Phase 6 Completed**: TimeSeriesDataStore refactored, state is now single source of truth

**Phase 6 Summary:**
- Removed display_options from all TimeSeriesDataStore entry structs
- TimeSeriesDataStore now contains only: data, layout_transform, data_cache, vertex_cache
- User-configurable options (color, visibility, scaling) moved to DataViewerState
- Two-lookup pattern at render time: data from store, options from state
- Created TestHelpers namespace for test infrastructure
- Fixed accessor patterns: `getMutable<>()` for modification, `get<>()` for const access
- SVGExporter rewritten to take separate parameters
- All viewer widgets updated (AnalogViewer_Widget, EventViewer_Widget, IntervalViewer_Widget)
- All tests passing (840 assertions in 59 test cases)

**Cumulative Architectural Wins (Phases 4-6):**
1. **Complete state extraction**: All user preferences in serializable DataViewerState
2. **Clean separation**: Runtime data (store) vs user options (state)
3. **Single source of truth**: DataViewer_Widget owns state, OpenGLWidget shares it
4. **UI → State → Render**: UI modifies state, rendering reads from state
5. **No duplicate state**: Eliminated all duplicate members between widgets
6. **Workspace ready**: Full state serialization enables save/restore

**Next Steps (Phase 7):**
- Integration testing for workspace save/restore
- Performance testing to confirm <0.1% overhead from two-lookup pattern
- Documentation updates
- Audit for any remaining duplicate state

---

## Files Summary

### New Files

| File | Description |
|------|-------------|
| `DataViewerStateData.hpp` | Serializable state struct definitions |
| `DataViewerStateData.test.cpp` | Serialization unit tests |
| `SeriesOptionsRegistry.hpp` | Generic type-safe registry |
| `SeriesOptionsRegistry.cpp` | Template specializations |
| `SeriesOptionsRegistry.test.cpp` | Registry unit tests |
| `DataViewerState.hpp` | EditorState subclass |
| `DataViewerState.cpp` | State implementation |
| `DataViewerState.test.cpp` | State unit tests |

### Modified Files

| File | Changes |
|------|---------|
| `CMakeLists.txt` | Add new source files |
| `OpenGLWidget.hpp` | Add state pointer, combine data+options in render |
| `OpenGLWidget.cpp` | Use state for style, store for data/layout |
| `DataViewer_Widget.hpp` | Add state ownership |
| `DataViewer_Widget.cpp` | Wire state, UI prefs from state |
| `TimeSeriesDataStore.hpp` | Remove display_options from entries |
| `TimeSeriesDataStore.cpp` | Update add/remove, layout application |
| `SceneBuildingHelpers.cpp` | Build style from state options |

---

## Success Criteria

1. **Serialization**: DataViewerState serializes/deserializes correctly via reflect-cpp
2. **API Control**: SeriesOptionsRegistry provides unified API for 3 series types (~20 methods instead of ~60)
3. **Signal Control**: ~10 consolidated signals instead of per-property signals
4. **State Restoration**: Opening a saved workspace restores all DataViewer settings
5. **Separation of Concerns**: 
   - TimeSeriesDataStore: data + computed state (layout, cache)
   - DataViewerState: user options (serializable)
6. **Independence**: Multiple DataViewer widgets can have independent state
7. **Performance**: No measurable impact from two-lookup pattern (<0.1% frame budget)

---

## Appendix: Comparison with MediaWidgetState

| Aspect | MediaWidgetState | DataViewerState |
|--------|------------------|-----------------|
| Series types | 6 (line, mask, point, tensor, interval, media) | 3 (analog, event, interval) |
| Registry name | DisplayOptionsRegistry | SeriesOptionsRegistry |
| View state | Viewport (zoom, pan, canvas size) | TimeSeriesViewState (time window, Y bounds, global scale) |
| Interaction prefs | Line/Mask/Point prefs | Not applicable (simpler tool modes) |
| Text overlays | Yes | No |
| Tool modes | Line/Mask/Point modes | InteractionMode enum |
| Theme | Not in state | In state (dark/light theme) |
| Grid | Not in state | In state (enabled, spacing) |
| Data access | Via DataManager (lookup by key) | Via TimeSeriesDataStore (lookup by key) |
| Options storage | State only (no duplication) | State only (no duplication) |

Both patterns follow the same principle: **data is accessed by key at render time, options are stored in state**. The ~0.01% frame overhead from two lookups is negligible compared to the architectural benefits.

## Appendix: Key Synchronization Strategy

Since data lives in TimeSeriesDataStore and options live in DataViewerState, key synchronization is important:

```
Add Series Flow:
1. DataStore.addAnalogSeries(key, data)
2. State.seriesOptions().set<AnalogSeriesOptionsData>(key, default_opts)

Remove Series Flow:
1. DataStore.removeAnalogSeries(key)
2. State.seriesOptions().remove<AnalogSeriesOptionsData>(key)

Load Workspace Flow:
1. State.fromJson(saved_json)  // Restores options
2. For each key in state.seriesOptions():
   - If DataManager has matching data, add to DataStore
   - Else mark option as "orphaned" (data not loaded yet)

Save Workspace Flow:
1. State already has current options (no sync needed)
2. state.toJson() → save
```

This ensures options persist even if data isn't currently loaded, enabling partial workspace restoration.
