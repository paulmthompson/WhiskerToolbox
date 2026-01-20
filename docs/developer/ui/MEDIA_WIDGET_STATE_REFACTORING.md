# Media_Widget State Extraction Roadmap

## Executive Summary

This document outlines a phased approach to extract serializable state from the complex `Media_Widget` hierarchy. The goal is to create a comprehensive `MediaWidgetState` that can be serialized to JSON using reflect-cpp, enabling workspace save/restore and laying the groundwork for properties panel separation.

**Current State**: `MediaWidgetState` is minimal (only tracks `displayed_data_key`)

**Target State**: Full serialization of all display options, per-data-type configurations, and interaction preferences

## Current Architecture Analysis

### State Categories

After analyzing the Media_Widget codebase, state falls into four categories:

| Category | Description | Serializable? | Examples |
|----------|-------------|---------------|----------|
| **Display Options** | How data is visually rendered | ✅ Yes | Colors, alpha, line thickness, marker shapes |
| **Processing Options** | Image filters and transforms | ✅ Yes | Contrast, gamma, CLAHE, colormap |
| **Interaction State** | Current tool/mode selections | ✅ Partial | Selection mode, brush size, edge snapping |
| **Transient State** | Temporary runtime state | ❌ No | Hover position, drawing points, preview masks |

### Current State Locations

```
Media_Widget/
├── Media_Widget.hpp
│   ├── _current_zoom, _user_zoom_active    # Viewport state
│   ├── _is_panning, _last_pan_point        # Transient interaction
│   └── _state (MediaWidgetState)           # Minimal - only displayed_data_key
│
├── Media_Window/Media_Window.hpp
│   ├── _media_configs    # std::unordered_map<string, unique_ptr<MediaDisplayOptions>>
│   ├── _line_configs     # std::unordered_map<string, unique_ptr<LineDisplayOptions>>
│   ├── _mask_configs     # std::unordered_map<string, unique_ptr<MaskDisplayOptions>>
│   ├── _point_configs    # std::unordered_map<string, unique_ptr<PointDisplayOptions>>
│   ├── _interval_configs # std::unordered_map<string, unique_ptr<DigitalIntervalDisplayOptions>>
│   ├── _tensor_configs   # std::unordered_map<string, unique_ptr<TensorDisplayOptions>>
│   ├── _canvasWidth, _canvasHeight         # Canvas dimensions
│   ├── _drawing_mode, _show_hover_circle   # Interaction state
│   └── _selected_entities, _selected_data_key  # Selection state
│
├── MediaLine_Widget/MediaLine_Widget.hpp
│   ├── _selection_mode (enum)              # Current line tool
│   ├── _smoothing_mode, _polynomial_order  # Line smoothing options
│   ├── _edge_snapping_enabled, _edge_threshold, _edge_search_radius
│   ├── _selected_line_index, _line_selection_threshold
│   └── _is_updating_percentages            # Transient flag
│
├── MediaMask_Widget/MediaMask_Widget.hpp
│   ├── _selection_mode (enum)              # Current mask tool
│   ├── _is_dragging, _is_adding_mode       # Transient brush state
│   ├── _preview_active                     # Preview flag
│   └── _allow_empty_mask                   # Preference
│
├── MediaPoint_Widget/MediaPoint_Widget.hpp
│   ├── _selection_enabled                  # Point selection mode
│   ├── _selected_point_id                  # Selected point
│   └── _selection_threshold                # Selection preference
│
├── MediaText_Widget/MediaText_Widget.hpp
│   └── _text_overlays                      # vector<TextOverlay>
│
└── DisplayOptions/DisplayOptions.hpp
    ├── BaseDisplayOptions                  # enabled, is_visible, hex_color, alpha
    ├── MediaDisplayOptions                 # Base + all processing options
    ├── LineDisplayOptions                  # Base + thickness, show_points, segment options
    ├── MaskDisplayOptions                  # Base + show_bounding_box, show_outline
    ├── PointDisplayOptions                 # Base + point_size, marker_shape
    ├── DigitalIntervalDisplayOptions       # Base + style, box_size, location
    └── TensorDisplayOptions                # Base + display_channel
```

