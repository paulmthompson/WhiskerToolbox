# MediaWidgetState Hybrid Architecture Plan

## Overview

This document describes the incremental plan to refactor `MediaWidgetState` from a monolithic class with 60+ methods to a cleaner hybrid architecture with ~25 methods using a generic `DisplayOptionsRegistry`.

## Current Assessment

### What MediaWidgetState Actually Needs

1. **Serialize/deserialize** for workspace save/restore
2. **Store display options** (colors, visibility, thickness, etc.) - 6 types
3. **Store interaction preferences** (brush size, edge snapping, etc.)
4. **Store viewport state** (zoom, pan)
5. **Store text overlays**
6. **Store active tool modes**

### What It Does NOT Need

- Data change notification (DataManager handles this)
- Inter-widget communication (WorkspaceManager handles this)
- Complex intra-widget state synchronization (case-by-case is fine)

## Target Architecture

```
MediaWidgetState (EditorState)
├── _data: MediaWidgetStateData          // Single serializable struct (unchanged)
├── _display_options: DisplayOptionsRegistry  // Generic for all 6 types
├── Viewport accessors (5 methods)       // Direct, not a slice
├── Interaction prefs accessors (6 methods)  // Direct, not a slice  
├── Text overlay accessors (6 methods)   // Direct, not a slice
├── Tool mode accessors (6 methods)      // Direct, not a slice
└── toJson() / fromJson()                // Unchanged
```

**Method count reduction**: ~60 methods → ~25 methods (primarily by genericizing display options)

---

## Phase 4A: Introduce DisplayOptionsRegistry

**Goal**: Replace 6 sets of display option methods with a single generic registry.

**Status**: ✅ Complete (January 2026)

**Duration**: 3-4 days

### Step 4A.1: Create DisplayOptionsRegistry class

Create a new class that provides type-safe generic access to all display option types:

```cpp
// DisplayOptionsRegistry.hpp
class DisplayOptionsRegistry : public QObject {
    Q_OBJECT
public:
    explicit DisplayOptionsRegistry(MediaWidgetStateData* data, QObject* parent = nullptr);
    
    // === Generic type-safe API ===
    
    template<typename T>
    void set(QString const& key, T const& options);
    
    template<typename T>
    T const* get(QString const& key) const;
    
    template<typename T>
    bool remove(QString const& key);
    
    template<typename T>
    bool has(QString const& key) const;
    
    template<typename T>
    QStringList keys() const;
    
    template<typename T>
    QStringList enabledKeys() const;  // Where is_visible == true
    
    // === Visibility convenience ===
    
    void setVisible(QString const& key, QString const& type, bool visible);
    bool isVisible(QString const& key, QString const& type) const;
    
signals:
    void optionsChanged(QString const& key, QString const& type);
    void optionsRemoved(QString const& key, QString const& type);
    void visibilityChanged(QString const& key, QString const& type, bool visible);
    
private:
    MediaWidgetStateData* _data;  // Non-owning, points to parent's _data
};
```

### Step 4A.2: Implement with template specializations

Each display option type gets explicit template specializations for type safety.

### Step 4A.3: Add to MediaWidgetState

```cpp
// MediaWidgetState.hpp changes
class MediaWidgetState : public EditorState {
public:
    // NEW: Generic display options access
    DisplayOptionsRegistry& displayOptions() { return _display_options; }
    DisplayOptionsRegistry const& displayOptions() const { return _display_options; }
    
    // DEPRECATED but still working: Old typed accessors (forward to registry)
    [[deprecated("Use displayOptions().get<LineDisplayOptions>(key)")]]
    LineDisplayOptions const* lineOptions(QString const& key) const;
    
private:
    DisplayOptionsRegistry _display_options{&_data, this};
};
```

### Step 4A.4: Update call sites incrementally

**Example - Media_Window.cpp**:
```cpp
// Before
if (auto const* opts = _media_widget_state->lineOptions(key)) { ... }

// After  
if (auto const* opts = _media_widget_state->displayOptions().get<LineDisplayOptions>(key)) { ... }
```

