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

### State Locations

```
DataViewer_Widget/
├── DataViewer_Widget.hpp
│   ├── _zoom_scaling_mode                    # ZoomScalingMode enum
│   ├── _properties_panel_collapsed           # bool
│   ├── _saved_splitter_sizes                 # QList<int>
│   ├── _highlighted_available_feature        # QString
│   └── _is_batch_add                         # Transient flag
│
├── OpenGLWidget.hpp
│   ├── _view_state (TimeSeriesViewState)     # Time window, Y bounds, global scale
│   │   ├── time_start, time_end              # X-axis visible range
│   │   ├── y_min, y_max                      # Y-axis viewport bounds
│   │   ├── vertical_pan_offset               # Y-axis pan offset
│   │   ├── global_zoom                       # Global amplitude scale
│   │   └── global_vertical_scale             # Global vertical scale
│   │
│   ├── _theme_state (ThemeState)             # Theme and colors
│   │   ├── theme                             # PlotTheme enum (Dark/Light)
│   │   ├── background_color                  # std::string hex
│   │   └── axis_color                        # std::string hex
│   │
│   ├── _grid_state (GridState)               # Grid overlay settings
│   │   ├── enabled                           # bool
│   │   └── spacing                           # int
│   │
│   ├── _ySpacing                             # float - vertical spacing factor
│   ├── _time (TimeFrameIndex)                # Current time position
│   └── _spike_sorter_configs                 # SpikeSorterConfigMap
│
├── TimeSeriesDataStore.hpp (in DataViewer namespace)
│   ├── _analog_series                        # map<key, AnalogSeriesEntry>
│   │   └── display_options: NewAnalogTimeSeriesDisplayOptions
│   │       ├── style (SeriesStyle)           # hex_color, alpha, line_thickness, is_visible
│   │       ├── layout_transform              # offset, gain (computed by layout engine)
│   │       ├── data_cache                    # cached statistics
│   │       └── scaling, gap_handling, etc.   # analog-specific settings
│   │
│   ├── _digital_event_series                 # map<key, DigitalEventSeriesEntry>
│   │   └── display_options: NewDigitalEventSeriesDisplayOptions
│   │       ├── style (SeriesStyle)
│   │       ├── layout_transform
│   │       └── plotting_mode, event_height, etc.
│   │
│   └── _digital_interval_series              # map<key, DigitalIntervalSeriesEntry>
│       └── display_options: NewDigitalIntervalSeriesDisplayOptions
│           ├── style (SeriesStyle)
│           ├── layout_transform
│           └── extend_full_canvas, margin_factor, etc.
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

```cpp
// DataViewerStateData.hpp
struct SeriesStyleData {
    std::string hex_color = "#007bff";
    float alpha = 1.0f;
    int line_thickness = 1;
    bool is_visible = true;
};
```

### Step 1.2: Create per-series-type options structs

```cpp
struct AnalogSeriesOptionsData {
    rfl::Flatten<SeriesStyleData> style;
    
    // Analog-specific user settings (exclude layout_transform, data_cache)
    float user_scale_factor = 1.0f;
    float y_offset = 0.0f;
    std::string gap_handling = "AlwaysConnect";  // Enum as string
    bool enable_gap_detection = false;
    float gap_threshold = 5.0f;
};

struct DigitalEventSeriesOptionsData {
    rfl::Flatten<SeriesStyleData> style;
    
    std::string plotting_mode = "FullCanvas";  // Enum as string
    float vertical_spacing = 0.1f;
    float event_height = 0.05f;
    float margin_factor = 0.95f;
};

struct DigitalIntervalSeriesOptionsData {
    rfl::Flatten<SeriesStyleData> style;
    
    bool extend_full_canvas = true;
    float margin_factor = 0.95f;
    float interval_height = 1.0f;
};
```

### Step 1.3: Create ViewState struct

```cpp
struct DataViewerViewState {
    // Time window (X-axis)
    int64_t time_start = 0;
    int64_t time_end = 1000;
    