## Refactoring Strategy

### Key Principles

1. **Incremental Migration**: Move state piece by piece, validating each step
2. **Preserve Existing Behavior**: Widget internals can read/write to state
3. **reflect-cpp Compatibility**: All serializable types must be plain structs
4. **No Inheritance in Serializable Types**: reflect-cpp cannot handle base/derived class hierarchies with members in both - use composition or flat structs
5. **Serializable From the Start**: Define option types as serializable directly, no parallel "runtime" vs "serializable" types
6. **Use `rfl::Flatten`**: For composing state structs while keeping JSON flat
7. **Native Enum Serialization**: C++ enums serialize as strings automatically (e.g., `PointMarkerShape::Circle` → `"Circle"`)
8. **Transient vs Persistent**: Clearly separate non-serializable state

### reflect-cpp Requirements

For a struct to be serializable with reflect-cpp:
- All members must be default-constructible
- No Qt types (QString → std::string, QColor → std::string hex)
- **No inheritance with members in base class** - use `rfl::Flatten` for composition
- **Enums serialize as strings automatically** - no special handling needed (e.g., `Color::green` serializes as `"green"`)
- Maps/vectors work with standard types
- No raw pointers or unique_ptr in serialized data

### Using `rfl::Flatten` for Composition

Instead of inheritance, use `rfl::Flatten` to compose structs:

```cpp
// BAD: Inheritance (won't serialize correctly)
struct BaseDisplayOptions {
    std::string hex_color;
    float alpha;
};
struct LineDisplayOptions : BaseDisplayOptions {  // ❌ Won't work!
    int line_thickness;
};

// GOOD: Composition with rfl::Flatten
struct CommonDisplayFields {
    std::string hex_color = "#007bff";
    float alpha = 1.0f;
    bool is_visible = false;
};

struct LineDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // ✅ Fields appear flat in JSON
    int line_thickness = 2;
    bool show_points = false;
};

// JSON output: {"enabled":true,"is_visible":true,"hex_color":"#007bff","alpha":1.0,"line_thickness":2,"show_points":false}
```

This gives the organizational benefits of composition while producing flat JSON.

---

## Phase 1: Refactor DisplayOptions to be Serializable ✅ COMPLETE

**Goal**: Modify existing DisplayOptions structs to be directly serializable with reflect-cpp (no inheritance, no conversion functions)

**Status**: ✅ Complete (January 20, 2026)

**Duration**: Completed in 1 day

**Key Changes**:
- Replaced inheritance-based `BaseDisplayOptions` with composition using `rfl::Flatten<CommonDisplayFields>`
- Added convenience accessor methods to all display option types for backward compatibility
- Converted `plot_color_with_alpha` to a template function supporting all display option types
- Updated ~60 usage sites across 7 files to use accessor methods
- All tests pass with no regressions

**Files Modified**: [DisplayOptions.hpp](../../../src/WhiskerToolbox/Media_Widget/DisplayOptions/DisplayOptions.hpp), [Media_Window.hpp](../../../src/WhiskerToolbox/Media_Widget/Media_Window/Media_Window.hpp), [Media_Window.cpp](../../../src/WhiskerToolbox/Media_Widget/Media_Window/Media_Window.cpp)

### 1.1 Key Transformation Pattern

**Before** (inheritance-based):
```cpp
struct BaseDisplayOptions {
    std::string hex_color;
    float alpha;
    bool is_visible;
};

struct LineDisplayOptions : BaseDisplayOptions {  // ❌ Won't serialize correctly
    int line_thickness = 2;
};
```

