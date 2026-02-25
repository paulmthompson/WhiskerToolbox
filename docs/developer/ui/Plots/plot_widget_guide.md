# Plot Widget Developer Guide

This guide covers the shared architecture, axis types, DataManager integration, and
testing requirements for all plot widgets in `src/WhiskerToolbox/Plots/`.

## Widget Inventory

| Widget | X-Axis Type | Y-Axis Type | Data Sources |
|--------|-------------|-------------|--------------|
| **LinePlotWidget** | RelativeTimeAxis | VerticalAxis | AnalogTimeSeries (multiple series) |
| **EventPlotWidget** | RelativeTimeAxis | *(trial rows)* | DigitalEventSeries (multiple events) |
| **PSTHWidget** | RelativeTimeAxis | VerticalAxis | DigitalEventSeries (histogram bins) |
| **HeatmapWidget** | RelativeTimeAxis | VerticalAxis | AnalogTimeSeries (color-mapped) |
| **ScatterPlotWidget** | HorizontalAxis | VerticalAxis | Point pairs from two data keys |
| **ACFWidget** | HorizontalAxis | VerticalAxis | DigitalEventSeries (autocorrelation) |
| **SpectrogramWidget** | *(custom)* | *(custom)* | AnalogTimeSeries |
| **TemporalProjectionViewWidget** | HorizontalAxis | VerticalAxis | PointData / LineData |
| **OnionSkinViewWidget** | HorizontalAxis | VerticalAxis | PointData / LineData / MaskData |
| **3DPlotWidget** | *(3D)* | *(3D)* | PointData |

## Directory Structure

Every plot widget follows a consistent directory layout:

```
Plots/<WidgetName>/
├── CMakeLists.txt
├── <WidgetName>Registration.hpp    # EditorRegistry registration module
├── <WidgetName>Registration.cpp
├── Core/
│   ├── <Widget>State.hpp           # QObject state class (EditorState subclass)
│   ├── <Widget>State.cpp
│   └── <Widget>StateData.hpp       # Plain serializable struct (rfl-compatible)
├── Rendering/                      # OpenGL rendering (if applicable)
│   ├── <Widget>OpenGLWidget.hpp
│   └── <Widget>OpenGLWidget.cpp
└── UI/
    ├── <Widget>Widget.hpp          # Main composite widget  
    ├── <Widget>Widget.cpp
    ├── <Widget>PropertiesWidget.hpp # Properties/settings panel
    ├── <Widget>PropertiesWidget.cpp
    └── <Widget>PropertiesWidget.ui  # Qt Designer form
```

---

## Axis Types

There are three axis types, each with an identical layered architecture:

```
AxisStateData (plain struct)  →  AxisState (QObject)  →  AxisWidget (QWidget)  →  AxisWithRangeControls (factory)
```

### 1. RelativeTimeAxis — Symmetric Time Windows

**When to use:** Plots aligned to events or intervals where X represents relative time
centered around an alignment point (t = 0).

**Components:**

| Layer | Class | Location |
|-------|-------|----------|
| Data | `RelativeTimeAxisStateData` | `Common/RelativeTimeAxisWidget/Core/` |
| State | `RelativeTimeAxisState` | `Common/RelativeTimeAxisWidget/Core/` |
| Widget | `RelativeTimeAxisWidget` | `Common/RelativeTimeAxisWidget/` |
| Factory | `RelativeTimeAxisWithRangeControls` | `Common/RelativeTimeAxisWidget/` |

**State fields:**
- `min_range` (default -500.0) — left edge of the time window
- `max_range` (default 500.0) — right edge of the time window

**Signals:** `minRangeChanged`, `maxRangeChanged`, `rangeChanged`, `rangeUpdated`

The `RelativeTimeAxisWidget` takes a `ViewStateGetter` (returns `CorePlotting::ViewState`)
rather than a simple `(min, max)` pair, making it zoom/pan-aware.

