# Keymap System — Implementation Roadmap

**Last updated:** 2026-03-30 — Steps 1.1 and 1.2 complete

## Progress

| Step | Status |
|------|--------|
| 1.1 — `KeymapSystem` static library | ✅ Complete |
| 1.2 — `KeyActionAdapter` component | ✅ Complete (delivered as part of 1.1) |
| 1.3 — Wire into MainWindow | ⬜ Not started |
| 1.4 — Persistence in AppPreferencesData | ⬜ Not started |
| 2.1 — Timeline actions | ⬜ Not started |
| 2.2 — Media_Widget group assignment | ⬜ Not started |
| 2.3 — Plot polygon editing | ⬜ Not started |
| 2.4 — Auto-generated focus actions | ⬜ Not started |
| 2.5 — Python Console shortcuts | ⬜ Not started |
| 3.1 — KeybindingEditor widget | ⬜ Not started |
| 3.2 — Register as editor type | ⬜ Not started |

## Overview

This roadmap breaks the Keymap System into incremental steps that each produce a working, testable result. Each step can be merged independently — later steps build on earlier ones but the system is functional after any completed step.

**Guiding principles:**

- No existing behavior changes until explicitly migrated (hardcoded keys keep working alongside the new system)
- Each step is buildable and testable in isolation
- Widget adoption is incremental — unmigrated widgets continue using `keyPressEvent()` as before

---

## Phase 1: Core Infrastructure

### ✅ Step 1.1 — Create the `KeymapSystem` static library

**Completed.** See [library_reference.qmd](library_reference.qmd) for full API documentation.

**Delivered files:**

| File | Contents |
|------|----------|
| `src/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | Static library. Depends on `Qt6::Core`, `Qt6::Gui`, `EditorState`. |
| `src/WhiskerToolbox/KeymapSystem/KeyAction.hpp` | `KeyActionScopeKind`, `KeyActionScope`, `KeyActionDescriptor` |
| `src/WhiskerToolbox/KeymapSystem/Keymap.hpp` | `KeymapEntry`, `KeymapOverrideEntry` |
| `src/WhiskerToolbox/KeymapSystem/KeyActionAdapter.hpp/.cpp` | Composition-based dispatch component |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp/.cpp` | Registration, scope resolution, override management, conflict detection |
| `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` | 23 passing tests |

**Modified files:** `src/WhiskerToolbox/CMakeLists.txt` (added subdirectory), `tests/WhiskerToolbox/CMakeLists.txt` (added subdirectory), `.github/copilot-instructions.md` (added library description).

---

### ✅ Step 1.2 — Create the `KeyActionAdapter` component

**Completed as part of Step 1.1.** `KeyActionAdapter` is implemented in `KeyActionAdapter.hpp/.cpp`. Widgets create it as a child `QObject` (composition, not inheritance), install a handler lambda, and register it with `KeymapManager`. Automatic deregistration happens via `QObject::destroyed`.

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

`KeymapOverrideEntry` (defined in `Keymap.hpp`) uses `std::string` fields for reflect-cpp compatibility — see `src/WhiskerToolbox/KeymapSystem/Keymap.hpp`.

**Exit criteria:** Override round-trip works: set override → restart app → override is still applied. (Can be verified once actions are registered in Phase 2.)

---

## Phase 2: Migrate Existing Hardcoded Keys

Each step in this phase registers action descriptors for an existing set of hardcoded keys, adds a `KeyActionAdapter` to the target widget, and removes the corresponding hardcoded code. Steps are independent and can be done in any order.

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
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` | Add `KeyActionAdapter *` member |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.cpp` | Create `KeyActionAdapter` in constructor. Handler dispatches `play_pause` → `PlayButton()`, `next_frame` / `prev_frame` → `changeScrollBarValue(±1)`, `jump_forward` / `jump_backward` → `changeScrollBarValue(±getFrameJumpValue())`. Register adapter with `KeymapManager`. |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Register above action descriptors. Remove arrow-key and spacebar handling from `eventFilter()`. |

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
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.hpp` | Add `KeyActionAdapter *` member |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.cpp` | Create `KeyActionAdapter` in constructor with handler for group assignment. Register adapter with `KeymapManager`. Remove the `keyPressEvent()` override that handles keys 1-9. |
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
| ScatterPlotOpenGLWidget | Add `KeyActionAdapter *` member, create adapter with polygon action handler, register with `KeymapManager`, remove `keyPressEvent()` polygon code |
| TemporalProjectionOpenGLWidget | Same pattern |
| BasePlotOpenGLWidget (Analysis Dashboard) | Same pattern (also migrate "R" for reset view) |
| Corresponding Registration files | Register actions |