**After** (composition with `rfl::Flatten`):
```cpp
struct CommonDisplayFields {
    std::string hex_color = "#007bff";
    float alpha = 1.0f;
    bool is_visible = false;
};

struct LineDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // ✅ Flattens in JSON
    int line_thickness = 2;
    
    // Accessor methods for backward compatibility
    std::string & hex_color() { return common.get().hex_color; }
    float & alpha() { return common.get().alpha; }
};
```

**Result**: JSON output is flat (no nested "common" object), while C++ code benefits from composition.

### 1.2 Enum Serialization

reflect-cpp automatically serializes C++ enums as strings - no conversion helpers needed:
- `PointMarkerShape::Circle` → JSON: `"Circle"`
- Native `enum class` types work directly in structs
- No `rfl::Literal` or manual string conversion required

### 1.3 Outcome

All six display option types (Media, Line, Mask, Point, Tensor, DigitalInterval) are now directly serializable with reflect-cpp. No separate "runtime" vs "serializable" type hierarchies needed.

---

## Phase 2: Create Comprehensive MediaWidgetStateData ✅ COMPLETE

**Goal**: Define the full state structure that MediaWidgetState will serialize

**Status**: ✅ Complete (January 20, 2026)

**Duration**: 1 day (completed same day as Phase 1)

### 2.1 Define State Structure ✅

**Implementation**: [MediaWidgetStateData.hpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetStateData.hpp)

The complete state structure has been implemented with:

The complete state structure has been implemented with:
- `TextOverlayData` - Serializable text annotations with position, color, orientation
- Interaction preferences - `LineInteractionPrefs`, `MaskInteractionPrefs`, `PointInteractionPrefs`
- `ViewportState` - Zoom, pan, canvas dimensions
- Tool mode enums - `LineToolMode`, `MaskToolMode`, `PointToolMode` (serialize as strings)
- `MediaWidgetStateData` - Main structure containing all persistent state

**Key Design Decisions**:
- Nested objects at top level for clear JSON structure (not `rfl::Flatten`)
- Native enum serialization (e.g., `LineToolMode::Add` → `"Add"`)
- All std:: types (no Qt dependencies)
- Transient state excluded (hover positions, drag state, etc.)

### 2.2 Testing ✅

**Tests**: [MediaWidgetStateData.test.cpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetStateData.test.cpp)

Comprehensive unit tests implemented covering:
- Individual component serialization (TextOverlayData, ViewportState, preferences)
- Enum string serialization validation
- `rfl::Flatten` flat JSON structure verification  
- Complex state round-trip serialization with all fields
- JSON structure validation (nested vs flat)

**All tests passing** ✅

### 2.3 State Size Estimation

Typical serialized state:
Typical serialized state:
- Base structure: ~500 bytes
- Per enabled feature: ~200-500 bytes
- Text overlays: ~100 bytes each
- **Typical session (5-10 features): 2-5 KB of JSON**

---

## Phase 3: Expand MediaWidgetState Class ✅ COMPLETE

**Goal**: Implement the Qt wrapper that holds `MediaWidgetStateData` and provides typed signals

**Status**: ✅ Complete (January 20, 2026)

**Duration**: 1 day (completed same day as Phases 1-2)

### 3.1 Key Changes ✅

The MediaWidgetState class has been fully expanded with:

**Viewport State Management**:
- `setZoom()`, `zoom()` - Zoom level control
- `setPan()`, `pan()` - Pan offset control
- `setCanvasSize()`, `canvasSize()` - Canvas dimension control
- `setViewport()`, `viewport()` - Complete viewport state access
- Signals: `zoomChanged`, `panChanged`, `canvasSizeChanged`, `viewportChanged`

**Feature Management**:
- `setFeatureEnabled()` - Enable/disable features by data type
- `isFeatureEnabled()` - Check feature visibility
- `enabledFeatures()` - Get list of enabled features for a type
- Signals: `featureEnabledChanged`

