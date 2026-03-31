# Keymap System — Implementation Roadmap

**Last updated:** 2026-03-30

## Overview

This roadmap breaks the Keymap System into incremental steps that each produce a working, testable result. Each step can be merged independently — later steps build on earlier ones but the system is functional after any completed step.

**Guiding principles:**

- No existing behavior changes until explicitly migrated (hardcoded keys keep working alongside the new system)
- Each step is buildable and testable in isolation
- Widget adoption is incremental — unmigrated widgets continue using `keyPressEvent()` as before

---

## Phase 1: Core Infrastructure

### Step 1.1 — Create the `KeymapSystem` static library

**Goal:** Define the core types and `KeymapManager` class without wiring it into the application.

**New files:**

| File | Contents |
|------|----------|
| `src/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | Static library. Depends on `Qt6::Core`, `ParameterSchema`, `EditorState` (for strong types). |
| `src/WhiskerToolbox/KeymapSystem/KeyAction.hpp` | `KeyActionScope` (enum/variant), `KeyActionDescriptor` (action metadata). |
| `src/WhiskerToolbox/KeymapSystem/Keymap.hpp` | `KeymapEntry` (action→key binding), `KeymapOverrideEntry` (serializable user override). |
| `src/WhiskerToolbox/KeymapSystem/KeyActionHandler.hpp` | `KeyActionHandler` mixin with virtual `handleKeyAction(QString) → bool`. |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` | Class declaration. |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.cpp` | Implementation: action registration, scope resolution, conflict detection, override management. |

**`KeyActionScope` design:**

```cpp
enum class KeyActionScopeKind { Global, EditorFocused, AlwaysRouted };

struct KeyActionScope {
    KeyActionScopeKind kind = KeyActionScopeKind::Global;
    QString editor_type_id;  // Only used for EditorFocused and AlwaysRouted

    static KeyActionScope global();
    static KeyActionScope editorFocused(QString const & type_id);
    static KeyActionScope alwaysRouted(QString const & type_id);
};
```

**`KeyActionDescriptor` design:**

```cpp
struct KeyActionDescriptor {
    QString action_id;          // "media.assign_group_1" — unique, dot-namespaced
    QString display_name;       // "Assign to Group 1" — for UI
    QString category;           // "Media Viewer" — grouping in keybinding editor
    KeyActionScope scope;       // When this action is eligible
    QKeySequence default_binding;  // Shipped default (may be empty = unbound)
};
```

**`KeymapManager` public API:**

```cpp
class KeymapManager : public QObject {
    Q_OBJECT
public:
    // --- Registration (called during widget type setup) ---
    void registerAction(KeyActionDescriptor descriptor);
    void unregisterAction(QString const & action_id);

    // --- Query ---
    std::vector<KeyActionDescriptor> allActions() const;
    std::vector<KeyActionDescriptor> actionsForScope(KeyActionScope const & scope) const;
    QKeySequence bindingFor(QString const & action_id) const;  // resolved: override > default

    // --- Override management ---
    void setUserOverride(QString const & action_id, QKeySequence const & binding);
    void clearUserOverride(QString const & action_id);
    void resetAllOverrides();

    // --- Conflict detection ---
    struct Conflict {
        QString action_id_a;
        QString action_id_b;
        QKeySequence key;
        KeyActionScope scope;
    };
    std::vector<Conflict> detectConflicts() const;

    // --- Dispatch (called from event filter) ---
    bool resolveKeyPress(QKeyEvent * event,
                         QString const & focused_editor_type_id) const;

