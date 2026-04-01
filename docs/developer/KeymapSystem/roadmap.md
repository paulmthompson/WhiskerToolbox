# Keymap System — Implementation Roadmap

**Last updated:** 2026-03-31 — Steps 1.1–1.4, 2.1, 2.2, 2.3, 3.1, 3.2, and 3.3 complete

## Progress

| Step | Status |
|------|--------|
| 1.1 — `KeymapSystem` static library | ✅ Complete |
| 1.2 — `KeyActionAdapter` component | ✅ Complete (delivered as part of 1.1) |
| 1.3 — Wire into MainWindow | ✅ Complete |
| 1.4 — Persistence in AppPreferencesData | ✅ Complete |
| 2.1 — Timeline actions | ✅ Complete |
| 2.2 — Media_Widget group assignment | ✅ Complete |
| 2.3 — Plot polygon editing | ✅ Complete (ScatterPlotWidget) |
| 2.4 — Auto-generated focus actions | ⬜ Not started |
| 2.5 — Python Console shortcuts | ⬜ Not started |
| 3.1 — KeybindingEditor widget | ✅ Complete (scaffold) |
| 3.2 — Register as editor type | ✅ Complete |
| 3.3 — Full KeybindingEditor UI | ✅ Complete |

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

### ✅ Step 1.3 — Wire `KeymapManager` into MainWindow

**Completed.** `KeymapManager` is created in the MainWindow constructor and installed as a QApplication event filter. It runs before MainWindow's own event filter (Qt LIFO order). The text-input bypass logic is implemented in `KeymapManager::eventFilter()`, and focus tracking is wired through `EditorRegistry`/`SelectionContext`.

**No registered actions yet**, so this step changes nothing at runtime — it only installs the plumbing.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` | Added `eventFilter()` override, `setEditorRegistry()`, `_isTextInputWidget()` |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.cpp` | Implemented event filter with text-input bypass, scope resolution, and adapter dispatch |
| `src/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | Added `Qt6::Widgets` dependency (for text-input widget type checks) |
| `src/WhiskerToolbox/Main_Window/mainwindow.hpp` | Added `KeymapSystem::KeymapManager *` member and `keymapManager()` accessor |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Creates `KeymapManager`, sets editor registry, installs as app event filter |
| `src/WhiskerToolbox/CMakeLists.txt` | Added `KeymapSystem` to WhiskerToolbox link dependencies |

---

### ✅ Step 1.4 — Add persistence to `AppPreferencesData`

**Completed.** User keybinding overrides are stored in `AppPreferencesData` and round-tripped via `KeymapManager::exportOverrides()` / `importOverrides()`.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/StateManagement/AppPreferencesData.hpp` | Added `keybinding_overrides` field (`std::vector<KeymapSystem::KeymapOverrideEntry>`) |
| `src/WhiskerToolbox/StateManagement/AppPreferences.hpp` | Added `keybindingOverrides()` getter and `setKeybindingOverrides()` setter |
| `src/WhiskerToolbox/StateManagement/AppPreferences.cpp` | Implemented getter/setter (setter triggers debounced auto-save) |
| `src/WhiskerToolbox/StateManagement/CMakeLists.txt` | Added `KeymapSystem` as PRIVATE dependency |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | On startup: imports overrides from preferences. On `bindingsChanged()`: exports overrides back to preferences. |

---

## Phase 2: Migrate Existing Hardcoded Keys

Each step in this phase registers action descriptors for an existing set of hardcoded keys, adds a `KeyActionAdapter` to the target widget, and removes the corresponding hardcoded code. Steps are independent and can be done in any order.

### ✅ Step 2.1 — Timeline actions (always-routed)

**Completed.** Five timeline actions are registered as `AlwaysRouted("TimeScrollBar")` and dispatched via a `KeyActionAdapter` in `TimeScrollBar`. The hardcoded key handling in `MainWindow::eventFilter()` has been removed.