**Usage pattern in a plot state:**
```cpp
// In your state constructor:
_relative_time_axis_state = std::make_unique<RelativeTimeAxisState>(this);

// Initialize from alignment window:
double half_window = _data.alignment.window_size / 2.0;
_relative_time_axis_state->setRangeSilent(-half_window, half_window);
_data.time_axis = _relative_time_axis_state->data();

// Synchronize axis ↔ view state:
connect(_relative_time_axis_state.get(), &RelativeTimeAxisState::rangeChanged,
        this, [this]() {
            _data.time_axis = _relative_time_axis_state->data();
            _data.view_state.x_min = _data.time_axis.min_range;
            _data.view_state.x_max = _data.time_axis.max_range;
            markDirty();
            emit viewStateChanged();
            emit stateChanged();
        });
```

**Used by:** LinePlotWidget, EventPlotWidget, PSTHWidget, HeatmapWidget

### 2. HorizontalAxis — Generic Value Axis

**When to use:** Plots where the X-axis represents an arbitrary numeric range (not
relative time). Typical for scatter plots, ACF lag values, or generic data axes.

**Components:**

| Layer | Class | Location |
|-------|-------|----------|
| Data | `HorizontalAxisStateData` | `Common/HorizontalAxisWidget/Core/` |
| State | `HorizontalAxisState` | `Common/HorizontalAxisWidget/Core/` |
| Widget | `HorizontalAxisWidget` | `Common/HorizontalAxisWidget/` |
| Factory | `HorizontalAxisWithRangeControls` | `Common/HorizontalAxisWidget/` |

**State fields:**
- `x_min` (default 0.0)
- `x_max` (default 100.0)

**Signals:** `xMinChanged`, `xMaxChanged`, `rangeChanged`, `rangeUpdated`

The `HorizontalAxisWidget` can take either a `RangeGetter` function or have its range
set directly with `setRange(min, max)`.

**Usage pattern in a plot state:**
```cpp
// In your state constructor:
_horizontal_axis_state = std::make_unique<HorizontalAxisState>(this);
_data.horizontal_axis = _horizontal_axis_state->data();
_data.view_state.x_min = _horizontal_axis_state->getXMin();
_data.view_state.x_max = _horizontal_axis_state->getXMax();

// Synchronize axis ↔ view state:
auto syncHorizontalData = [this]() {
    _data.horizontal_axis = _horizontal_axis_state->data();
    markDirty();
    emit stateChanged();
};
connect(_horizontal_axis_state.get(), &HorizontalAxisState::rangeChanged,
        this, syncHorizontalData);
connect(_horizontal_axis_state.get(), &HorizontalAxisState::rangeUpdated,
        this, syncHorizontalData);
```

**Used by:** ScatterPlotWidget, ACFWidget, TemporalProjectionViewWidget, OnionSkinViewWidget

### 3. VerticalAxis — Y-Axis (Value or Trial Index)

**When to use:** Almost all 2D plots need a Y-axis. Can represent analog values,
trial indices, bin counts, or spatial coordinates.

**Components:**

| Layer | Class | Location |
|-------|-------|----------|
| Data | `VerticalAxisStateData` | `Common/VerticalAxisWidget/Core/` |
| State | `VerticalAxisState` | `Common/VerticalAxisWidget/Core/` |
| Widget | `VerticalAxisWidget` | `Common/VerticalAxisWidget/` |
| Factory | `VerticalAxisWithRangeControls` | `Common/VerticalAxisWidget/` |
| Helper | `VerticalAxisSynchronizer` | `Common/VerticalAxisWidget/` |

**State fields:**
- `y_min` (default 0.0)
- `y_max` (default 100.0)

**Signals:** `yMinChanged`, `yMaxChanged`, `rangeChanged`, `rangeUpdated`

**Inversion support:** The `VerticalAxisWidget` supports `setInverted(true)` for
image-coordinate conventions (Y=0 at top, increasing downward).