    // Y-axis
    float y_min = -1.0f;
    float y_max = 1.0f;
    float vertical_pan_offset = 0.0f;
    
    // Global scaling
    float global_zoom = 1.0f;
    float global_vertical_scale = 1.0f;
    
    // Vertical spacing
    float y_spacing = 0.1f;
};
```

### Step 1.4: Create ThemeState and GridState structs

```cpp
enum class DataViewerTheme {
    Dark,
    Light
};

struct DataViewerThemeState {
    std::string theme = "Dark";  // Enum as string for serialization
    std::string background_color = "#000000";
    std::string axis_color = "#FFFFFF";
};

struct DataViewerGridState {
    bool enabled = false;
    int spacing = 100;
};
```

### Step 1.5: Create UIPreferences struct

```cpp
enum class DataViewerZoomScalingMode {
    Fixed,
    Adaptive
};

struct DataViewerUIPreferences {
    std::string zoom_scaling_mode = "Adaptive";  // Enum as string
    bool properties_panel_collapsed = false;
    // Note: splitter sizes not serialized (Qt-specific, layout-dependent)
};
```

### Step 1.6: Create InteractionState struct

```cpp
struct DataViewerInteractionState {
    std::string interaction_mode = "Normal";  // InteractionMode as string
};
```

### Step 1.7: Create main DataViewerStateData struct

```cpp
struct DataViewerStateData {
    // Identity
    std::string instance_id;
    std::string display_name = "Data Viewer";
    
    // View state
    DataViewerViewState view;
    
    // Theme and grid
    DataViewerThemeState theme;
    DataViewerGridState grid;
    
    // UI preferences
    DataViewerUIPreferences ui;
    
    // Interaction
    DataViewerInteractionState interaction;
    
    // Per-series display options (key -> options)
    std::map<std::string, AnalogSeriesOptionsData> analog_options;
    std::map<std::string, DigitalEventSeriesOptionsData> event_options;
    std::map<std::string, DigitalIntervalSeriesOptionsData> interval_options;
};
```

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

```cpp
// SeriesOptionsRegistry.hpp
class SeriesOptionsRegistry : public QObject {
    Q_OBJECT
public:
    explicit SeriesOptionsRegistry(DataViewerStateData* data, QObject* parent = nullptr);
    
    // Template API
    template<typename T> void set(QString const& key, T const& options);
    template<typename T> T const* get(QString const& key) const;
    template<typename T> T* getMutable(QString const& key);
    template<typename T> bool remove(QString const& key);
    template<typename T> bool has(QString const& key) const;
    template<typename T> QStringList keys() const;
    template<typename T> QStringList visibleKeys() const;
    template<typename T> int count() const;
    template<typename T> static QString typeName();
    template<typename T> void notifyChanged(QString const& key);
    
    // Visibility by type name (for non-template access)
    bool setVisible(QString const& key, QString const& type_name, bool visible);
    bool isVisible(QString const& key, QString const& type_name) const;

signals:
    void optionsChanged(QString const& key, QString const& type_name);
    void optionsRemoved(QString const& key, QString const& type_name);
    void visibilityChanged(QString const& key, QString const& type_name, bool visible);
    
private:
    DataViewerStateData* _data;
};
```

### Step 2.2: Implement template specializations

Each series type gets explicit template specializations:

```cpp
// Type names
template<> QString SeriesOptionsRegistry::typeName<AnalogSeriesOptionsData>() { return "analog"; }
template<> QString SeriesOptionsRegistry::typeName<DigitalEventSeriesOptionsData>() { return "event"; }
template<> QString SeriesOptionsRegistry::typeName<DigitalIntervalSeriesOptionsData>() { return "interval"; }

// set(), get(), getMutable(), remove(), has(), keys(), visibleKeys(), count(), notifyChanged()
// ... for each type
```

### Step 2.3: Unit tests

- set/get round-trip for all 3 types
- remove functionality
- keys() and visibleKeys()
- Signal emission verification
- Type safety verification

### Files to Create

- `src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.cpp`
- `src/WhiskerToolbox/DataViewer_Widget/SeriesOptionsRegistry.test.cpp`

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt`

