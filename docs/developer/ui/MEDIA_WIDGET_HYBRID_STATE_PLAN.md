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

**Status**: ✅ Complete (January 2026)

**Duration**: 2-3 days

### Step 4C.1: Remove duplicate storage from Media_Window

### Step 4C.2: Update Media_Window methods

All `add*ToScene` and `_plot*Data` methods read from state instead of local maps.

### Step 4C.3: Update accessor methods

Methods like `getLineConfig()` forward to state's display options registry.

---

## Phase 4D: Wire Up Sub-Widgets

**Goal**: Sub-widgets read/write display options through state.

**Status**: ✅ Complete (January 2026)

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

**Status**: ✅ Complete (January 2026)

**Duration**: 1 day

### Implementation Summary

**Key Changes Made:**

1. **Removed duplicate state storage** from Media_Widget:
   - Removed `_current_zoom` member variable
   - Removed `_user_zoom_active` member variable
   - Made zoom constants (`_zoom_step`, `_min_zoom`, `_max_zoom`) `constexpr`

2. **Made state the single source of truth** for zoom/pan:
   - `_applyZoom()` now reads current zoom from `_state->zoom()` and writes back via `_state->setZoom()`
   - `resetZoom()` directly sets `_state->setZoom(1.0)` instead of syncing
   - Pan changes during drag update state directly via `_state->setPan()`

3. **Added helper method** `_isUserZoomActive()`:
   - Derives from `_state->zoom() != 1.0` instead of storing separate flag
   - Used in `resizeEvent()` and `_updateCanvasSize()`

4. **Removed intermediate sync methods**:
   - Deleted `_syncZoomToState()` (zoom is set directly to state)
   - Deleted `_syncPanToState()` (pan is set directly to state)
   - Kept `_syncCanvasSizeToState()` (canvas size is still derived from scene)

5. **Simplified state signal handlers**:
   - `_onStateZoomChanged()` reads current transform from QGraphicsView instead of comparing to local variable
   - `_onStatePanChanged()` remains unchanged (reads from scrollbars)

6. **Updated restoreFromState()**:
   - Removed assignments to deleted member variables
   - Applies zoom/pan directly from state to QGraphicsView

### Code Examples

```cpp
// Before: Duplicate storage
double _current_zoom{1.0};
bool _user_zoom_active{false};

// After: State is the source of truth
// (No member variables - read from _state->zoom())

bool Media_Widget::_isUserZoomActive() const {
    if (!_state) return false;
    return std::abs(_state->zoom() - 1.0) > 1e-6;
}

void Media_Widget::_applyZoom(double factor, bool anchor_under_mouse) {
    if (!ui->graphicsView || !_state) return;
    double current_zoom = _state->zoom();  // Read from state
    double new_zoom = current_zoom * factor;
    new_zoom = std::clamp(new_zoom, _min_zoom, _max_zoom);
    // ... apply to QGraphicsView ...
    _state->setZoom(new_zoom);  // Write to state
}
```

---

## Summary Timeline

| Phase | Duration | Status | Deliverables |
|-------|----------|--------|--------------|
| 4A: DisplayOptionsRegistry | 3-4 days | ✅ Complete | Generic registry, deprecate old methods |
| 4B: Simplify Accessors | 1-2 days | ✅ Complete | Consolidated signals, audit complete |
| 4C: Media_Window Integration | 2-3 days | ✅ Complete | Remove duplicate storage, use state |
| 4D: Sub-Widget Integration | 3-4 days | ✅ Complete | All 6 sub-widgets wired up |
| 4E: Viewport Integration | 1 day | ✅ Complete | State as single source of truth for zoom/pan |
| **Total** | **~2 weeks** | **✅ Complete** | **Full hybrid architecture achieved** |

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
- `Media_Widget.hpp` - Remove `_current_zoom`, `_user_zoom_active`; add `_isUserZoomActive()` helper
- `Media_Widget.cpp` - State as single source of truth for zoom/pan; remove sync methods
- `CMakeLists.txt` - Add new source files

---

## Success Criteria

1. ✅ **Method reduction**: ~60 methods → ~25 methods
2. ✅ **Signal reduction**: ~20 signals → ~8 signals
3. ✅ **Single source of truth**: Display options only in MediaWidgetStateData
4. ✅ **No duplicate storage**: Media_Window doesn't have its own option maps; Media_Widget doesn't duplicate zoom/pan
5. ✅ **All tests passing**: Existing + new DisplayOptionsRegistry tests
6. ✅ **Serialization works**: Workspace save/restore functions correctly with zoom/pan persistence

## Conclusion

The MediaWidgetState hybrid architecture refactoring is now **complete**. All phases (4A-4E) have been successfully implemented and tested. The state class now provides:

- A clean, generic `DisplayOptionsRegistry` for all 6 display option types
- Consolidated signals (3 category signals vs. 10+ individual signals)
- Single source of truth for all widget state (display options, viewport, preferences, overlays, tool modes)
- No duplicate storage between state and widgets
- Full workspace serialization support

The architecture is ready for production use.