**Actions registered:**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `timeline.play_pause` | Play / Pause | AlwaysRouted("TimeScrollBar") | Space |
| `timeline.next_frame` | Next Frame | AlwaysRouted("TimeScrollBar") | Right |
| `timeline.prev_frame` | Previous Frame | AlwaysRouted("TimeScrollBar") | Left |
| `timeline.jump_forward` | Jump Forward | AlwaysRouted("TimeScrollBar") | Ctrl+Right |
| `timeline.jump_backward` | Jump Backward | AlwaysRouted("TimeScrollBar") | Ctrl+Left |

**Behavior change:** Plain arrow keys now step by ±1 frame (previously used `getFrameJumpValue()`). Ctrl+arrows continue to jump by the frame jump value. This makes the distinction between single-frame and multi-frame navigation explicit.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` | Added `KeyActionAdapter *` member, `setKeymapManager()` method, forward declarations for `KeymapSystem` types |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.cpp` | Implemented `setKeymapManager()`: creates adapter with handler dispatching all 5 timeline actions |
| `src/WhiskerToolbox/TimeScrollBar/CMakeLists.txt` | Added `KeymapSystem` as PRIVATE dependency |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Registered 5 timeline action descriptors after `KeymapManager` creation. Called `setKeymapManager()` on `TimeScrollBar`. Removed all hardcoded Space/arrow/Ctrl+arrow handling from `eventFilter()`. |
| `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` | Added 4 test cases for timeline action registration, scope resolution, adapter dispatch, and rebinding |

**Text-input bypass:** Handled by `KeymapManager::eventFilter()` as a general mechanism — the bypass logic in `MainWindow::eventFilter()` is no longer needed for timeline keys.

---

### ✅ Step 2.2 — Media_Widget group assignment (editor-focused)

**Completed.** Nine group-assignment actions are registered as `EditorFocused("MediaWidget")` in `MediaWidgetModule::registerTypes()`. A `KeyActionAdapter` in `Media_Window` handles the dispatch, and the old `keyPressEvent()` override has been removed.