**Display Options (All 6 Types)**:
- Line, Mask, Point, Tensor, Interval, Media
- CRUD operations: `set*Options()`, `*Options()`, `remove*Options()`
- Signals: `displayOptionsChanged`, `displayOptionsRemoved`

**Interaction Preferences**:
- `setLinePrefs()`, `linePrefs()` - Line tool preferences
- `setMaskPrefs()`, `maskPrefs()` - Mask tool preferences
- `setPointPrefs()`, `pointPrefs()` - Point tool preferences
- Signals: `linePrefsChanged`, `maskPrefsChanged`, `pointPrefsChanged`

**Text Overlays**:
- `addTextOverlay()` - Add with auto-assigned ID
- `removeTextOverlay()` - Remove by ID
- `updateTextOverlay()` - Update existing overlay
- `clearTextOverlays()` - Remove all overlays
- `getTextOverlay()` - Retrieve by ID
- `textOverlays()` - Get all overlays
- Signals: `textOverlayAdded`, `textOverlayRemoved`, `textOverlayUpdated`, `textOverlaysCleared`

**Active Tool Modes**:
- `setActiveLineMode()`, `activeLineMode()` - Line tool mode
- `setActiveMaskMode()`, `activeMaskMode()` - Mask tool mode
- `setActivePointMode()`, `activePointMode()` - Point tool mode
- Signals: `activeLineModeChanged`, `activeMaskModeChanged`, `activePointModeChanged`

**Direct Data Access**:
- `data()` - Const reference to underlying MediaWidgetStateData for efficient batch reads
- `viewport()` - Const reference to viewport state

### 3.2 Implementation Details ✅

All setters implement:
- Value change detection to avoid unnecessary signals
- Dirty state tracking via `markDirty()`
- Qt signal emission for each property change
- Floating-point comparison with epsilon for viewport values

Feature management auto-creates display options with defaults when enabling a feature that doesn't exist yet.

Text overlay IDs are auto-assigned using an incrementing counter stored in the state data.

### 3.3 Testing ✅

**Test Coverage**: Comprehensive unit tests in [MediaWidgetState.test.cpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetState.test.cpp)

Test categories:
- Basic state properties (instance ID, type name, display name, dirty tracking)
- Serialization (round-trip, instance ID preservation, invalid JSON)
- Qt signals (all 20+ signal types verified with QSignalSpy)
- Viewport state (zoom, pan, canvas size, complete viewport)
- Feature management (enable/disable, all 6 types, enabled list)
- Display options CRUD (all 6 types with proper signal emission)
- Interaction preferences (line, mask, point)
- Text overlays (add, remove, update, clear, get by ID)
- Tool modes (line, mask, point with enum serialization)
- Direct data access (`data()`, `viewport()`)
- Complex state round-trip (all properties simultaneously)

**Result**: All tests passing ✅

### 3.4 Files Modified ✅

- [MediaWidgetState.hpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetState.hpp) - Full interface with 60+ methods and 20+ signals
- [MediaWidgetState.cpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetState.cpp) - Complete implementation (~600 lines)
- [MediaWidgetState.test.cpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetState.test.cpp) - Comprehensive tests (~850 lines)

### 3.5 Design Patterns Used ✅

**Signal-Driven Architecture**: Each state property emits both a specific signal (e.g., `zoomChanged`) and the generic `stateChanged` signal, enabling both fine-grained and coarse-grained observation.

**Const-Correctness**: All getters return const references or values. Modification only through setters ensures signal emission.

**Automatic Option Creation**: `setFeatureEnabled(..., true)` auto-creates default display options if they don't exist, simplifying widget integration.

**ID Management**: Text overlays use auto-incrementing IDs stored in state, ensuring uniqueness across serialization cycles.

**Reference**: See [MediaWidgetState.hpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetState.hpp) and [MediaWidgetState.cpp](../../../src/WhiskerToolbox/Media_Widget/MediaWidgetState.cpp) for the complete implementation.