---

## Phase 3: Create DataViewerState Class

**Goal**: Create the EditorState subclass with typed accessors and signals

**Estimated Duration**: 3-4 days

### Step 3.1: Create DataViewerState class declaration

```cpp
// DataViewerState.hpp
class DataViewerState : public EditorState {
    Q_OBJECT
public:
    explicit DataViewerState(QObject* parent = nullptr);
    ~DataViewerState() override = default;
    
    // EditorState interface
    [[nodiscard]] QString getTypeName() const override { return "DataViewer"; }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const& name) override;
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const& json) override;
    
    // Direct data access
    [[nodiscard]] DataViewerStateData const& data() const { return _data; }
    
    // Series options registry
    [[nodiscard]] SeriesOptionsRegistry& seriesOptions() { return _series_options; }
    [[nodiscard]] SeriesOptionsRegistry const& seriesOptions() const { return _series_options; }
    
    // === View State ===
    void setTimeWindow(int64_t start, int64_t end);
    [[nodiscard]] std::pair<int64_t, int64_t> timeWindow() const;
    void setYBounds(float y_min, float y_max);
    [[nodiscard]] std::pair<float, float> yBounds() const;
    void setVerticalPanOffset(float offset);
    [[nodiscard]] float verticalPanOffset() const;
    void setGlobalZoom(float zoom);
    [[nodiscard]] float globalZoom() const;
    void setGlobalVerticalScale(float scale);
    [[nodiscard]] float globalVerticalScale() const;
    void setYSpacing(float spacing);
    [[nodiscard]] float ySpacing() const;
    void setViewState(DataViewerViewState const& view);
    [[nodiscard]] DataViewerViewState const& viewState() const;
    
    // === Theme ===
    void setTheme(QString const& theme);  // "Dark" or "Light"
    [[nodiscard]] QString theme() const;
    void setBackgroundColor(QString const& hex);
    [[nodiscard]] QString backgroundColor() const;
    void setAxisColor(QString const& hex);
    [[nodiscard]] QString axisColor() const;
    
    // === Grid ===
    void setGridEnabled(bool enabled);
    [[nodiscard]] bool gridEnabled() const;
    void setGridSpacing(int spacing);
    [[nodiscard]] int gridSpacing() const;
    
    // === UI Preferences ===
    void setZoomScalingMode(QString const& mode);  // "Fixed" or "Adaptive"
    [[nodiscard]] QString zoomScalingMode() const;
    void setPropertiesPanelCollapsed(bool collapsed);
    [[nodiscard]] bool propertiesPanelCollapsed() const;
    
    // === Interaction ===
    void setInteractionMode(QString const& mode);
    [[nodiscard]] QString interactionMode() const;

signals:
    // Consolidated signals (following MediaWidgetState pattern)
    void viewStateChanged();
    void themeChanged();
    void gridChanged();
    void uiPreferencesChanged();
    void interactionModeChanged(QString const& mode);
    // Display options signals come from SeriesOptionsRegistry
    
private:
    DataViewerStateData _data;
    SeriesOptionsRegistry _series_options{&_data, this};
    void _connectRegistrySignals();
};
```

### Step 3.2: Implement DataViewerState

Implement all methods with appropriate signal emission.

### Step 3.3: Unit tests

- Serialization round-trip
- Signal emission for all setters
- Registry integration

### Files to Create

- `src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerState.cpp`
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerState.test.cpp`

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt`

---

## Phase 4: Wire State to OpenGLWidget

**Goal**: OpenGLWidget uses state for view/theme/grid instead of local members

**Estimated Duration**: 3-4 days

### Step 4.1: Add state pointer to OpenGLWidget