**Actions registered:**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `media.assign_group_1` | Assign to Group 1 | EditorFocused("MediaWidget") | 1 |
| `media.assign_group_2` | Assign to Group 2 | EditorFocused("MediaWidget") | 2 |
| `media.assign_group_3` | Assign to Group 3 | EditorFocused("MediaWidget") | 3 |
| `media.assign_group_4` | Assign to Group 4 | EditorFocused("MediaWidget") | 4 |
| `media.assign_group_5` | Assign to Group 5 | EditorFocused("MediaWidget") | 5 |
| `media.assign_group_6` | Assign to Group 6 | EditorFocused("MediaWidget") | 6 |
| `media.assign_group_7` | Assign to Group 7 | EditorFocused("MediaWidget") | 7 |
| `media.assign_group_8` | Assign to Group 8 | EditorFocused("MediaWidget") | 8 |
| `media.assign_group_9` | Assign to Group 9 | EditorFocused("MediaWidget") | 9 |

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/Media_Widget/MediaWidgetRegistration.hpp` | Added `KeymapSystem::KeymapManager *` parameter to `registerTypes()` (default: `nullptr`) |
| `src/WhiskerToolbox/Media_Widget/MediaWidgetRegistration.cpp` | Added `registerGroupActions()` helper. Registers 9 `media.assign_group_N` descriptors. Updated view and custom-editor factories to call `media_window->setKeymapManager(km)`. |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.hpp` | Added `KeymapSystem` forward declarations; added `setKeymapManager()` public method; added `KeyActionAdapter *` private member; removed `keyPressEvent()` override declaration. |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.cpp` | Added `KeymapSystem/KeyActionAdapter.hpp` and `KeymapSystem/KeymapManager.hpp` includes. Implemented `setKeymapManager()`: creates adapter parented to `this`, sets type `"MediaWidget"`, handler extracts group number from action ID and calls `assignEntitiesToGroup()`. Removed `keyPressEvent()` implementation. |
| `src/WhiskerToolbox/Media_Widget/CMakeLists.txt` | Added `KeymapSystem` as PRIVATE dependency. |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Passes `_keymap_manager` to `MediaWidgetModule::registerTypes()`. Fixed duplicate `KeymapManager` creation (was created twice due to merge artifact). |
| `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` | Added 4 test cases: group action registration/scope/defaults, EditorFocused resolution, adapter dispatch, and rebinding via user override. |

**Test count:** 27 passing (19 original + 4 timeline + 4 media group).

---

### ✅ Step 2.3 — Plot polygon editing (editor-focused)

**Completed for ScatterPlotWidget.** Three polygon editing actions are registered as `EditorFocused("ScatterPlotWidget")` in `ScatterPlotWidgetModule::registerTypes()`. A `KeyActionAdapter` in `ScatterPlotOpenGLWidget` handles the dispatch, and the old `keyPressEvent()` polygon handling has been removed.

**Actions registered:**

| Action ID | Display Name | Scope | Default Key |
|-----------|-------------|-------|-------------|
| `scatter_plot.polygon_complete` | Complete Polygon | EditorFocused("ScatterPlotWidget") | Enter |
| `scatter_plot.polygon_cancel` | Cancel Polygon | EditorFocused("ScatterPlotWidget") | Escape |
| `scatter_plot.polygon_undo_vertex` | Undo Last Vertex | EditorFocused("ScatterPlotWidget") | Backspace |

**Handler behavior:** All three actions check `_polygon_controller->isActive()` — if polygon mode is not active, the adapter returns `false` so the event passes through normally.

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/ScatterPlotWidgetRegistration.hpp` | Added `KeymapSystem::KeymapManager *` parameter to `registerTypes()` (default: `nullptr`) |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/ScatterPlotWidgetRegistration.cpp` | Added `registerPolygonActions()` helper. Registers 3 polygon action descriptors. Updated view and custom-editor factories to call `widget->setKeymapManager(km)`. Updated signature to match. |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Rendering/ScatterPlotOpenGLWidget.hpp` | Added `KeymapSystem` forward declarations; added `setKeymapManager()` public method; added `KeyActionAdapter *` private member; removed `keyPressEvent()` override declaration. |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Rendering/ScatterPlotOpenGLWidget.cpp` | Added `KeymapSystem/KeyActionAdapter.hpp` and `KeymapSystem/KeymapManager.hpp` includes. Implemented `setKeymapManager()`. Removed `keyPressEvent()` implementation. |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/UI/ScatterPlotWidget.hpp` | Added `KeymapSystem` forward declaration; added `setKeymapManager()` pass-through method. |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/UI/ScatterPlotWidget.cpp` | Added `KeymapSystem/KeymapManager.hpp` include. Implemented `setKeymapManager()` pass-through to `_opengl_widget`. |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/CMakeLists.txt` | Added `KeymapSystem` as PRIVATE dependency. |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Passes `_keymap_manager` to `ScatterPlotWidgetModule::registerTypes()`. |

---

### Step 2.3 (remaining) — TemporalProjection / AnalysisDashboard polygon editing

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

### ✅ Step 3.1 — Create `KeybindingEditor` widget

**Completed (scaffold).** The static library is created and compiles cleanly. The widget is a placeholder with a `QLabel`; the full UI (QTreeView model, Record/Reset controls) will be implemented in a later pass. The `registerTypes()` call is wired in CMakeLists.txt but not yet called from mainwindow (that is Step 3.2).

**Delivered files:**