**ViewState synchronization helper:**
```cpp
#include "Plots/Common/VerticalAxisWidget/VerticalAxisSynchronizer.hpp"

// Sync vertical axis state to OpenGL pan/zoom changes:
syncVerticalAxisToViewState(
    _vertical_axis_state.get(),
    _state.get(),
    [](auto const & vs) { return std::make_pair(vs.y_min, vs.y_max); }
);
```

**Used by:** LinePlotWidget, PSTHWidget, HeatmapWidget, ScatterPlotWidget, ACFWidget,
TemporalProjectionViewWidget, OnionSkinViewWidget

### AxisMapping — Custom Label Formatting

All three axis widgets support an optional `AxisMapping` for custom label formatting.
An `AxisMapping` provides:

- `worldToDomain(double world) → double` — convert rendering coordinates to semantic values
- `domainToWorld(double domain) → double` — reverse conversion
- `formatLabel(double domain) → std::string` — produce the tick label string

Factory functions in `CorePlotting/CoordinateTransform/AxisMapping.hpp`:

| Factory | Purpose |
|---------|---------|
| `identityAxis(title)` | No conversion, decimal formatting |
| `linearAxis(scale, offset, title, precision)` | Linear transform with configurable precision |
| `trialIndexAxis(total_trials)` | Integer trial labels (1-based) |
| `relativeTimeAxis(title)` | "+N" / "-N" / "0" formatting |
| `analogAxis(title, unit)` | Numeric with unit suffix |

```cpp
// Example: Set trial index labels on a vertical axis
axis_widget->setAxisMapping(CorePlotting::trialIndexAxis(100));

// Example: Clear custom formatting
axis_widget->clearAxisMapping();
```

### Adding an Axis to a New Plot Widget

**Step 1: Add axis state to your StateData struct:**
```cpp
struct MyPlotStateData {
    std::string instance_id;
    std::string display_name = "My Plot";
    CorePlotting::ViewStateData view_state;
    // Choose the appropriate axis types:
    RelativeTimeAxisStateData time_axis;     // or HorizontalAxisStateData
    VerticalAxisStateData vertical_axis;
    // Plus any alignment data:
    PlotAlignmentData alignment;
};
```

**Step 2: Create axis state objects in your State constructor:**
```cpp
MyPlotState::MyPlotState(QObject * parent)
    : EditorState(parent),
      _relative_time_axis_state(std::make_unique<RelativeTimeAxisState>(this)),
      _vertical_axis_state(std::make_unique<VerticalAxisState>(this))
{
    // Initialize axis data from state
    _data.time_axis = _relative_time_axis_state->data();
    _data.vertical_axis = _vertical_axis_state->data();
    
    // Sync view_state bounds to axis ranges
    _data.view_state.x_min = _data.time_axis.min_range;
    _data.view_state.x_max = _data.time_axis.max_range;
    _data.view_state.y_min = _data.vertical_axis.y_min;
    _data.view_state.y_max = _data.vertical_axis.y_max;
    
    // Wire up synchronization (see patterns in existing widgets)
    // ...
}
```

**Step 3: Create axis widgets in your UI using the factory:**
```cpp
// In the properties widget or main widget:
auto axis_system = createRelativeTimeAxisWithRangeControls(
    _state->relativeTimeAxisState(),
    plot_area,       // parent for axis display widget
    controls_area    // parent for spinbox controls
);

auto y_axis_system = createVerticalAxisWithRangeControls(
    _state->verticalAxisState(),
    plot_area,
    controls_area
);
```

**Step 4: Serialize/deserialize in `toJson()`/`fromJson()`:**
```cpp
std::string MyPlotState::toJson() const {
    return rfl::json::write(_data);  // StateData contains all axis data
}

bool MyPlotState::fromJson(std::string const & json) {
    auto result = rfl::json::read<MyPlotStateData>(json);
    if (result) {
        _data = *result;
        // Restore axis states from deserialized data:
        _relative_time_axis_state->data() = _data.time_axis;
        _vertical_axis_state->data() = _data.vertical_axis;
        emit stateChanged();
        return true;
    }
    return false;
}
```