---

## Phase 4: Integrate State into Media_Widget

**Goal**: Make Media_Widget read from and write to MediaWidgetState

**Duration**: 2 weeks

### 4.1 Migration Strategy

**Do NOT** remove existing member variables immediately. Instead:

1. Add state synchronization at key points
2. Existing widgets continue to work
3. State becomes the "source of truth" gradually

### 4.2 Viewport State Integration

```cpp
// In Media_Widget.cpp

void Media_Widget::_applyZoom(double factor, bool anchor_under_mouse) {
    // Existing zoom logic...
    _current_zoom = new_zoom;
    
    // NEW: Sync to state
    _state->setZoom(_current_zoom);
}

// When restoring from serialized state:
void Media_Widget::restoreFromState() {
    _current_zoom = _state->zoom();
    auto [pan_x, pan_y] = _state->pan();
    // Apply zoom/pan to graphics view...
}
```

### 4.3 Display Options Integration

Since DisplayOptions are now directly serializable, the integration is straightforward:

```cpp
// In Media_Window.cpp

void Media_Window::addLineDataToScene(std::string const& line_key) {
    // Check if state has saved options for this key
    if (_media_widget_state) {
        auto const* saved_opts = _media_widget_state->lineOptions(QString::fromStdString(line_key));
        if (saved_opts) {
            // Options are already the right type - just copy
            _line_configs[line_key] = std::make_unique<LineDisplayOptions>(*saved_opts);
        } else {
            // New feature - create with default color
            auto opts = std::make_unique<LineDisplayOptions>();
            opts->common.get().hex_color = DefaultDisplayValues::getColorForIndex(_line_configs.size());
            _line_configs[line_key] = std::move(opts);
        }
    } else {
        // No state - use defaults
        auto opts = std::make_unique<LineDisplayOptions>();
        opts->common.get().hex_color = DefaultDisplayValues::getColorForIndex(_line_configs.size());
        _line_configs[line_key] = std::move(opts);
    }
    
    UpdateCanvas();
}

// When options change, sync to state
void Media_Window::onLineOptionsChanged(std::string const& line_key) {
    if (_media_widget_state && _line_configs.contains(line_key)) {
        // Direct copy - no conversion needed
        _media_widget_state->setLineOptions(
            QString::fromStdString(line_key), 
            *_line_configs[line_key]
        );
    }
}
```

### 4.4 Text Overlay Integration

TextOverlayData is now directly serializable:

```cpp
// MediaText_Widget now delegates to state
void MediaText_Widget::addTextOverlay(TextOverlay const& overlay) {
    // Convert Qt types to serializable
    TextOverlayData data;
    data.text = overlay.text.toStdString();
    data.orientation = overlay.orientation;  // Native enum - no string conversion needed
    data.x_position = overlay.x_position;
    data.y_position = overlay.y_position;
    data.color = overlay.color.name().toStdString();
    data.font_size = overlay.font_size;
    data.enabled = overlay.enabled;
    
    int id = _state->addTextOverlay(data);  // State assigns ID
    refreshTable();
}

std::vector<TextOverlay> MediaText_Widget::getEnabledTextOverlays() const {
    std::vector<TextOverlay> result;
    for (auto const& data : _state->textOverlays()) {
        if (data.enabled) {
            TextOverlay overlay;
            overlay.id = data.id;
            overlay.text = QString::fromStdString(data.text);
            overlay.orientation = data.orientation;  // Native enum - direct assignment
            overlay.x_position = data.x_position;
            overlay.y_position = data.y_position;
            overlay.color = QColor(QString::fromStdString(data.color));
            overlay.font_size = data.font_size;
            overlay.enabled = data.enabled;
            result.push_back(overlay);
        }
    }
    return result;
}
```

Note: The `TextOverlay` struct in MediaText_Widget.hpp uses Qt types (QString, QColor) for UI convenience. The `TextOverlayData` uses std::string for serialization. A simple conversion is needed at the boundary.