| File | Contents |
|------|----------|
| `src/WhiskerToolbox/KeybindingEditor/CMakeLists.txt` | Static library `KeybindingEditor`. Depends on `KeymapSystem` (PRIVATE), `EditorState` (PUBLIC), `Qt6::Widgets` (PUBLIC). |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorState.hpp/.cpp` | Minimal `EditorState` subclass. `toJson()` returns `{}`. No user state — all persistent data lives in `KeymapManager`. |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.hpp/.cpp` | `QWidget` subclass. Constructor takes `shared_ptr<KeybindingEditorState>` and `KeymapManager*`. Placeholder `QLabel` body. |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.hpp/.cpp` | `KeybindingEditorModule::registerTypes(EditorRegistry*, KeymapManager*)`. Registers type `"KeybindingEditor"`, `Zone::Right`, `allow_multiple = false`, `menu_path = "View/Tools"`. |

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/CMakeLists.txt` | Added `add_subdirectory(KeybindingEditor)` and `target_link_libraries(WhiskerToolbox PRIVATE KeybindingEditor)` |

**UI layout (planned for full implementation):**

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

### ✅ Step 3.2 — Register as editor type

**Completed.** `KeybindingEditorModule::registerTypes()` is called from `MainWindow::_registerEditorTypes()`. The widget appears in the **View/Tools** menu and can be opened/raised like any other editor type. Only one instance is allowed (`allow_multiple = false`).

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | Added `#include "KeybindingEditor/KeybindingEditorRegistration.hpp"`. Added `KeybindingEditorModule::registerTypes(_editor_registry.get(), _keymap_manager)` call at the end of `_registerEditorTypes()`. |

---

### ✅ Step 3.3 — Full KeybindingEditor UI

**Completed.** The placeholder widget has been replaced with a full tree-based keybinding editor. The widget displays all registered actions in a two-level tree (Category → Action) with columns for name, current binding, scope, and default binding. Users can record new bindings, clear bindings, and reset individual or all overrides.

**New files:**

| File | Contents |
|------|----------|
| `src/WhiskerToolbox/KeybindingEditor/KeymapModel.hpp/.cpp` | `QAbstractItemModel` subclass providing a two-level tree: categories (from `KeyActionDescriptor::category`) as parent rows, actions as child rows. Columns: Name, Binding, Scope, Default. Exposes `ActionIdRole`, `IsCategoryRole`, `HasConflictRole`, `IsOverrideRole` custom data roles. Conflict detection via `KeymapManager::detectConflicts()` highlights conflicting bindings in red. Overridden bindings shown in blue italic. |
| `src/WhiskerToolbox/KeybindingEditor/KeySequenceRecorder.hpp/.cpp` | `QPushButton` subclass that enters "recording" mode on click, grabs keyboard, captures the next key combination, and emits `keySequenceRecorded(QKeySequence)`. Escape cancels recording. Focus loss auto-cancels. Bare modifier keys (Shift, Ctrl, Alt, Meta) are ignored during recording. |

**Modified files:**

| File | Change |
|------|--------|
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.hpp` | Replaced placeholder with full UI: `QLineEdit` search bar, `QTreeView` with `QSortFilterProxyModel`, `KeySequenceRecorder` record button, Reset/Clear/Reset All buttons. |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.cpp` | Full implementation: builds UI layout, connects `bindingsChanged()` signal to refresh model, search filtering with `QSortFilterProxyModel::setRecursiveFilteringEnabled(true)`, selection-aware button states. |
| `src/WhiskerToolbox/KeybindingEditor/CMakeLists.txt` | Added `KeymapModel.hpp/.cpp` and `KeySequenceRecorder.hpp/.cpp` to source list. |

**UI layout:**

```
┌──────────────────────────────────────────────────────────────┐
│ Search: [________________]                                   │
├──────────────────────────────────────────────────────────────┤
│ Action         │ Binding    │ Scope              │ Default   │
│ ▼ Application                                               │
│   Open Keybinding Editor  (unbound)  Global        (none)   │
│ ▼ Media Viewer                                              │
│   Assign to Group 1       1          Focused(…)    1        │
│   Assign to Group 2       2          Focused(…)    2        │
│   ...                                                       │
│ ▼ Timeline                                                  │
│   Play / Pause            Space      Always(…)     Space    │
│   Next Frame              →          Always(…)     →        │
│   ...                                                       │
├──────────────────────────────────────────────────────────────┤
│ [Record]  [Clear]  [Reset]                    [Reset All]   │
└──────────────────────────────────────────────────────────────┘
```