### The "Silent Update" Pattern

All axis states have two range-setting methods:

- **`setRange(min, max)`** — Emits both `rangeChanged` and `rangeUpdated`.
  Use when the user changes the range via spinboxes (Flow A: user input).
  
- **`setRangeSilent(min, max)`** — Emits only `rangeUpdated`, NOT `rangeChanged`.
  Use when the range changes from panning/zooming (Flow B: OpenGL interaction).
  This prevents feedback loops between the axis state and the view state.

The `VerticalAxisSynchronizer` helper template automates Flow B:
```cpp
syncVerticalAxisToViewState(axis_state, plot_state, compute_bounds_fn);
```

---

## DataManager Integration

### Overview

Plot widgets interact with the DataManager to:
1. **Discover available data keys** — populate combo boxes
2. **Retrieve data for visualization** — `getData<T>(key)` calls
3. **React to key additions/removals** — observer callbacks

### The Observer Pattern

The DataManager exposes a simple observer API:

```cpp
class DataManager {
public:
    using ObserverCallback = std::function<void()>;
    int addObserver(ObserverCallback callback);      // Returns callback ID
    void removeObserver(int callback_id);            // Removes by ID
};
```

The callback fires on **any** DataManager state change (data added, data deleted).

### Required Integration Steps

#### 1. Store the DataManager and Observer ID

```cpp
class MyPropertiesWidget : public QWidget {
    // ...
private:
    std::shared_ptr<DataManager> _data_manager;
    int _dm_observer_id = -1;  // -1 means "no callback registered"
};
```

#### 2. Register the Observer in the Constructor

```cpp
MyPropertiesWidget::MyPropertiesWidget(
    std::shared_ptr<MyState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent), _data_manager(std::move(data_manager))
{
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateComboBoxes();
        });
    }
}
```

#### 3. Remove the Observer in the Destructor

**This is critical.** Failing to remove the observer will leave a dangling `this`
pointer in the DataManager's callback list, causing undefined behavior (usually a
crash) when the DataManager next fires observers.

```cpp
MyPropertiesWidget::~MyPropertiesWidget()
{
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
}
```

#### 4. Handle Key Removal in Combo Box Repopulation

When the observer fires, re-query `DataManager::getKeys<T>()` to rebuild combo boxes.
The deleted key will simply not appear. If possible, preserve the user's current
selection:

```cpp
void MyPropertiesWidget::_populateComboBoxes() {
    // Save current selection
    QString current = _combo->currentData().toString();
    
    _combo->clear();
    auto keys = _data_manager->getKeys<AnalogTimeSeries>();
    for (auto const & key : keys) {
        _combo->addItem(QString::fromStdString(key),
                        QString::fromStdString(key));
    }
    
    // Restore selection if still valid
    int idx = _combo->findData(current);
    if (idx >= 0) {
        _combo->setCurrentIndex(idx);
    }
}
```

### Handling Stale Keys in Visualizations

When a user adds a data key to a plot (e.g., adds "signal_A" as a LinePlot series)
and then that key is deleted from the DataManager, the plot state still references
the deleted key. Widgets should handle this gracefully:

1. **In rendering code:** Always null-check `getData<T>(key)` results before use.
2. **In the observer callback:** After repopulating combo boxes, optionally check
   whether any currently visualized keys are now missing and auto-remove them:

```cpp
void MyPropertiesWidget::_onDataManagerChanged() {
    _populateComboBoxes();
    
    // Purge stale visualization keys
    auto all_keys = _data_manager->getAllKeys();
    std::set<std::string> valid_keys(all_keys.begin(), all_keys.end());
    
    for (auto const & name : _state->getPlotSeriesNames()) {
        auto opts = _state->getPlotSeriesOptions(name);
        if (opts && valid_keys.find(opts->series_key) == valid_keys.end()) {
            _state->removePlotSeries(name);  // Key no longer exists
        }
    }
}
```

