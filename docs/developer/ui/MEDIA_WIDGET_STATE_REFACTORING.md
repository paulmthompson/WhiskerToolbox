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

## Phase 1: Refactor DisplayOptions to be Serializable

**Goal**: Modify existing DisplayOptions structs to be directly serializable with reflect-cpp (no inheritance, no conversion functions)

**Duration**: 1 week

### 1.1 Refactor DisplayOptions.hpp

Replace the inheritance-based approach with composition using `rfl::Flatten`:

```cpp
// src/WhiskerToolbox/Media_Widget/DisplayOptions/DisplayOptions.hpp

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <string>

// ==================== Enums ====================
// reflect-cpp natively serializes enums as strings (e.g., PointMarkerShape::Circle → "Circle")
// No rfl::Literal or special handling needed!

enum class PointMarkerShape { Circle, Square, Triangle, Cross, X, Diamond };
enum class IntervalPlottingStyle { Box, Border };
enum class IntervalLocation { TopLeft, TopRight, BottomLeft, BottomRight };
enum class ColormapType {
    None, Jet, Hot, Cool, Spring, Summer, Autumn, Winter,
    Rainbow, Ocean, Pink, HSV, Parula, Viridis, Plasma,
    Inferno, Magma, Turbo, Red, Green, Blue, Cyan, Magenta, Yellow
};

// ==================== Processing Options ====================
// These are already flat structs - just ensure they're serializable

struct ContrastOptions {
    bool active = false;
    double alpha = 1.0;
    int beta = 0;
    double display_min = 0.0;
    double display_max = 255.0;
    
    // Runtime method - not serialized
    void calculateAlphaBetaFromMinMax();
    void calculateMinMaxFromAlphaBeta();
};

struct GammaOptions {
    bool active = false;
    double gamma = 1.0;
};

struct SharpenOptions {
    bool active = false;
    double sigma = 3.0;
};

struct ClaheOptions {
    bool active = false;
    int grid_size = 8;
    double clip_limit = 2.0;
};

struct BilateralOptions {
    bool active = false;
    int diameter = 5;
    double sigma_color = 20.0;
    double sigma_spatial = 20.0;
};

struct MedianOptions {
    bool active = false;
    int kernel_size = 5;
};

struct ColormapOptions {
    bool active = false;
    ColormapType colormap = ColormapType::None;  // Serializes as "None"
    double alpha = 1.0;
    bool normalize = true;
};

// MagicEraserOptions contains runtime mask data - NOT serialized
// Keep separate for runtime use only

// ==================== Common Display Fields ====================
// Shared fields that appear in multiple display option types
// Use rfl::Flatten to compose into other structs

struct CommonDisplayFields {
    std::string hex_color = "#007bff";
    float alpha = 1.0f;
    bool is_visible = false;
};

// ==================== Display Options (Composition, not Inheritance) ====================

struct MediaDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // Flatten to replace inheritance
    ContrastOptions contrast;                   // Nested object in JSON
    GammaOptions gamma;                         // Nested object in JSON
    SharpenOptions sharpen;                     // Nested object in JSON
    ClaheOptions clahe;                         // Nested object in JSON
    BilateralOptions bilateral;                 // Nested object in JSON
    MedianOptions median;                       // Nested object in JSON
    ColormapOptions colormap;                   // Nested object in JSON
    // Note: MagicEraser excluded - contains runtime mask data
};

struct PointDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // Flatten to replace inheritance
    int point_size = 5;
    PointMarkerShape marker_shape = PointMarkerShape::Circle;  // Serializes as "Circle"
};

struct LineDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // Flatten to replace inheritance
    int line_thickness = 2;
    bool show_points = false;
    bool edge_snapping = false;
    bool show_position_marker = false;
    int position_percentage = 20;
    bool show_segment = false;
    int segment_start_percentage = 0;
    int segment_end_percentage = 100;
    // Note: selected_line_index is transient, not serialized
};

struct MaskDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // Flatten to replace inheritance
    bool show_bounding_box = false;
    bool show_outline = false;
    bool use_as_transparency = false;
};

struct TensorDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // Flatten to replace inheritance
    int display_channel = 0;
};

struct DigitalIntervalDisplayOptions {
    rfl::Flatten<CommonDisplayFields> common;  // Flatten to replace inheritance
    IntervalPlottingStyle plotting_style = IntervalPlottingStyle::Box;  // Serializes as "Box"
    int box_size = 20;
    int frame_range = 2;
    IntervalLocation location = IntervalLocation::TopRight;  // Serializes as "TopRight"
    int border_thickness = 5;
};
```