**Exit criteria:** Polygon editing works as before. Keys are configurable.

---

### Step 2.4 — Auto-generated focus actions

**Goal:** For every registered editor type, automatically create a `"focus.<type_id>"` action (default: unbound). When fired, `KeymapManager` calls `EditorRegistry::openEditor()` and `SelectionContext::setActiveEditor()`.

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

Register with `type_id = "KeybindingEditor"`, `allow_multiple = false`, `preferred_zone = Zone::Center`, accessible from the View/Tools menu.

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
4. Add a `KeyActionAdapter *` member, create it in the constructor with a handler lambda
5. Register the adapter with `KeymapManager` (deregistration is automatic via QObject parenting)
6. Remove the original `keyPressEvent()` / `eventFilter()` key handling
7. Verify the widget still works
8. Verify keys appear in the Keybinding Editor

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

| Path | Phase | Status | Type |
|------|-------|--------|------|
| `src/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | 1.1 | ✅ Done | CMake |
| `src/WhiskerToolbox/KeymapSystem/KeyAction.hpp` | 1.1 | ✅ Done | Header |
| `src/WhiskerToolbox/KeymapSystem/Keymap.hpp` | 1.1 | ✅ Done | Header |
| `src/WhiskerToolbox/KeymapSystem/KeyActionAdapter.hpp` | 1.1 | ✅ Done | Header |
| `src/WhiskerToolbox/KeymapSystem/KeyActionAdapter.cpp` | 1.1 | ✅ Done | Source |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` | 1.1 | ✅ Done | Header |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.cpp` | 1.1 | ✅ Done | Source |
| `tests/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | 1.1 | ✅ Done | CMake |
| `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` | 1.1 | ✅ Done | Test |
| `docs/developer/KeymapSystem/library_reference.qmd` | 1.1 | ✅ Done | Docs |
| `src/WhiskerToolbox/KeybindingEditor/CMakeLists.txt` | 3.1 | ⬜ Todo | CMake |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.hpp` | 3.1 | ⬜ Todo | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.cpp` | 3.1 | ⬜ Todo | Source |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.hpp` | 3.2 | ⬜ Todo | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.cpp` | 3.2 | ⬜ Todo | Source |

## Summary of All Modified Files

| Path | Phase | Status | Change |
|------|-------|--------|--------|
| `src/WhiskerToolbox/CMakeLists.txt` | 1.1 | ✅ Done | Added `KeymapSystem` subdirectory |
| `tests/WhiskerToolbox/CMakeLists.txt` | 1.1 | ✅ Done | Added `KeymapSystem` subdirectory |
| `.github/copilot-instructions.md` | 1.1 | ✅ Done | Added `KeymapSystem` library description |
| `src/WhiskerToolbox/StateManagement/AppPreferencesData.hpp` | 1.4 | ⬜ Todo | Add `keybinding_overrides` field |
| `src/WhiskerToolbox/Main_Window/mainwindow.hpp` | 1.3 | ⬜ Todo | Add `KeymapManager *` member |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | 1.3, 2.1–2.4 | ⬜ Todo | Create `KeymapManager`, register actions, migrate `eventFilter()` |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` | 2.1 | ⬜ Todo | Add `KeyActionAdapter *` member |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.cpp` | 2.1 | ⬜ Todo | Create adapter, set handler, register with `KeymapManager` |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.hpp` | 2.2 | ⬜ Todo | Add `KeyActionAdapter *` member |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.cpp` | 2.2 | ⬜ Todo | Create adapter, set handler, register, remove `keyPressEvent()` |
| `src/WhiskerToolbox/Media_Widget/MediaWidgetRegistration.cpp` | 2.2 | ⬜ Todo | Register group-assignment actions |
| Plot widget files (Scatter, Temporal, Analysis) | 2.3 | ⬜ Todo | Same migration pattern |