> **Note:** Currently not all widgets implement stale key purging. This is a gap
> that should be addressed — see the testing section below for how to verify this.

### PlotAlignmentWidget — Shared Alignment UI

Most event-aligned plots (LinePlot, EventPlot, PSTH, Heatmap) delegate their alignment
UI to the shared `PlotAlignmentWidget`. This widget:

- Registers its **own** DataManager observer internally
- Populates its alignment event combo box with both `DigitalEventSeries` and
  `DigitalIntervalSeries` keys
- Cleans up its observer in its own destructor

The parent properties widget still needs its own observer for data-selection combo
boxes (e.g., which AnalogTimeSeries to plot).

---

## Common Testing Requirements

Every plot properties widget that registers a DataManager observer should be tested
for the following behaviors. These tests verify the contract between the widget
and the DataManager and prevent dangling reference bugs.

### Test Categories

| Category | What It Verifies |
|----------|-----------------|
| **Combo box population** | Combo box correctly reflects current DataManager keys |
| **Observer refresh** | Adding/removing data updates the combo box in real-time |
| **Destruction cleanup** | Observer is removed in destructor; no crash after widget dies |
| **Stale key handling** | Visualized keys that are deleted from DM are handled gracefully |

### Existing Per-Widget Tests

Each properties widget has its own test file following an identical pattern:

- `LinePlotPropertiesWidget.test.cpp`
- `EventPlotPropertiesWidget.test.cpp`
- `ACFPropertiesWidget.test.cpp`
- `PSTHPropertiesWidget.test.cpp`
- `HeatmapPropertiesWidget.test.cpp`

### Template-Based Common Tests

Because the test logic is nearly identical across widgets, we provide a
`TEMPLATE_TEST_CASE`-based common test suite in
`tests/WhiskerToolbox/Plots/Common/PlotWidgetCommonTests.hpp`. This header
defines parameterized tests that any plot widget can instantiate by providing
a traits class.

See `tests/WhiskerToolbox/Plots/Common/PlotWidgetCommonTests.hpp` for the
trait requirements and `PlotWidgetCommonTests.test.cpp` for instantiations
covering LinePlotWidget, EventPlotWidget, and ACFWidget.

---

## Shared Helpers Reference

### PlotInteractionHelpers (No Qt dependency)

Template functions for zoom, pan, projection, and coordinate conversion. Used by
all OpenGL rendering widgets.

| Function | Purpose |
|----------|---------|
| `computeOrthoProjection(view_state, ...)` | Build orthographic projection matrix |
| `handlePanning(state, view_state, delta, ...)` | Convert pixel drag to world-space pan |
| `handleZoom(state, view_state, delta, y_only, both)` | Apply scroll-wheel zoom with axis selection |
| `screenToWorld(proj, w, h, pos)` | Pixel → world coordinate transform |
| `worldToScreen(proj, w, h, x, y)` | World → pixel coordinate transform |
| `screenToNDC(pos, w, h)` | Pixel → normalized device coordinates |

These are constrained by C++20 concepts (`ViewStateLike`, `ZoomPanSettable`,
`ViewStateWithBounds`) so they work with any plot's view state type.

### PlotAlignmentGather (Template data gatherers)

Free functions for creating trial-aligned `GatherResult` objects:

| Function | Purpose |
|----------|---------|
| `gatherWithEventAlignment<T>(source, events, pre, post)` | Expand events to intervals |
| `gatherWithIntervalAlignment<T>(source, intervals, align)` | Use interval boundaries |
| `createAlignedGatherResult<T>(dm, key, alignment_data)` | High-level: auto-detect type |
| `getAlignmentSource(dm, key)` | Look up alignment series type |

### LineSelectionHelpers

Shared logic for line-drag selection across `LinePlotOpenGLWidget` and
`TemporalProjectionOpenGLWidget`.