Example JSON for `MediaDisplayOptions`:
```json
{
  "enabled": true,
  "is_visible": true,
  "hex_color": "#007bff",
  "alpha": 1.0,
  "contrast": {
    "active": true,
    "alpha": 1.5,
    "beta": 10,
    "display_min": 0.0,
    "display_max": 200.0
  },
  "gamma": {
    "active": false,
    "gamma": 1.0
  },
  "colormap": {
    "active": true,
    "colormap": "Viridis",
    "alpha": 0.8,
    "normalize": true
  }
}
```

Only `CommonDisplayFields` is flattened (so `enabled`, `is_visible`, `hex_color`, `alpha` appear at the top level). Processing options remain as nested objects for clarity.

### 1.2 Native Enum Serialization

**No conversion helpers needed!** reflect-cpp automatically serializes C++ enums as strings:

```cpp
// Example: enum serialization is automatic
enum class PointMarkerShape { Circle, Square, Triangle, Cross, X, Diamond };

struct PointDisplayOptions {
    PointMarkerShape marker_shape = PointMarkerShape::Diamond;
};

// Serializes to: {"marker_shape":"Diamond"}
// Deserializes from: {"marker_shape":"Diamond"} → PointMarkerShape::Diamond
```

This means:
- Use native `enum class` types directly in structs
- No `rfl::Literal` type aliases needed
- No conversion functions between strings and enums
- Switch statements work naturally with enum values

### 1.3 Update Existing Code

Existing code that accesses `opts.hex_color` now accesses `opts.common.get().hex_color` or use a helper:

```cpp
// Helper to access common fields
template<typename T>
CommonDisplayFields& getCommon(T& opts) {
    return opts.common.get();
}

template<typename T>
CommonDisplayFields const& getCommon(T const& opts) {
    return opts.common.get();
}

// Usage:
QRgb plot_color_with_alpha(LineDisplayOptions const& opts) {
    auto const& c = getCommon(opts);
    QColor color(QString::fromStdString(c.hex_color));
    color.setAlphaF(c.alpha);
    return color.rgba();
}
```

### 1.4 Unit Tests

```cpp
TEST_CASE("LineDisplayOptions serialization") {
    LineDisplayOptions opts;
    opts.common.get().hex_color = "#ff0000";
    opts.common.get().alpha = 0.8f;
    opts.line_thickness = 3;
    opts.show_points = true;
    
    auto json = rfl::json::write(opts);
    
    // Verify flat JSON structure (no nested "common" object)
    REQUIRE(json.find("\"hex_color\":\"#ff0000\"") != std::string::npos);
    REQUIRE(json.find("\"line_thickness\":3") != std::string::npos);
    
    auto restored = rfl::json::read<LineDisplayOptions>(json);
    REQUIRE(restored);
    REQUIRE(restored->common.get().hex_color == "#ff0000");
    REQUIRE(restored->line_thickness == 3);
}
```

---

## Phase 2: Create Comprehensive MediaWidgetStateData

**Goal**: Define the full state structure that MediaWidgetState will serialize

**Duration**: 1 week

### 2.1 Define State Structure

Use nested objects for clarity in the JSON output. `rfl::Flatten` is used within DisplayOptions (Phase 1) but **not** at the top-level state structure:

```cpp
// src/WhiskerToolbox/Media_Widget/MediaWidgetStateData.hpp

#include "DisplayOptions/DisplayOptions.hpp"
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <map>
#include <string>
#include <vector>

// ==================== Text Overlay Data ====================

enum class TextOrientation { Horizontal, Vertical };

struct TextOverlayData {
    int id = -1;
    std::string text;
    TextOrientation orientation = TextOrientation::Horizontal;  // Serializes as "Horizontal"
    float x_position = 0.5f;
    float y_position = 0.5f;
    std::string color = "#ffffff";
    int font_size = 12;
    bool enabled = true;
};

// ==================== Interaction Preferences ====================
// User preferences for tools (not per-feature state)

struct LineInteractionPrefs {
    std::string smoothing_mode = "SimpleSmooth";  // "SimpleSmooth" or "PolynomialFit"
    int polynomial_order = 3;
    bool edge_snapping_enabled = false;
    int edge_threshold = 100;
    int edge_search_radius = 20;
    int eraser_radius = 10;
    float selection_threshold = 15.0f;
};

struct MaskInteractionPrefs {
    int brush_size = 15;
    bool hover_circle_visible = true;
    bool allow_empty_mask = false;
};

struct PointInteractionPrefs {
    float selection_threshold = 10.0f;
};

// ==================== Viewport State ====================

struct ViewportState {
    double zoom = 1.0;
    double pan_x = 0.0;
    double pan_y = 0.0;
    int canvas_width = 640;
    int canvas_height = 480;
};

// ==================== Tool Mode Enums ====================
// Native enums serialize as strings automatically

enum class LineMode { None, Add, Erase, Select, DrawAllFrames };
enum class MaskMode { None, Brush };
enum class PointMode { None, Select };

// ==================== Main State Structure ====================

/**
 * @brief Complete serializable state for MediaWidget
 * 
 * This struct contains all persistent state that should be saved/restored
 * when serializing a workspace. Transient state (hover positions, active
 * drag operations, etc.) is intentionally excluded.
 * 
 * Uses nested objects (not rfl::Flatten) for clear JSON structure.
 */
struct MediaWidgetStateData {
    // === Identity ===
    std::string instance_id;
    std::string display_name = "Media Viewer";
    
    // === Primary Display ===
    std::string displayed_media_key;
    
    // === Viewport (nested object) ===
    ViewportState viewport;
    
    // === Per-Feature Display Options ===
    // Each DisplayOptions contains 'enabled' field via CommonDisplayFields
    std::map<std::string, MediaDisplayOptions> media_options;
    std::map<std::string, LineDisplayOptions> line_options;
    std::map<std::string, MaskDisplayOptions> mask_options;
    std::map<std::string, PointDisplayOptions> point_options;
    std::map<std::string, DigitalIntervalDisplayOptions> interval_options;
    std::map<std::string, TensorDisplayOptions> tensor_options;
    
    // === Interaction Preferences (nested objects) ===
    LineInteractionPrefs line_prefs;
    MaskInteractionPrefs mask_prefs;
    PointInteractionPrefs point_prefs;
    
    // === Text Overlays ===
    std::vector<TextOverlayData> text_overlays;
    int next_overlay_id = 0;
    
    // === Active Tool State ===
    LineMode active_line_mode = LineMode::None;  // Serializes as "None"
    MaskMode active_mask_mode = MaskMode::None;
    PointMode active_point_mode = PointMode::None;
};
```

### 2.2 Example JSON Output

The nested structure produces clear, readable JSON:

```json
{
  "instance_id": "abc123",
  "display_name": "Media Viewer",
  "displayed_media_key": "video.mp4",
  "viewport": {
    "zoom": 2.0,
    "pan_x": 100.0,
    "pan_y": 50.0,
    "canvas_width": 1920,
    "canvas_height": 1080
  },
  "enabled_lines": {
    "whisker_1": true,
    "whisker_2": true
  },
  "line_options": {
    "whisker_1": {
      "hex_color": "#ff0000",
      "alpha": 0.8,
      "is_visible": true,
      "line_thickness": 3,
      "show_points": true
    }
  },
  "line_prefs": {
    "smoothing_mode": "PolynomialFit",
    "polynomial_order": 5,
    "edge_snapping_enabled": true
  },
  "text_overlays": [
    {
      "id": 0,
      "text": "Frame: 100",
      "orientation": "Horizontal",
      "x_position": 0.1,
      "y_position": 0.1
    }
  ],
  "active_line_mode": "Add"
}
```