**Features implemented:**

- **QTreeView** with custom `KeymapModel`, grouped by category
- **Scope column** showing `Global`, `Focused (TypeId)`, or `Always (TypeId)`
- **Record button** (`KeySequenceRecorder`) — captures next key press as new binding
- **Clear button** — unbinds the selected action (sets to empty `QKeySequence`)
- **Reset button** — reverts the selected action to its default binding
- **Reset All button** — clears all overrides (with confirmation dialog)
- **Conflict highlighting** — conflicting bindings shown in red text
- **Override highlighting** — user-customized bindings shown in blue italic
- **Search filter** — recursive `QSortFilterProxyModel` narrows tree by action name
- **Auto-refresh** — model rebuilds on `KeymapManager::bindingsChanged()` signal
- **Button state management** — Record/Clear/Reset disabled when no action is selected

**Exit criteria:** ✅ Widget opens from Modules menu. Displays all registered actions grouped by category. Record, Clear, Reset, and Reset All all work. Overrides persist across restart (via existing AppPreferencesData pipeline from Step 1.4).

---

## Phase 4: Extended Widget Adoption

This phase is ongoing — each widget migrates its keyboard handling at its own pace. No ordering required.

### Candidate widgets for migration:

| Widget | Actions | Priority |
|--------|---------|----------|
| `DataViewer_Widget` | Wheel zoom sensitivity, any future keys | Low |
| `Whisker_Widget` | "T" key (currently commented out) | Low |
| `LinePlotWidget` | Ctrl-release for line selection cancel | Medium |
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
| `src/WhiskerToolbox/KeybindingEditor/CMakeLists.txt` | 3.1 | ✅ Done | CMake |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorState.hpp/.cpp` | 3.1 | ✅ Done | Header/Source |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.hpp` | 3.1 | ✅ Done | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditor.cpp` | 3.1 | ✅ Done | Source |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.hpp` | 3.2 | ✅ Done | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeybindingEditorRegistration.cpp` | 3.2 | ✅ Done | Source |
| `src/WhiskerToolbox/KeybindingEditor/KeymapModel.hpp` | 3.3 | ✅ Done | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeymapModel.cpp` | 3.3 | ✅ Done | Source |
| `src/WhiskerToolbox/KeybindingEditor/KeySequenceRecorder.hpp` | 3.3 | ✅ Done | Header |
| `src/WhiskerToolbox/KeybindingEditor/KeySequenceRecorder.cpp` | 3.3 | ✅ Done | Source |

## Summary of All Modified Files