    // --- Persistence ---
    std::vector<KeymapOverrideEntry> exportOverrides() const;
    void importOverrides(std::vector<KeymapOverrideEntry> const & overrides);

signals:
    void bindingsChanged();
};
```

**Modified files:**

| File | Change |
|------|--------|
| `src/CMakeLists.txt` | Add `add_subdirectory(WhiskerToolbox/KeymapSystem)` |

**Tests:** `tests/KeymapSystem/test_keymap_manager.cpp`

- Register actions in different scopes, verify `allActions()` returns them
- `bindingFor()` returns default when no override; returns override when set
- `detectConflicts()` flags same key+scope, allows same key in different scopes
- `resolveKeyPress()` returns correct action ID given scope priority
- `exportOverrides()` → `importOverrides()` round-trip

**Exit criteria:** Library builds, tests pass. No application behavior changes.

---

### Step 1.2 — Create the `KeyActionHandler` mixin

**Goal:** Define the interface that widgets implement to receive keymap dispatches.

This is part of Step 1.1's file set (`KeyActionHandler.hpp`), but called out separately because widget adoption depends on it.

**Design:**

```cpp
class KeyActionHandler {
public:
    virtual ~KeyActionHandler() = default;

    /// Handle a keymap action. Return true if handled.
    virtual bool handleKeyAction(QString const & action_id) = 0;
};
```

Widgets that want to receive dispatches inherit from this (alongside `QWidget` / `QGraphicsScene`). This follows the `DataFocusAware` mixin pattern — lightweight, no Qt dependency beyond `QString`.

**Exit criteria:** Header compiles. No runtime effect yet.

---

### Step 1.3 — Wire `KeymapManager` into MainWindow

**Goal:** Install `KeymapManager` as a QApplication event filter, running alongside the existing `MainWindow::eventFilter()`.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/Main_Window/mainwindow.hpp` | Add `KeymapManager *` member |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Create `KeymapManager` in constructor. Install as app event filter. Wire `SelectionContext::activeEditorId()` for focus tracking. |

**Behavior:** `KeymapManager::eventFilter()` runs *first*. If it finds a matching action, it consumes the event. If not, the event falls through to the existing `MainWindow::eventFilter()` which handles the hardcoded keys as before.

**No registered actions yet**, so this step changes nothing at runtime — it only installs the plumbing.

**Exit criteria:** Application starts and runs identically to before. No regressions.

---

### Step 1.4 — Add persistence to `AppPreferencesData`