### Step 4A.5: Tests for DisplayOptionsRegistry

Create unit tests covering:
- set/get round-trip for all 6 types
- remove functionality
- keys() and enabledKeys()
- Signal emission verification
- Type safety (can't get wrong type)

### Files to Create

- `src/WhiskerToolbox/Media_Widget/DisplayOptionsRegistry.hpp`
- `src/WhiskerToolbox/Media_Widget/DisplayOptionsRegistry.cpp`
- `src/WhiskerToolbox/Media_Widget/DisplayOptionsRegistry.test.cpp`

### Files to Modify

- `src/WhiskerToolbox/Media_Widget/MediaWidgetState.hpp`
- `src/WhiskerToolbox/Media_Widget/MediaWidgetState.cpp`
- `src/WhiskerToolbox/Media_Widget/CMakeLists.txt`

---

## Phase 4B: Simplify Remaining Accessors

**Goal**: Keep direct accessors for non-display-option state, but consolidate signals.

**Status**: ✅ Complete (January 2026)

**Duration**: 1-2 days

### Step 4B.1: Audit remaining methods

| Category | Methods | Keep As-Is? |
|----------|---------|-------------|
| Viewport | `setZoom`, `zoom`, `setPan`, `pan`, `setCanvasSize`, `canvasSize`, `setViewport`, `viewport` | ✅ Yes - only 8 methods |
| Interaction Prefs | `setLinePrefs`, `linePrefs`, `setMaskPrefs`, `maskPrefs`, `setPointPrefs`, `pointPrefs` | ✅ Yes - only 6 methods |
| Text Overlays | `addTextOverlay`, `removeTextOverlay`, `updateTextOverlay`, `clearTextOverlays`, `getTextOverlay`, `textOverlays` | ✅ Yes - only 6 methods |
| Tool Modes | `setActiveLineMode`, `activeLineMode`, etc. | ✅ Yes - only 6 methods |

**Total non-display-options methods: ~26** - manageable without further abstraction.

### Step 4B.2: Consolidate signals

Replaced fine-grained signals with 3 new consolidated signals:

```cpp
signals:
    // === Consolidated category signals (ONLY) ===
    void interactionPrefsChanged(QString const& category);  // "line", "mask", "point"
    void textOverlaysChanged();  // After any add/remove/update/clear
    void toolModesChanged(QString const& category);  // "line", "mask", "point"
    
    // === Display options (from registry) ===
    // optionsChanged, optionsRemoved, visibilityChanged - from DisplayOptionsRegistry
```

Old fine-grained signals (`linePrefsChanged`, `maskPrefsChanged`, `pointPrefsChanged`, `textOverlayAdded`, `textOverlayRemoved`, `textOverlayUpdated`, `textOverlaysCleared`, `activeLineModeChanged`, `activeMaskModeChanged`, `activePointModeChanged`) have been completely removed.

### Step 4B.3: Tests updated

Existing test cases have been removed and consolidated signals are now the only ones tested:
- `interactionPrefsChanged` verified with correct category for line/mask/point prefs
- `textOverlaysChanged` verified on add/remove/update/clear operations
- `toolModesChanged` verified with correct category for line/mask/point modes
- No spurious emissions when setting same value

---

## Phase 4C: Wire Up Media_Window

**Goal**: Media_Window uses state for display options instead of local maps.

**Status**: 🔲 Not Started

**Duration**: 2-3 days

### Step 4C.1: Remove duplicate storage from Media_Window

Remove these member variables:
```cpp
// REMOVE from Media_Window.hpp:
std::unordered_map<std::string, std::unique_ptr<LineDisplayOptions>> _line_configs;
std::unordered_map<std::string, std::unique_ptr<MaskDisplayOptions>> _mask_configs;
std::unordered_map<std::string, std::unique_ptr<PointDisplayOptions>> _point_configs;
std::unordered_map<std::string, std::unique_ptr<TensorDisplayOptions>> _tensor_configs;
std::unordered_map<std::string, std::unique_ptr<DigitalIntervalDisplayOptions>> _interval_configs;
```

### Step 4C.2: Update Media_Window methods

All `add*ToScene` and `_plot*Data` methods read from state instead of local maps.

### Step 4C.3: Update accessor methods

Methods like `getLineConfig()` forward to state's display options registry.

---

## Phase 4D: Wire Up Sub-Widgets

**Goal**: Sub-widgets read/write display options through state.

**Status**: 🔲 Not Started

**Duration**: 3-4 days

### Pattern for each sub-widget

Each sub-widget needs:
1. Pointer to state (set by Media_Widget)
2. Read options on show/setActiveKey
3. Write options when UI changes

### Estimated effort per sub-widget

| Widget | Complexity | Estimated Time |
|--------|------------|----------------|
| MediaLine_Widget | Medium (many options) | 2-3 hours |
| MediaMask_Widget | Low | 1-2 hours |
| MediaPoint_Widget | Low | 1 hour |
| MediaTensor_Widget | Low | 1 hour |
| MediaInterval_Widget | Low | 1 hour |
| MediaProcessing_Widget | Medium (media options) | 2-3 hours |

---

## Phase 4E: Viewport Integration

**Goal**: Media_Widget syncs zoom/pan with state for persistence.

**Status**: 🔲 Not Started

**Duration**: 1 day

### Implementation

```cpp
void Media_Widget::_applyZoom(double factor, bool anchor_under_mouse) {
    // ... existing zoom logic ...
    _current_zoom = new_zoom;
    
    // Sync to state
    if (_state) {
        _state->setZoom(_current_zoom);
    }
}

void Media_Widget::restoreFromState() {
    if (!_state) return;
    
    _current_zoom = _state->zoom();
    auto [pan_x, pan_y] = _state->pan();
    
    // Apply to graphics view
    // ...
}
```

---

## Summary Timeline

| Phase | Duration | Status | Deliverables |
|-------|----------|--------|--------------|
| 4A: DisplayOptionsRegistry | 3-4 days | ✅ Complete | Generic registry, deprecate old methods |
| 4B: Simplify Accessors | 1-2 days | ✅ Complete | Consolidated signals, audit complete |
| 4C: Media_Window Integration | 2-3 days | 🔲 Not Started | Remove duplicate storage, use state |
| 4D: Sub-Widget Integration | 3-4 days | 🔲 Not Started | All 6 sub-widgets wired up |
| 4E: Viewport Integration | 1 day | 🔲 Not Started | Zoom/pan persistence |
| **Total** | **~2 weeks** | | Full hybrid architecture |

---

## Files Summary

### New Files

- `DisplayOptionsRegistry.hpp`
- `DisplayOptionsRegistry.cpp`
- `DisplayOptionsRegistry.test.cpp`

### Modified Files

- `MediaWidgetState.hpp` - Add registry, deprecate old methods
- `MediaWidgetState.cpp` - Forward deprecated methods to registry
- `Media_Window.hpp` - Remove `_*_configs` maps
- `Media_Window.cpp` - Use state for all display options
- `MediaLine_Widget.cpp` - Read/write via state
- `MediaMask_Widget.cpp` - Read/write via state
- `MediaPoint_Widget.cpp` - Read/write via state
- `MediaTensor_Widget.cpp` - Read/write via state
- `MediaInterval_Widget.cpp` - Read/write via state
- `MediaProcessing_Widget.cpp` - Read/write via state
- `Media_Widget.cpp` - Viewport sync, pass state to sub-widgets
- `CMakeLists.txt` - Add new source files

---

## Success Criteria

1. **Method reduction**: ~60 methods → ~25 methods
2. **Signal reduction**: ~20 signals → ~8 signals
3. **Single source of truth**: Display options only in MediaWidgetStateData
4. **No duplicate storage**: Media_Window doesn't have its own option maps
5. **All tests passing**: Existing + new DisplayOptionsRegistry tests
6. **Serialization works**: Workspace save/restore functions correctly