Note: Within `line_options`, the `LineDisplayOptions` struct uses `rfl::Flatten<CommonDisplayFields>`, so its common fields (`enabled`, `is_visible`, `hex_color`, `alpha`) appear flat alongside type-specific fields (`line_thickness`, `show_points`).

### 2.3 State Size Estimation

The structure serializes to approximately:
- Base structure: ~500 bytes
- Per enabled feature: ~200-500 bytes
- Text overlays: ~100 bytes each

Typical session (5-10 features): **2-5 KB** of JSON

---

## Phase 3: Expand MediaWidgetState Class

**Goal**: Implement the Qt wrapper that holds `MediaWidgetStateData` and provides typed signals

**Duration**: 2 weeks

### 3.1 Using rfl::Flatten for EditorState Composition

The key insight: `MediaWidgetState` IS-A `EditorState` in Qt terms (for signals/slots), but the **data** uses composition with `rfl::Flatten`:

```cpp
// The data struct uses Flatten for EditorState base data
struct EditorStateBaseData {
    std::string instance_id;
    std::string type_name;
};

struct MediaWidgetStateData {
    rfl::Flatten<EditorStateBaseData> base;  // Flattened into JSON
    std::string display_name = "Media Viewer";
    std::string displayed_media_key;
    ViewportState viewport;
    // ... rest of state
};

// The Qt class still uses inheritance for signals/slots
class MediaWidgetState : public EditorState {
    Q_OBJECT
    // ...
private:
    MediaWidgetStateData _data;  // Composition for data
};
```

### 3.2 Extended MediaWidgetState Interface

```cpp
// src/WhiskerToolbox/Media_Widget/MediaWidgetState.hpp (expanded)

#include "EditorState/EditorState.hpp"
#include "MediaWidgetStateData.hpp"

#include <QObject>

class MediaWidgetState : public EditorState {
    Q_OBJECT

public:
    explicit MediaWidgetState(QObject* parent = nullptr);
    ~MediaWidgetState() override = default;

    // === Type Identification ===
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("MediaWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const& name) override;

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const& json) override;
    
    // === Direct Data Access (for efficiency) ===
    [[nodiscard]] MediaWidgetStateData const& data() const { return _data; }

    // === Viewport State ===
    void setZoom(double zoom);
    [[nodiscard]] double zoom() const { return _data.viewport.zoom; }
    
    void setPan(double x, double y);
    [[nodiscard]] std::pair<double, double> pan() const;
    
    void setCanvasSize(int width, int height);
    [[nodiscard]] std::pair<int, int> canvasSize() const;

    // === Feature Management ===
    void setFeatureEnabled(QString const& data_key, QString const& data_type, bool enabled);
    [[nodiscard]] bool isFeatureEnabled(QString const& data_key, QString const& data_type) const;
    [[nodiscard]] QStringList enabledFeatures(QString const& data_type) const;

    // === Display Options Access ===
    // Returns pointer for read; use setter for modification
    [[nodiscard]] LineDisplayOptions const* lineOptions(QString const& key) const;
    void setLineOptions(QString const& key, LineDisplayOptions const& options);
    
    [[nodiscard]] MaskDisplayOptions const* maskOptions(QString const& key) const;
    void setMaskOptions(QString const& key, MaskDisplayOptions const& options);
    
    [[nodiscard]] PointDisplayOptions const* pointOptions(QString const& key) const;
    void setPointOptions(QString const& key, PointDisplayOptions const& options);
    
    // ... similar for other types ...

    // === Interaction Preferences ===
    [[nodiscard]] LineInteractionPrefs const& linePrefs() const { return _data.line_prefs; }
    void setLinePrefs(LineInteractionPrefs const& prefs);
    
    [[nodiscard]] MaskInteractionPrefs const& maskPrefs() const { return _data.mask_prefs; }
    void setMaskPrefs(MaskInteractionPrefs const& prefs);
    
    [[nodiscard]] PointInteractionPrefs const& pointPrefs() const { return _data.point_prefs; }
    void setPointPrefs(PointInteractionPrefs const& prefs);

    // === Text Overlays ===
    [[nodiscard]] std::vector<TextOverlayData> const& textOverlays() const { return _data.text_overlays; }
    int addTextOverlay(TextOverlayData overlay);  // Returns assigned ID
    void removeTextOverlay(int overlay_id);
    void updateTextOverlay(int overlay_id, TextOverlayData const& overlay);
    void clearTextOverlays();

    // === Active Tool State ===
    void setActiveLineMode(QString const& mode);
    [[nodiscard]] QString activeLineMode() const;
    
    void setActiveMaskMode(QString const& mode);
    [[nodiscard]] QString activeMaskMode() const;

signals:
    // === Viewport Signals ===
    void zoomChanged(double zoom);
    void panChanged(double x, double y);
    void canvasSizeChanged(int width, int height);

    // === Feature Signals ===
    void featureEnabledChanged(QString const& data_key, QString const& data_type, bool enabled);
    void displayOptionsChanged(QString const& data_key, QString const& data_type);

    // === Interaction Preference Signals ===
    void linePrefsChanged();
    void maskPrefsChanged();
    void pointPrefsChanged();

    // === Text Overlay Signals ===
    void textOverlayAdded(int overlay_id);
    void textOverlayRemoved(int overlay_id);
    void textOverlayUpdated(int overlay_id);
    void textOverlaysCleared();

    // === Tool Mode Signals ===
    void activeLineModeChanged(QString const& mode);
    void activeMaskModeChanged(QString const& mode);

private:
    MediaWidgetStateData _data;
};
```