| Path | Phase | Status | Change |
|------|-------|--------|--------|
| `src/WhiskerToolbox/CMakeLists.txt` | 1.1, 1.3 | ✅ Done | Added `KeymapSystem` subdirectory (1.1); linked `KeymapSystem` to WhiskerToolbox target (1.3) |
| `tests/WhiskerToolbox/CMakeLists.txt` | 1.1 | ✅ Done | Added `KeymapSystem` subdirectory |
| `.github/copilot-instructions.md` | 1.1 | ✅ Done | Added `KeymapSystem` library description |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.hpp` | 1.3 | ✅ Done | Added `eventFilter()`, `setEditorRegistry()`, `_isTextInputWidget()` |
| `src/WhiskerToolbox/KeymapSystem/KeymapManager.cpp` | 1.3 | ✅ Done | Implemented event filter with text-input bypass and dispatch |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.hpp` | 2.1 | ✅ Done | Added `KeyActionAdapter *` member, `setKeymapManager()`, forward declarations |
| `src/WhiskerToolbox/TimeScrollBar/TimeScrollBar.cpp` | 2.1 | ✅ Done | Implemented `setKeymapManager()` with adapter handler for 5 timeline actions |
| `src/WhiskerToolbox/TimeScrollBar/CMakeLists.txt` | 2.1 | ✅ Done | Added `KeymapSystem` as PRIVATE dependency |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | 1.3, 1.4, 2.1 | ✅ Done | Created KeymapManager (1.3); imported/exported overrides (1.4); registered timeline actions, called `setKeymapManager()`, removed hardcoded eventFilter key handling (2.1) |
| `tests/WhiskerToolbox/KeymapSystem/test_keymap_manager.cpp` | 2.1 | ✅ Done | Added 4 timeline action test cases |
| `src/WhiskerToolbox/KeymapSystem/CMakeLists.txt` | 1.3 | ✅ Done | Added `Qt6::Widgets` dependency |
| `src/WhiskerToolbox/Main_Window/mainwindow.hpp` | 1.3 | ✅ Done | Added `KeymapManager *` member and `keymapManager()` accessor |
| `src/WhiskerToolbox/Main_Window/mainwindow.cpp` | 1.3, 1.4, 2.1, 2.2, 2.3 | ✅ Done | Created KeymapManager (1.3); imported/exported overrides (1.4); registered timeline actions, called `setKeymapManager()`, removed hardcoded eventFilter key handling (2.1); passes `km` to MediaWidget (2.2) and ScatterPlotWidget (2.3) registration calls |
| `src/WhiskerToolbox/StateManagement/AppPreferencesData.hpp` | 1.4 | ✅ Done | Added `keybinding_overrides` field |
| `src/WhiskerToolbox/StateManagement/AppPreferences.hpp` | 1.4 | ✅ Done | Added `keybindingOverrides()` / `setKeybindingOverrides()` |
| `src/WhiskerToolbox/StateManagement/AppPreferences.cpp` | 1.4 | ✅ Done | Implemented getter/setter |
| `src/WhiskerToolbox/StateManagement/CMakeLists.txt` | 1.4 | ✅ Done | Added `KeymapSystem` dependency |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.hpp` | 2.2 | ✅ Done | Added `KeyActionAdapter *` member and `setKeymapManager()` |
| `src/WhiskerToolbox/Media_Widget/Rendering/Media_Window/Media_Window.cpp` | 2.2 | ✅ Done | Implemented adapter with handler; removed `keyPressEvent()` frame-advance code |
| `src/WhiskerToolbox/Media_Widget/MediaWidgetRegistration.cpp` | 2.2 | ✅ Done | Registered group-assignment actions; passes `km` to `Media_Window::setKeymapManager()` |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/ScatterPlotWidgetRegistration.hpp` | 2.3 | ✅ Done | Added `KeymapSystem::KeymapManager *` parameter to `registerTypes()` |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/ScatterPlotWidgetRegistration.cpp` | 2.3 | ✅ Done | Added `registerPolygonActions()` helper; passes `km` to widget factories |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Rendering/ScatterPlotOpenGLWidget.hpp` | 2.3 | ✅ Done | Added `setKeymapManager()`, `KeyActionAdapter *` member; removed `keyPressEvent()` override |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/Rendering/ScatterPlotOpenGLWidget.cpp` | 2.3 | ✅ Done | Implemented `setKeymapManager()` with polygon action handler; removed `keyPressEvent()` |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/UI/ScatterPlotWidget.hpp` | 2.3 | ✅ Done | Added `setKeymapManager()` pass-through declaration |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/UI/ScatterPlotWidget.cpp` | 2.3 | ✅ Done | Implemented `setKeymapManager()` pass-through to OpenGL widget |
| `src/WhiskerToolbox/Plots/ScatterPlotWidget/CMakeLists.txt` | 2.3 | ✅ Done | Added `KeymapSystem` as PRIVATE dependency |
| Remaining plot widgets (Temporal, Analysis) | 2.3+ | ⬜ Todo | Same migration pattern when polygon editing is added |