**Goal:** Store user keybinding overrides across sessions.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/StateManagement/AppPreferencesData.hpp` | Add `std::vector<KeymapOverrideEntry> keybinding_overrides` field |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | On startup: load overrides from `AppPreferences` into `KeymapManager`. On `bindingsChanged()`: save back. |

**`KeymapOverrideEntry`** (defined in `Keymap.hpp`):

```cpp
struct KeymapOverrideEntry {
    std::string action_id;
    std::string key_sequence;   // QKeySequence::toString() format
    bool enabled = true;
};
```

**Exit criteria:** Override round-trip works: set override → restart app → override is still applied. (Can be verified once actions are registered in Phase 2.)

---

## Phase 2: Migrate Existing Hardcoded Keys

Each step in this phase registers actions for an existing set of hardcoded keys, implements `handleKeyAction()` on the target widget, and removes the corresponding hardcoded code. Steps are independent and can be done in any order.

### Step 2.1 — Timeline actions (always-routed)

**Actions to register (in MainWindow initialization):**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `timeline.play_pause` | Play / Pause | AlwaysRouted("TimeScrollBar") | Space |
| `timeline.next_frame` | Next Frame | AlwaysRouted("TimeScrollBar") | Right |
| `timeline.prev_frame` | Previous Frame | AlwaysRouted("TimeScrollBar") | Left |
| `timeline.jump_forward` | Jump Forward | AlwaysRouted("TimeScrollBar") | Ctrl+Right |
| `timeline.jump_backward` | Jump Backward | AlwaysRouted("TimeScrollBar") | Ctrl+Left |

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` | Inherit `KeyActionHandler`, implement `handleKeyAction()` |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.cpp` | Dispatch `play_pause` → `PlayButton()`, `next_frame` / `prev_frame` → `changeScrollBarValue(±1)`, `jump_forward` / `jump_backward` → `changeScrollBarValue(±getFrameJumpValue())` |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Register above actions. Remove arrow-key and spacebar handling from `eventFilter()`. Register `TimeScrollBar` as the always-routed handler for these actions with `KeymapManager`. |

**Handling the text-input bypass:** The bypass logic (don't consume keys when a text input has focus) moves into `KeymapManager::eventFilter()` as a general mechanism, not per-action code. This is cleaner than the current approach of checking `qobject_cast<QLineEdit*>` in multiple places.

**Exit criteria:** Space/arrow keys work exactly as before. User can reassign them via `KeymapManager::setUserOverride()` API (no UI yet). Existing tests pass.

---

### Step 2.2 — Media_Widget group assignment (editor-focused)

**Actions to register (in `MediaWidgetModule::registerTypes()`):**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `media.assign_group_1` | Assign to Group 1 | EditorFocused("MediaWidget") | 1 |
| `media.assign_group_2` | Assign to Group 2 | EditorFocused("MediaWidget") | 2 |
| … | … | … | … |
| `media.assign_group_9` | Assign to Group 9 | EditorFocused("MediaWidget") | 9 |

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.hpp` | Inherit `KeyActionHandler` |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.cpp` | Implement `handleKeyAction()` for group assignment. Remove the `keyPressEvent()` override that handles keys 1-9. |
| `src/WhiskerToolbox/Media_Widget/MediaWidgetRegistration.cpp` | Register the 9 actions with `KeymapManager`. |

**Exit criteria:** Group assignment works as before. Keys are now configurable.

---

### Step 2.3 — Plot polygon editing (editor-focused)

**Actions to register (per plot widget type):**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `<type>.polygon_complete` | Complete Polygon | EditorFocused | Enter |
| `<type>.polygon_cancel` | Cancel Polygon | EditorFocused | Escape |
| `<type>.polygon_undo_vertex` | Undo Last Vertex | EditorFocused | Backspace |

Where `<type>` is `scatter_plot`, `temporal_projection`, `analysis_dashboard`, etc.

**Modified files:**

| File | Change |
|------|--------|
| ScatterPlotOpenGLWidget | Inherit `KeyActionHandler`, implement `handleKeyAction()`, remove `keyPressEvent()` polygon code |
| TemporalProjectionOpenGLWidget | Same pattern |
| BasePlotOpenGLWidget (Analysis Dashboard) | Same pattern (also migrate "R" for reset view) |
| Corresponding Registration files | Register actions |

**Exit criteria:** Polygon editing works as before. Keys are configurable.

---

### Step 2.4 — Auto-generated focus actions

**Goal:** For every registered editor type, automatically create a `"focus.<type_id>"` action.

**Implementation in `KeymapManager`:**

```cpp
void KeymapManager::generateFocusActions(EditorRegistry const * registry) {
    for (auto const & type_info : registry->allTypes()) {
        registerAction({
            .action_id = "focus." + type_info.type_id,
            .display_name = "Focus " + type_info.display_name,
            .category = "Focus",
            .scope = KeyActionScope::global(),
            .default_binding = QKeySequence()  // unbound by default
        });
    }
}
```

**Execution:** When a focus action fires, `KeymapManager` calls `EditorRegistry::openEditor()` to raise/create the widget, then `SelectionContext::setActiveEditor()` to transfer focus.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.cpp` | Add `generateFocusActions()`. Handle `"focus.*"` actions in dispatch by calling `EditorRegistry::openEditor()`. |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Call `generateFocusActions()` after `_registerEditorTypes()`. |

**Exit criteria:** `allActions()` includes a focus action for every editor type. Assigning a key to one raises that widget.

---

### Step 2.5 — Python Console shortcuts (editor-focused)