### 3.3 Implementation Pattern

```cpp
// MediaWidgetState.cpp

void MediaWidgetState::setZoom(double zoom) {
    if (std::abs(_data.viewport.zoom - zoom) > 1e-6) {
        _data.viewport.zoom = zoom;
        markDirty();
        emit zoomChanged(zoom);
    }
}

void MediaWidgetState::setLineOptions(QString const& key, LineDisplayOptions const& options) {
    std::string key_std = key.toStdString();
    _data.line_options[key_std] = options;
    markDirty();
    emit displayOptionsChanged(key, QStringLiteral("line"));
}

int MediaWidgetState::addTextOverlay(TextOverlayData overlay) {
    overlay.id = _data.next_overlay_id++;
    _data.text_overlays.push_back(overlay);
    markDirty();
    emit textOverlayAdded(overlay.id);
    return overlay.id;
}

std::string MediaWidgetState::toJson() const {
    return rfl::json::write(_data);
}

bool MediaWidgetState::fromJson(std::string const& json) {
    auto result = rfl::json::read<MediaWidgetStateData>(json);
    if (result) {
        _data = std::move(*result);
        
        // Restore instance ID from data
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        
        emit stateChanged();
        return true;
    }
    return false;
}
```

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

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| 1. Refactor DisplayOptions | 1 week | Make DisplayOptions.hpp serializable with rfl::Flatten |
| 2. MediaWidgetStateData | 1 week | Complete state data structure |
| 3. Expanded MediaWidgetState | 2 weeks | Full Qt wrapper with signals |
| 4. Media_Widget Integration | 2 weeks | State reads/writes from Media_Widget |
| 5. Sub-Widget Integration | 2 weeks | All sub-widgets connected to state |
| 6. Testing | 1 week | Unit tests, integration tests |

**Total: 9 weeks**

---

## File Structure After Refactoring

```
src/WhiskerToolbox/Media_Widget/
├── DisplayOptions/
│   ├── DisplayOptions.hpp              # MODIFIED: Serializable with rfl::Flatten, native enums
│   ├── CoordinateTypes.hpp             # Existing (unchanged)
├── MediaWidgetState.hpp                # EXPANDED: Full state interface
├── MediaWidgetState.cpp                # EXPANDED: Full implementation
├── MediaWidgetStateData.hpp            # NEW: reflect-cpp data struct
├── Media_Widget.hpp                    # Modified: Uses state
├── Media_Widget.cpp                    # Modified: State integration
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