---

## Phase 5: Integrate with Sub-Widgets

**Goal**: Connect MediaLine_Widget, MediaMask_Widget, etc. to state

**Duration**: 2 weeks

### 5.1 Sub-Widget State Access Pattern

Sub-widgets don't own state; they read/write through Media_Widget:

```cpp
// MediaLine_Widget.hpp
class MediaLine_Widget : public QWidget {
public:
    void setMediaWidgetState(std::shared_ptr<MediaWidgetState> state);
    
private:
    std::weak_ptr<MediaWidgetState> _state;
    
    void _syncFromState();
    void _syncToState();
};

// MediaLine_Widget.cpp
void MediaLine_Widget::_syncFromState() {
    auto state = _state.lock();
    if (!state) return;
    
    auto const& prefs = state->linePrefs();
    _smoothing_mode = (prefs.smoothing_mode == "PolynomialFit") 
        ? Smoothing_Mode::PolynomialFit : Smoothing_Mode::SimpleSmooth;
    _polynomial_order = prefs.polynomial_order;
    _edge_snapping_enabled = prefs.edge_snapping_enabled;
    // ... update UI widgets to match ...
}

void MediaLine_Widget::_toggleEdgeSnapping(bool checked) {
    _edge_snapping_enabled = checked;
    
    // Sync to state
    if (auto state = _state.lock()) {
        auto prefs = state->linePrefs();
        prefs.edge_snapping_enabled = checked;
        state->setLinePrefs(prefs);
    }
}
```

### 5.2 Connect State Signals to Sub-Widgets

```cpp
// In Media_Widget::_createOptions()

// When state changes externally (e.g., from properties panel), update sub-widgets
connect(_state.get(), &MediaWidgetState::linePrefsChanged,
        _line_widget, &MediaLine_Widget::_syncFromState);
        
connect(_state.get(), &MediaWidgetState::displayOptionsChanged,
        this, [this](QString const& key, QString const& type) {
    if (type == "line" && _line_widget->activeKey() == key.toStdString()) {
        _line_widget->refreshDisplayFromState();
    }
});
```

---

## Phase 6: Validation and Testing

**Goal**: Comprehensive testing of state serialization and restoration

**Duration**: 1 week

### 6.0 Test Location Conventions

**Unit tests** (testing a single translation unit in isolation):
- Named `TranslationUnitName.test.cpp`
- Located in the **same folder** as the translation unit being tested
- Examples:
  - `src/WhiskerToolbox/Media_Widget/MediaWidgetStateData.test.cpp`
  - `src/WhiskerToolbox/Media_Widget/DisplayOptions/DisplayOptions.test.cpp`

**Integration tests** (testing interactions between multiple components):
- Located in `tests/WhiskerToolbox/Media_Widget/`
- Test driver: `tests/WhiskerToolbox/Media_Widget/CMakeLists.txt`
- Examples:
  - `tests/WhiskerToolbox/Media_Widget/MediaWidgetState.integration.test.cpp`
  - `tests/WhiskerToolbox/Media_Widget/WorkspaceIntegration.test.cpp`

### 6.1 Unit Tests