**Actions:**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `python.execute` | Execute Input | EditorFocused("PythonConsole") | Shift+Enter |
| `python.clear_output` | Clear Output | EditorFocused("PythonConsole") | Ctrl+L |
| `python.history_older` | History: Older | EditorFocused("PythonConsole") | Up |
| `python.history_newer` | History: Newer | EditorFocused("PythonConsole") | Down |

**Modified files:** `PythonConsoleWidget.hpp/.cpp`, its registration file.

**Exit criteria:** Python console shortcuts are configurable.

---

## Phase 3: Keybinding Editor Widget

### Step 3.1 — Create `KeybindingEditor` widget

**Goal:** A dedicated dock widget for viewing and editing keybindings.

**New files:**

| File | Contents |
|------|----------|
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.hpp` | Widget class |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.cpp` | Implementation |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.hpp` | `registerTypes()` |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.cpp` | Registration implementation |
| `src/WhiskerToolbox/KeybindingEditor/CMakeLists.txt` | Static library, depends on KeymapSystem + Qt6::Widgets |

**UI layout:**

```
┌─────────────────────────────────────────────────────────────┐
│ Search: [________________]                [Reset All]       │
├─────────────────────────────────────────────────────────────┤
│ ▼ Focus                                                     │
│   Focus Media Viewer          (unbound)     [Record] [Reset]│
│   Focus Scatter Plot          (unbound)     [Record] [Reset]│
│   Focus ML Workflow           (unbound)     [Record] [Reset]│
│ ▼ Timeline                                                  │
│   Play / Pause                Space         [Record] [Reset]│
│   Next Frame                  →             [Record] [Reset]│
│   Previous Frame              ←             [Record] [Reset]│
│   Jump Forward                Ctrl+→        [Record] [Reset]│
│   Jump Backward               Ctrl+←        [Record] [Reset]│
│ ▼ Media Viewer                                              │
│   Assign to Group 1           1             [Record] [Reset]│
│   Assign to Group 2           2             [Record] [Reset]│
│   ...                                                       │
│ ▼ Scatter Plot                                              │
│   Complete Polygon            Enter         [Record] [Reset]│
│   Cancel Polygon              Esc           [Record] [Reset]│
│   ⚠ Undo Last Vertex         Backspace     [Record] [Reset]│
└─────────────────────────────────────────────────────────────┘
```

Features:

- **QTreeView** with a custom model, grouped by `category`
- **Scope badge** next to each action (Global, Focused, Always-Routed)
- **"Record" button** enters capture mode — next key press becomes the new binding
- **Conflict warning** (⚠ icon) when two actions in the same scope share a key
- **"Reset" per-action** clears the user override, reverting to default
- **"Reset All"** clears all overrides
- **Search filter** narrows the tree by action name or key

**Exit criteria:** Widget opens from View menu. Displays all registered actions. Record + Reset work. Overrides persist across restart.

---

### Step 3.2 — Register as editor type

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Add `KeybindingEditorModule::registerTypes()` call in `_registerEditorTypes()`. |

**Registration details:**

```cpp
registry->registerType({
    .type_id = "KeybindingEditor",
    .display_name = "Keybinding Editor",
    .menu_path = "View/Tools",
    .preferred_zone = Zone::Center,
    .allow_multiple = false,
    .create_state = ...
    .create_view = ...
});
```

**Exit criteria:** Keybinding Editor opens via View > Tools menu. Only one instance allowed.

---

## Phase 4: Extended Widget Adoption

This phase is ongoing — each widget migrates its keyboard handling at its own pace. No ordering required.

### Candidate widgets for migration:

| Widget | Actions | Priority |
|--------|---------|----------|
| `DataViewer_Widget` | Wheel zoom sensitivity, any future keys | Low |
| `Whisker_Widget` | "T" key (currently commented out) | Low |
| `LinePlotWidget` | Ctrl-release for line selection cancel | Medium |
| `BasePlotOpenGLWidget` | "R" for reset view | Medium |
| Future widgets | Define actions at creation time | N/A |

### Migration checklist for each widget:

1. Identify all keys handled in `keyPressEvent()` or `eventFilter()`
2. Define `KeyActionDescriptor` for each, with appropriate scope
3. Register descriptors in the widget's `registerTypes()`
4. Inherit `KeyActionHandler`, implement `handleKeyAction()`
5. Remove the original `keyPressEvent()` / `eventFilter()` key handling
6. Verify the widget still works
7. Verify keys appear in the Keybinding Editor

---

## Phase 5: Advanced Features (Future)

These are not planned for initial implementation but the architecture supports them.

### Multi-chord sequences

`QKeySequence` natively supports up to 4 key combos (e.g., Ctrl+K then Ctrl+C). The `KeymapManager` would need a state machine to track partial matches. Data model already supports this — no structural changes needed, only dispatch logic.

### Mouse button bindings

Blender also binds mouse buttons (e.g., middle-click for orbit). A parallel `MouseActionDescriptor` using the same `KeyActionScope` model could be added. The event filter already handles `QEvent::MouseButtonPress`.

### Keymap presets

Blender ships multiple keymap presets (Blender, Industry Compatible). WhiskerToolbox could ship presets for different workflows (e.g., "Labeling", "Analysis"). Implementation: a `KeymapPreset` struct containing a complete set of overrides, selectable in the Keybinding Editor.

### Passthrough actions

Some actions might want to fire but *not* consume the event, allowing it to also reach the focused widget. A `bool consuming = true` field on `KeyActionDescriptor` would control this. Not needed for v1.

### Context-sensitive action availability

Currently, an `EditorFocused` action is available whenever that editor type has focus. A richer version could add a predicate (like `ContextAction::is_applicable`) — e.g., "polygon_complete only available when polygon tool is active." This would tie into the existing `ContextAction` infrastructure.

---

## Summary of All New Files

| Path | Phase | Type |
|------|-------|------|
| `src/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | 1.1 | CMake |
| `src/WhiskerToolbox/KeymapSystem/KeyAction.hpp` | 1.1 | Header |
| `src/WhiskerToolbox/KeymapSystem/Keymap.hpp` | 1.1 | Header |
| `src/WhiskerToolbox/KeymapSystem/KeyActionHandler.hpp` | 1.1 | Header |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` | 1.1 | Header |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.cpp` | 1.1 | Source |
| `src/WhiskerToolbox/KeybindingEditor/CMakeLists.txt` | 3.1 | CMake |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.hpp` | 3.1 | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.cpp` | 3.1 | Source |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.hpp` | 3.2 | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.cpp` | 3.2 | Source |
| `tests/KeymapSystem/CMakeLists.txt` | 1.1 | CMake |
| `tests/KeymapSystem/test_keymap_manager.cpp` | 1.1 | Test |

## Summary of All Modified Files

| Path | Phase | Change |
|------|-------|--------|
| `src/CMakeLists.txt` | 1.1 | Add `KeymapSystem` subdirectory |
| `src/WhiskerToolbox/StateManagement/AppPreferencesData.hpp` | 1.4 | Add `keybinding_overrides` field |
| `src/WhiskerToolbox/Main_Window/mainwindow.hpp` | 1.3 | Add `KeymapManager *` member |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | 1.3, 2.1–2.4 | Create `KeymapManager`, register actions, migrate `eventFilter()` |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` | 2.1 | Inherit `KeyActionHandler` |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.cpp` | 2.1 | Implement `handleKeyAction()` |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.hpp` | 2.2 | Inherit `KeyActionHandler` |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.cpp` | 2.2 | Implement `handleKeyAction()`, remove `keyPressEvent()` |
| `src/WhiskerToolbox/Media_Widget/MediaWidgetRegistration.cpp` | 2.2 | Register group-assignment actions |
| Plot widget files (Scatter, Temporal, Analysis) | 2.3 | Same migration pattern |
| `.github/copilot-instructions.md` | 1.1 | Add `KeymapSystem` library description |