```cpp
// OpenGLWidget.hpp
class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    void setState(std::shared_ptr<DataViewerState> state);
    
private:
    std::shared_ptr<DataViewerState> _state;
    
    // Keep local copies for performance-critical paths
    // but sync from/to state
};
```

### Step 4.2: Wire view state

Replace direct `_view_state` manipulation with state accessors where appropriate:

```cpp
// Before
_view_state.global_zoom = scale;

// After
if (_state) {
    _state->setGlobalZoom(scale);
}
// Or maintain local cache and sync periodically
```

Key decisions:
- **Hot path (rendering)**: Keep local `_view_state` for performance
- **User interactions**: Update state, let state notify observers
- **State changes**: Sync local copy from state signals

### Step 4.3: Wire theme and grid state

Theme and grid are not hot-path critical, can read directly from state.

### Step 4.4: Connect state signals

```cpp
void OpenGLWidget::setState(std::shared_ptr<DataViewerState> state) {
    _state = state;
    
    connect(_state.get(), &DataViewerState::viewStateChanged,
            this, &OpenGLWidget::_onViewStateChanged);
    connect(_state.get(), &DataViewerState::themeChanged,
            this, &OpenGLWidget::_onThemeChanged);
    connect(_state.get(), &DataViewerState::gridChanged,
            this, &OpenGLWidget::_onGridChanged);
    
    _syncFromState();
}
```

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp`

---

## Phase 5: Wire State to DataViewer_Widget

**Goal**: DataViewer_Widget uses state for UI preferences

**Estimated Duration**: 2-3 days

### Step 5.1: Add state to DataViewer_Widget

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

### Step 5.2: Wire UI preferences

- `_zoom_scaling_mode` → state
- `_properties_panel_collapsed` → state

### Step 5.3: Pass state to OpenGLWidget

```cpp
DataViewer_Widget::DataViewer_Widget(...) {
    _state = std::make_shared<DataViewerState>();
    ui->openGLWidget->setState(_state);
}
```

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.hpp`
- `src/WhiskerToolbox/DataViewer_Widget/DataViewer_Widget.cpp`

---

## Phase 6: Refactor TimeSeriesDataStore

**Goal**: Remove display options from TimeSeriesDataStore; state becomes source of truth for user options

**Estimated Duration**: 4-5 days

This phase separates concerns between runtime data and serializable options.

### Step 6.1: Refactor TimeSeriesDataStore entry structs

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

### Step 6.2: Update add/remove methods

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

### Step 6.3: Update rendering to combine data + options

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

### Step 6.4: Update layout application

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

### Step 6.5: Handle key synchronization

When a series is removed from DataStore, also remove from state:

```cpp
void OpenGLWidget::removeAnalogTimeSeries(std::string const& key) {
    _data_store->removeAnalogSeries(key);
    _state->seriesOptions().remove<AnalogSeriesOptionsData>(QString::fromStdString(key));
}
```

### Files to Modify

- `src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.hpp` - Remove display_options
- `src/WhiskerToolbox/DataViewer_Widget/TimeSeriesDataStore.cpp` - Update add/remove
- `src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.hpp` - Add state integration
- `src/WhiskerToolbox/DataViewer_Widget/OpenGLWidget.cpp` - Update rendering
- `src/WhiskerToolbox/DataViewer_Widget/SceneBuildingHelpers.cpp` - Use state for style

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

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| 1: State Data Structures | 2-3 days | DataViewerStateData.hpp, unit tests |
| 2: SeriesOptionsRegistry | 3-4 days | Generic registry, template specializations |
| 3: DataViewerState Class | 3-4 days | EditorState subclass, accessors, signals |
| 4: Wire OpenGLWidget | 3-4 days | View/theme/grid from state |
| 5: Wire DataViewer_Widget | 2-3 days | UI preferences from state |
| 6: Refactor TimeSeriesDataStore | 4-5 days | Remove options, state as source of truth |
| 7: Cleanup & Testing | 2-3 days | Integration tests, documentation |
| **Total** | **~3-4 weeks** | **Full state extraction** |

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