```cpp
// File: src/WhiskerToolbox/Media_Widget/MediaWidgetStateData.test.cpp

TEST_CASE("MediaWidgetStateData full serialization") {
    MediaWidgetStateData data;
    
    // Set up complex state
    data.displayed_media_key = "video.mp4";
    data.viewport.zoom = 2.5;
    data.enabled_lines["whisker_1"] = true;
    
    // LineDisplayOptions is now directly serializable
    LineDisplayOptions line_opts;
    line_opts.common.get().hex_color = "#ff0000";
    line_opts.common.get().alpha = 0.8f;
    line_opts.line_thickness = 3;
    line_opts.show_points = true;
    data.line_options["whisker_1"] = line_opts;
    
    data.text_overlays.push_back(TextOverlayData{
        .id = 0,
        .text = "Frame: 100",
        .x_position = 0.1f,
        .y_position = 0.1f
    });
    
    auto json = rfl::json::write(data);
    auto restored = rfl::json::read<MediaWidgetStateData>(json);
    
    REQUIRE(restored);
    REQUIRE(restored->viewport.zoom == Approx(2.5));
    REQUIRE(restored->enabled_lines.at("whisker_1") == true);
    
    // Access flattened common fields
    REQUIRE(restored->line_options.at("whisker_1").common.get().hex_color == "#ff0000");
    REQUIRE(restored->line_options.at("whisker_1").line_thickness == 3);
    REQUIRE(restored->text_overlays.size() == 1);
    REQUIRE(restored->text_overlays[0].text == "Frame: 100");
}

TEST_CASE("rfl::Flatten produces flat JSON") {
    LineDisplayOptions opts;
    opts.common.get().hex_color = "#00ff00";
    opts.common.get().alpha = 0.5f;
    opts.common.get().is_visible = true;
    opts.line_thickness = 4;
    
    auto json = rfl::json::write(opts);
    
    // Should NOT have nested "common" object
    REQUIRE(json.find("\"common\"") == std::string::npos);
    // Should have flat fields
    REQUIRE(json.find("\"hex_color\":\"#00ff00\"") != std::string::npos);
    REQUIRE(json.find("\"line_thickness\":4") != std::string::npos);
}
```

### 6.2 Integration Tests

```cpp
TEST_CASE("MediaWidgetState workspace integration") {
    auto dm = std::make_shared<DataManager>();
    WorkspaceManager workspace(dm);
    
    // Create and configure state
    auto state = std::make_shared<MediaWidgetState>();
    state->setZoom(3.0);
    state->setFeatureEnabled("line_1", "line", true);
    
    LineDisplayOptionsData line_opts;
    line_opts.hex_color = "#00ff00";
    line_opts.line_thickness = 5;
    state->setLineOptions("line_1", line_opts);
    
    workspace.registerState(state);
    
    // Serialize workspace
    auto json = workspace.toJson();
    
    // Restore to new workspace
    WorkspaceManager restored_ws(dm);
    REQUIRE(restored_ws.fromJson(json));
    
    auto states = restored_ws.getAllStates();
    REQUIRE(states.size() == 1);
    
    auto restored_state = std::dynamic_pointer_cast<MediaWidgetState>(states[0]);
    REQUIRE(restored_state);
    REQUIRE(restored_state->zoom() == Approx(3.0));
    REQUIRE(restored_state->isFeatureEnabled("line_1", "line"));
    
    auto* restored_opts = restored_state->lineOptions("line_1");
    REQUIRE(restored_opts);
    REQUIRE(restored_opts->hex_color == "#00ff00");
}
```

### 6.3 Manual Testing Checklist

- [ ] Create new Media_Widget, configure display options
- [ ] Save workspace, close application
- [ ] Reopen, restore workspace
- [ ] Verify all display options restored correctly
- [ ] Verify zoom/pan position restored
- [ ] Verify text overlays restored
- [ ] Verify enabled features restored
- [ ] Test with multiple Media_Widget instances

---

## Implementation Timeline

| Phase | Duration | Status | Deliverables |
|-------|----------|--------|--------------|
| 1. Refactor DisplayOptions | 1 day | ✅ Complete | DisplayOptions.hpp serializable with rfl::Flatten + accessor methods |
| 2. MediaWidgetStateData | 1 day | ✅ Complete | Complete state data structure + comprehensive tests |
| 3. Expanded MediaWidgetState | 1 day | ✅ Complete | Full Qt wrapper with signals + comprehensive tests |
| 4. Media_Widget Integration | 2 weeks | 🔲 Not Started | State reads/writes from Media_Widget |
| 5. Sub-Widget Integration | 2 weeks | 🔲 Not Started | All sub-widgets connected to state |
| 6. Testing | 1 week | 🔲 Not Started | Integration tests (unit tests done) |

**Total: 9 weeks** (originally estimated) → **Actual: Phases 1-3 completed in 1 day**

---

## File Structure After Refactoring

```
src/WhiskerToolbox/Media_Widget/
├── DisplayOptions/
│   ├── DisplayOptions.hpp              # ✅ MODIFIED: Serializable with rfl::Flatten, native enums
│   ├── CoordinateTypes.hpp             # Existing (unchanged)
├── MediaWidgetState.hpp                # ✅ COMPLETE: Full Qt wrapper with 60+ methods and 20+ signals
├── MediaWidgetState.cpp                # ✅ COMPLETE: ~600 lines of implementation
├── MediaWidgetState.test.cpp           # ✅ COMPLETE: ~850 lines of comprehensive tests
├── MediaWidgetStateData.hpp            # ✅ NEW: Complete reflect-cpp data struct
├── MediaWidgetStateData.test.cpp       # ✅ NEW: Comprehensive unit tests
├── Media_Widget.hpp                    # TODO: Phase 4 - State integration
├── Media_Widget.cpp                    # TODO: Phase 4 - State integration
├── Media_Window/
│   └── Media_Window.hpp                # Modified: Reads from state
├── MediaLine_Widget/
│   └── MediaLine_Widget.hpp            # Modified: Syncs with state
├── MediaMask_Widget/
│   └── MediaMask_Widget.hpp            # Modified: Syncs with state
├── MediaPoint_Widget/
│   └── MediaPoint_Widget.hpp           # Modified: Syncs with state
└── MediaText_Widget/
    └── MediaText_Widget.hpp            # Modified: Delegates to state
```

**Key simplifications vs original plan:**
- No `SerializableDisplayOptions.hpp` - DisplayOptions.hpp is directly serializable
- No `DisplayOptionsConversion.hpp` - no conversion functions needed
- No `EnumHelpers.hpp` - enums serialize natively as strings
- Single source of truth for option types

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking existing functionality | High | Incremental migration, keep existing code working |
| rfl::Flatten behavior changes | Medium | Pin reflect-cpp version, comprehensive tests |
| Performance overhead | Medium | Profile, lazy initialization of options maps |
| Signal connection complexity | Medium | Clear ownership, document signal flow |
| Large JSON files | Low | Most state is compact, text overlays are bounded |

---

## Success Criteria

1. **Full Serialization**: All display options can be saved/restored
2. **No Regression**: Existing functionality unchanged
3. **Test Coverage**: 80%+ coverage on state classes
4. **Flat JSON**: rfl::Flatten produces clean, flat JSON output
5. **No Conversion Layer**: DisplayOptions used directly (no parallel types)

---

## Next Steps After Completion

Once MediaWidgetState is fully extracted:

1. **Properties Panel Separation** (Phase 3 of main roadmap):
   - Create dedicated properties widgets that read/write MediaWidgetState
   - Remove Feature_Table_Widget from Media_Widget
   - Properties panel observes state via signals

2. **Multiple Media_Widget Instances**:
   - Each instance has its own state
   - Can open same media with different display options

3. **Undo/Redo Support**:
   - Command pattern wraps state changes
   - Commands serialize to JSON for undo stack

---

## References

- [reflect-cpp Documentation](https://github.com/getml/reflect-cpp)
- [rfl::Flatten Documentation](https://rfl.getml.com/flatten_structs/)
- [reflect-cpp Enum Serialization](https://rfl.getml.com/enums/) - Enums serialize as strings automatically
- [EditorState Base Class](../../../src/WhiskerToolbox/EditorState/EditorState.hpp)
- [Main Refactoring Roadmap](./EDITOR_STATE_REFACTORING_ROADMAP.md)
