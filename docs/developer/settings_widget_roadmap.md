# Settings Widget — Design & Roadmap

## Progress Summary

*Last updated: 2026-03-21*

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 1:** SettingsWidget Shell & AppPreferences Page | Not Started | Register SettingsWidget in EditorRegistry. Migrate output directory from DataManager_Widget. Surface all existing AppPreferences fields. |
| **Phase 2:** Appearance Settings (Theme & Typography) | Not Started | Dark/light/system theme. Font size. QSS stylesheet loading. Extend AppPreferencesData. |
| **Phase 3:** Widget-Contributed Settings Pages | Not Started | `EditorTypeInfo` extension for settings page factories. Per-widget settings rendered in Settings. |
| **Phase 4:** Keybinding Editor | Not Started | KeybindingMap in AppPreferences. Keybinding resolver in MainWindow. Global + widget-scoped shortcuts. Settings UI for editing. |
| **Phase 5:** Keybinding–Command Integration | Not Started | Map keybindings to Command Architecture commands. Context-aware dispatch. Macro triggers. |

---

## 1. Problem Statement

WhiskerToolbox has a growing number of application-level and widget-level preferences distributed across multiple widgets and systems:

| Preference | Current Location | Issue |
|---|---|---|
| Output directory | `DataManager_Widget` (OutputDirectoryWidget) | Belongs in global settings, not the data manager |
| Python environment path | `PythonPropertiesWidget` reading `AppPreferences` | Discoverable only if user opens the Python widget |
| Default import/export directories | `AppPreferences` (no dedicated UI) | No centralized place to edit them |
| Default TimeFrame key | `AppPreferences` (no dedicated UI) | No centralized place to edit it |
| UI theme (dark/light) | **Not implemented** | Frequently requested |
| Font size / scaling | **Not implemented** | Accessibility concern |
| Keyboard shortcuts | **Hardcoded** per widget | No customization; scattered `keyPressEvent` overrides |

Users have no single place to discover and configure these options. Settings that are logically application-global are hidden inside data-specific widgets. Additionally, the application lacks any customizable keyboard shortcut system — shortcuts are hardcoded in `Media_Window::keyPressEvent()`, `Whisker_Widget::keyPressEvent()`, and various `eventFilter` implementations with no way for users to change them.

### Goals

1. **Centralized settings access:** One widget where users can discover and modify all application preferences.
2. **Widget-contributed settings pages:** Widgets can optionally contribute a settings panel rendered inside the Settings widget, following the same pattern as `create_properties` in the editor registry.
3. **Appearance customization:** Dark mode, light mode, system-follow, and font size.
4. **Customizable keybindings:** Users can remap keyboard shortcuts. The keybinding system integrates with the Command Architecture so command-based actions can be triggered by hotkeys.
5. **Persistence:** All settings persist in the existing `AppPreferences` JSON system with debounced auto-save.

---

## 2. Architecture Overview

### 2.1 High-Level Design

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SettingsWidget (View)                        │
│  ┌─────────────┐  ┌──────────────────────────────────────────────┐  │
│  │ Category     │  │  Active Settings Page                       │  │
│  │ List         │  │                                             │  │
│  │              │  │  (QStackedWidget — one page per category)   │  │
│  │ ▸ General    │  │                                             │  │
│  │   Appearance │  │  ┌─────────────────────────────────────┐    │  │
│  │   Paths      │  │  │  GeneralSettingsPage                │    │  │
│  │   Python     │  │  │  ─────────────────────              │    │  │
│  │   Keybindings│  │  │  Default TimeFrame Key: [________]  │    │  │
│  │ ────────────│  │  │  Output Directory: [/path/...] [📁] │    │  │
│  │ ▸ Widgets   │  │  └─────────────────────────────────────┘    │  │
│  │   Media     │  │                                             │  │
│  │   Line Plot │  │  ┌─────────────────────────────────────┐    │  │
│  │              │  │  │  (widget-contributed pages here)    │    │  │
│  └─────────────┘  │  └─────────────────────────────────────┘    │  │
│                    └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

**Layout:** A left-hand category list (QListWidget or QTreeWidget) with a right-hand QStackedWidget displaying the active settings page. This mirrors the standard settings UI pattern used by Qt Creator, VS Code, and most desktop applications.

### 2.2 State & Persistence Model

The Settings widget does **not** own its own `EditorState` in the traditional sense — it is a view over `AppPreferences` and widget-contributed settings providers. However, it is registered in the `EditorRegistry` for consistent lifecycle management and can be opened via the menu system.

```
                           ┌──────────────────┐
                           │  AppPreferences   │ ← Existing (preferences.json)
                           │  (Global prefs)   │
                           └────────┬─────────┘
                                    │ reads/writes
┌──────────────────┐       ┌────────▼─────────┐
│  EditorRegistry  │◄──────│ SettingsWidget    │
│  (type registry) │       │ (View)            │
└──────┬───────────┘       └────────┬─────────┘
       │                            │ queries for settings pages
       │ EditorTypeInfo             │
       │ .create_settings           ▼
       │ (new factory fn)    ┌─────────────────┐
       └────────────────────►│ Widget-Provided  │
                             │ Settings Pages   │
                             └─────────────────┘
```

### 2.3 Where Settings Live

| Setting Kind | Storage | Provider | Why |
|---|---|---|---|
| Global app settings (paths, theme, font) | `AppPreferencesData` | Built-in SettingsWidget pages | Application-scoped, machine-specific |
| Keybinding map | `AppPreferencesData` | Built-in KeybindingEditorPage | Application-scoped, user-customizable |
| Widget-specific defaults | `AppPreferencesData` (subsections) or separate JSON section | Widget `create_settings` factory | Defaults for new instances of that widget type |

**Important design decision:** Widget-contributed settings are *defaults for new instances*, not per-instance configuration. Per-instance settings remain in each widget's `EditorState` and are edited via that widget's properties panel. The Settings widget surfaces only the "what should a new instance of this widget look like by default?" question.

---

## 3. Detailed Design

### 3.1 SettingsWidget Registration (Phase 1)

Register the Settings widget in the `EditorRegistry` like any other editor:

```cpp
struct SettingsWidgetState : public EditorState {
    Q_OBJECT
public:
    QString getTypeName() const override { return "SettingsWidget"; }

    // Minimal state — tracks which settings page is currently selected
    std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    int selectedPageIndex() const;
    void setSelectedPageIndex(int index);
};
```

```cpp
// SettingsWidgetRegistration.cpp
void SettingsWidgetModule::registerTypes(EditorRegistry * registry,
                                         AppPreferences * preferences)
{
    registry->registerType({
        .type_id = "SettingsWidget",
        .display_name = "Settings",
        .menu_path = "Edit/Settings",
        .preferred_zone = Zone::Center,
        .properties_zone = Zone::Center,  // No separate properties panel
        .allow_multiple = false,          // Singleton — only one settings window
        .create_state = []() {
            return std::make_shared<SettingsWidgetState>();
        },
        .create_view = [preferences](auto state) {
            return new SettingsWidget(state, preferences);
        },
        .create_properties = nullptr,  // No properties panel
    });
}
```

**Menu access:** `Edit → Settings` (standard location). Also consider `Ctrl+,` as a default keybinding (matching VS Code convention).

### 3.2 Built-In Settings Pages (Phase 1–2)

Each built-in page is a QWidget reading/writing `AppPreferences`:

| Page | Fields | Phase |
|---|---|---|
| **General** | Default TimeFrame key, output directory | 1 |
| **Paths** | Default import directory, default export directory | 1 |
| **Python** | Python environment search paths, preferred Python env | 1 |
| **Appearance** | Theme (light/dark/system), font size, font family | 2 |
| **Keybindings** | Keybinding table editor | 4 |

**General page** absorbs the `OutputDirectoryWidget` currently in `DataManager_Widget`. The `OutputDirectoryWidget` itself can be reused as a composable component — its `dirChanged` signal simply wires to `AppPreferences::setDefaultExportDirectory()` instead of `DataManager::setOutputPath()`.

### 3.3 Widget-Contributed Settings Pages (Phase 3)

#### Option A: Extend `EditorTypeInfo` with a Settings Factory (Recommended)

Add an optional `create_settings` factory function to `EditorTypeInfo`:

```cpp
struct EditorTypeInfo {
    // ... existing fields ...

    /// Optional: Factory to create a settings page for this editor type.
    /// The page configures default preferences for new instances.
    /// Returns nullptr if this editor type has no configurable defaults.
    std::function<QWidget*(AppPreferences*)> create_settings;
};
```

When the SettingsWidget initializes, it queries all registered editor types:

```cpp
void SettingsWidget::_buildWidgetSettingsPages() {
    for (auto const & [type_id, info] : _registry->registeredTypes()) {
        if (info.create_settings) {
            QWidget * page = info.create_settings(_preferences);
            _addPage("Widgets/" + info.display_name, page);
        }
    }
}
```

**Advantages:**
- Follows the existing `create_view` / `create_properties` factory pattern exactly.
- Widgets opt-in with zero coupling to the Settings widget itself.
- New widgets get a settings page simply by providing the factory.

#### Option B: EditorState Provides Settings (Rejected)

An alternative considered was having `EditorState` subclasses provide a `createSettingsWidget()` method. This was rejected because:
- Settings pages configure *type-level defaults*, not instance-level state.
- `EditorState` instances are per-editor-instance; type-level configuration belongs in `EditorTypeInfo`.
- It would require instantiating a temporary `EditorState` just to create a settings page.

#### Widget Settings Data Flow

For widgets that contribute a settings page:

```
┌────────────────────┐     reads/writes      ┌──────────────────────┐
│ MediaWidgetSettings │ ──────────────────── │ AppPreferencesData   │
│ Page (in Settings) │                       │ .widget_defaults     │
└────────────────────┘                       │   .media_widget {    │
                                             │     default_zoom: 1  │
                                             │     show_overlays: t │
                                             │   }                  │
┌────────────────────┐     reads on creation │                      │
│ New MediaWidget    │ ◄──────────────────── │                      │
│ Instance           │                       └──────────────────────┘
└────────────────────┘
```

Widget-specific defaults are stored as nested structs in `AppPreferencesData`. Each widget type defines its own defaults struct:

```cpp
struct MediaWidgetDefaults {
    double default_zoom = 1.0;
    bool show_overlays = true;
    // ...
};

struct AppPreferencesData {
    // ... existing fields ...

    // === Widget Defaults ===
    std::optional<MediaWidgetDefaults> media_widget_defaults;
    // Additional widget defaults added as needed
};
```

Since `AppPreferencesData` uses reflect-cpp, new optional fields deserialize gracefully from old config files (missing → `std::nullopt` → use hardcoded defaults).

### 3.4 Appearance System (Phase 2)

#### Theme Architecture

```cpp
// New fields in AppPreferencesData
struct AppearancePreferences {
    std::string theme = "system";  // "light", "dark", "system"
    int font_size = 10;            // Base font point size
    std::string font_family;       // Empty = system default
};

struct AppPreferencesData {
    // ... existing fields ...
    AppearancePreferences appearance;
};
```

**Theme application flow:**

```
AppPreferences::preferenceChanged("appearance.theme")
        │
        ▼
ThemeManager::applyTheme(theme_name)
        │
        ├── Load QSS stylesheet from resources (:/themes/dark.qss)
        ├── QApplication::setStyleSheet(...)
        ├── QPalette adjustments for system theme
        └── Emit themeChanged() for widgets needing manual updates
```

The `ThemeManager` is a small non-widget class owned by `MainWindow`:

```cpp
class ThemeManager : public QObject {
    Q_OBJECT
public:
    void applyTheme(std::string const & theme_name);
    void setFontSize(int point_size);

signals:
    void themeChanged(QString theme_name);
};
```

Stylesheets are embedded as Qt resources (`:/themes/light.qss`, `:/themes/dark.qss`). The "system" option reads `QPalette` from the platform and applies minimal overrides.

### 3.5 Keybinding System (Phase 4–5)

The keybinding system has three components: **storage**, **resolution**, and **editing UI**.

#### 3.5.1 Keybinding Storage

```cpp
struct KeybindingEntry {
    std::string action_id;      // Unique identifier for the action
    std::string key_sequence;   // Qt key sequence string, e.g. "Ctrl+Shift+T"
    std::string scope;          // "global", or editor type_id for widget-scoped
};

struct AppPreferencesData {
    // ... existing fields ...
    std::vector<KeybindingEntry> keybindings;
};
```

**Default keybindings** are defined in code (not in the JSON). User customizations *override* defaults. The JSON only stores user-modified bindings. At resolution time, defaults are merged with user overrides.

#### 3.5.2 Keybinding Registry & Resolution

```cpp
/// @brief Central registry for keyboard shortcuts
/// Owned by MainWindow. Queries AppPreferences for user overrides.
class KeybindingManager : public QObject {
    Q_OBJECT
public:
    /// Register a default keybinding for an action
    void registerDefault(std::string const & action_id,
                         std::string const & display_name,
                         std::string const & default_key_sequence,
                         std::string const & scope = "global");

    /// Resolve: given a key event and active editor context,
    /// return the action_id to invoke (if any)
    std::optional<std::string> resolve(QKeyEvent const * event,
                                        std::string const & active_editor_type) const;

    /// Get effective key sequence for an action (user override or default)
    std::string effectiveKeySequence(std::string const & action_id) const;

    /// Get all registered actions (for the settings UI)
    std::vector<KeybindingInfo> allBindings() const;

    /// Set a user override
    void setUserBinding(std::string const & action_id,
                        std::string const & key_sequence);

    /// Reset an action to its default
    void resetToDefault(std::string const & action_id);
};
```

**Resolution priority:**

```
Key Press Event
    │
    ▼
1. Check widget-scoped bindings for the active editor type
    │  (e.g., "T" → "trace_whisker" only when WhiskerWidget is focused)
    │
    ▼  (no match)
2. Check global bindings
    │  (e.g., "Ctrl+," → "open_settings")
    │
    ▼  (no match)
3. Fall through to Qt's default event handling
```

**Scope model:**
- **Global:** Active regardless of which widget has focus. Installed as a QShortcut on MainWindow or via a top-level event filter.
- **Widget-scoped:** Active only when a specific editor type has focus. The `KeybindingManager` checks `SelectionContext::activeEditorId()` to determine the active editor type.

#### 3.5.3 Integration with Command Architecture

Actions fall into two categories:

| Category | Example | Dispatch Mechanism |
|---|---|---|
| **Command actions** | "Save current data", "Run pipeline" | `CommandFactory::createCommand()` → `execute(ctx)` |
| **UI actions** | "Open Settings", "Toggle sidebar", "Zoom in" | Direct slot invocation via `QAction::trigger()` or signal |

```cpp
/// @brief Dispatches resolved keybinding actions
class ActionDispatcher : public QObject {
    Q_OBJECT
public:
    /// Register a UI action (non-command)
    void registerUIAction(std::string const & action_id,
                          std::function<void()> callback);

    /// Register a command action (dispatches through Command Architecture)
    void registerCommandAction(std::string const & action_id,
                               std::string const & command_name,
                               std::string const & default_params_json);

    /// Dispatch an action by ID
    void dispatch(std::string const & action_id);
};
```

**Wire-up in MainWindow:**

```cpp
// Global event filter in MainWindow
bool MainWindow::eventFilter(QObject * obj, QEvent * event) {
    if (event->type() == QEvent::KeyPress) {
        auto * key_event = static_cast<QKeyEvent *>(event);
        auto active_type = _registry->activeEditorType();
        auto action = _keybinding_manager->resolve(key_event, active_type);
        if (action) {
            _action_dispatcher->dispatch(*action);
            return true;  // consumed
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
```

#### 3.5.4 Migrating Existing Hardcoded Shortcuts

Existing `keyPressEvent` overrides become registered defaults:

```cpp
// Before (in Media_Window):
void Media_Window::keyPressEvent(QKeyEvent * event) {
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        // Assign to group
    }
}

// After (in MediaWidgetModule::registerTypes):
keybinding_manager->registerDefault(
    "media.assign_group_1", "Assign to Group 1", "1", "MediaWidget");
keybinding_manager->registerDefault(
    "media.assign_group_2", "Assign to Group 2", "2", "MediaWidget");
// ...

// Media_Window no longer overrides keyPressEvent.
// The KeybindingManager resolves "1" → "media.assign_group_1"
// and ActionDispatcher calls the assign-to-group logic.
```

#### 3.5.5 Keybinding Editor Page (Settings UI)

The keybinding page in the Settings widget displays a filterable table:

```
┌────────────────────────────────────────────────────────────┐
│  Keybindings                                   [Search: ] │
├──────────────────────────┬──────────┬─────────────────────┤
│  Action                  │ Shortcut │ Scope               │
├──────────────────────────┼──────────┼─────────────────────┤
│  Open Settings           │ Ctrl+,   │ Global              │
│  Undo                    │ Ctrl+Z   │ Global              │
│  Redo                    │ Ctrl+Y   │ Global              │
│  Trace Whisker           │ T        │ Whisker Widget      │
│  Assign to Group 1       │ 1        │ Media Widget        │
│  Assign to Group 2       │ 2        │ Media Widget        │
│  ...                     │          │                     │
├──────────────────────────┴──────────┴─────────────────────┤
│  Double-click a shortcut cell to record a new key combo.  │
│  [Reset All to Defaults]                                  │
└────────────────────────────────────────────────────────────┘
```

Editing flow:
1. User double-clicks a shortcut cell.
2. Cell enters "recording" mode — next key combo is captured.
3. If the combo conflicts with another binding, a warning is shown.
4. User confirms or cancels.
5. Override is saved to `AppPreferences`.

---

## 4. Migration Plan: Output Directory

The output directory is currently set via `OutputDirectoryWidget` embedded in `DataManager_Widget`, which then calls `DataManager::setOutputPath()`. This should migrate to the Settings widget.

### Current Flow

```
DataManager_Widget
    └── OutputDirectoryWidget
            │ dirChanged(path)
            ▼
    DataManager_Widget::_changeOutputDir()
            │ setOutputPath(path)
            ▼
    DataManager::setOutputPath()
```

### Target Flow

```
SettingsWidget → GeneralSettingsPage
    └── OutputDirectoryWidget (reused component)
            │ dirChanged(path)
            ▼
    AppPreferences::setDefaultExportDirectory(path)
            │ preferenceChanged("default_export_directory")
            ▼
    DataManager observes AppPreferences and syncs its output path
```

### Migration Steps

1. Add `AppPreferences` observation in `DataManager` (or in `MainWindow` as glue code) so that changes to `default_export_directory` automatically update `DataManager::setOutputPath()`.
2. Move `OutputDirectoryWidget` from `DataManager_Widget` to the General settings page.
3. Remove output directory UI from `DataManager_Widget`.
4. Existing `DataManager_Widget` code that reads the output path continues to work via `DataManager::getOutputPath()`.

---

## 5. Implementation Roadmap

### Phase 1: SettingsWidget Shell & AppPreferences Pages

**Goal:** Users can open a Settings widget and edit all existing `AppPreferences` fields.

**Tasks:**

1. Create `SettingsWidgetState` (minimal — just tracks selected page index).
2. Create `SettingsWidget` view with QListWidget + QStackedWidget layout.
3. Create built-in pages:
   - **GeneralSettingsPage** — Default TimeFrame key, output directory (reuse `OutputDirectoryWidget`).
   - **PathsSettingsPage** — Default import directory, default export directory.
   - **PythonSettingsPage** — Python env search paths, preferred env.
4. Register `SettingsWidget` in `EditorRegistry` with `Zone::Center`, `allow_multiple = false`.
5. Wire `AppPreferences` signals so the pages read initial values and react to external changes.
6. Migrate output directory out of `DataManager_Widget`.
7. Add menu entry: `Edit → Settings`.

**Files to create:**
- `src/WhiskerToolbox/SettingsWidget/SettingsWidgetState.hpp/.cpp`
- `src/WhiskerToolbox/SettingsWidget/SettingsWidget.hpp/.cpp`
- `src/WhiskerToolbox/SettingsWidget/SettingsWidgetRegistration.hpp/.cpp`
- `src/WhiskerToolbox/SettingsWidget/Pages/GeneralSettingsPage.hpp/.cpp`
- `src/WhiskerToolbox/SettingsWidget/Pages/PathsSettingsPage.hpp/.cpp`
- `src/WhiskerToolbox/SettingsWidget/Pages/PythonSettingsPage.hpp/.cpp`

**Files to modify:**
- `src/WhiskerToolbox/DataManager_Widget/` — Remove output directory section.
- `src/WhiskerToolbox/Main_Window/mainwindow.cpp` — Add Settings registration and menu entry.
- `src/CMakeLists.txt` — Add SettingsWidget to build.

**Dependencies:** None beyond existing infrastructure.

---

### Phase 2: Appearance Settings (Theme & Typography)

**Goal:** Users can switch between light/dark themes and adjust font size.

**Tasks:**

1. Extend `AppPreferencesData` with `AppearancePreferences` struct.
2. Create `ThemeManager` class (loads QSS, applies palette).
3. Create light and dark QSS stylesheets as Qt resources.
4. Create `AppearanceSettingsPage` in the Settings widget.
5. Wire `ThemeManager` to `AppPreferences` changes.
6. Add font size control (`QApplication::setFont()`).

**Files to create:**
- `src/WhiskerToolbox/SettingsWidget/Pages/AppearanceSettingsPage.hpp/.cpp`
- `src/WhiskerToolbox/ThemeManager/ThemeManager.hpp/.cpp`
- `resources/themes/light.qss`
- `resources/themes/dark.qss`

**Risk:** Global QSS can conflict with widget-specific stylesheets (currently used for status label coloring). Audit and convert those to dynamic palette queries.

---

### Phase 3: Widget-Contributed Settings Pages

**Goal:** Widgets can optionally contribute a settings page surfaced in the Settings widget.

**Tasks:**

1. Add `create_settings` field to `EditorTypeInfo`.
2. Modify `SettingsWidget` to query registered types and build "Widgets" category.
3. Define widget defaults structs (e.g., `MediaWidgetDefaults`).
4. Add optional widget defaults fields to `AppPreferencesData`.
5. Implement `create_settings` for 1–2 pilot widgets (e.g., Media widget, DataViewer).
6. Wire widget constructors to read defaults from `AppPreferences` on creation.

**Design decision needed:** Whether widget defaults in `AppPreferencesData` should be a `std::map<std::string, std::string>` (type_id → JSON blob) for full extensibility, or individual typed optional structs. The typed approach is safer (schema-validated at compile time) but requires modifying `AppPreferencesData` for each new widget. The map approach is open for extension but loses compile-time safety. **Recommendation:** Start with typed optionals. If the number of widgets with settings pages exceeds ~5, refactor to a map approach.

---

### Phase 4: Keybinding Editor

**Goal:** Users can view and customize keyboard shortcuts.

**Tasks:**

1. Add `std::vector<KeybindingEntry>` to `AppPreferencesData`.
2. Create `KeybindingManager` (registry of defaults, resolution logic, user overrides).
3. Install global event filter in `MainWindow` for keybinding dispatch.
4. Create `ActionDispatcher` for UI and command action dispatch.
5. Create `KeybindingEditorPage` in Settings widget (table view, recording, conflict detection).
6. Register default keybindings for existing QActions (undo, redo, save, etc.).
7. Migrate existing hardcoded shortcuts:
   - `Media_Window::keyPressEvent()` — group assignment (1–9).
   - `Whisker_Widget::keyPressEvent()` — trace ('T').
   - `PlotDockWidgetContent::eventFilter()` — plot key handling.

**Files to create:**
- `src/WhiskerToolbox/KeybindingManager/KeybindingManager.hpp/.cpp`
- `src/WhiskerToolbox/KeybindingManager/ActionDispatcher.hpp/.cpp`
- `src/WhiskerToolbox/SettingsWidget/Pages/KeybindingEditorPage.hpp/.cpp`

**Risk:** Event filter ordering. The global keybinding filter must not swallow events meant for text input widgets (QLineEdit, QTextEdit, QSpinBox). The filter should skip resolution when a text-input widget has focus, unless the key sequence includes a modifier (Ctrl, Alt, Meta).

---

### Phase 5: Keybinding–Command Integration

**Goal:** Keybindings can directly trigger Command Architecture commands with variable substitution.

**Tasks:**

1. Extend `ActionDispatcher` to create and execute commands via `CommandFactory`.
2. Define `CommandContext` population from the active editor state (current frame, selected data, etc.).
3. Register command-triggered keybindings (e.g., "Ctrl+Shift+S" → `SaveData` command with `${active_data_key}`).
4. Add "Record Macro" functionality — capture a sequence of keybinding-triggered commands as a `CommandSequenceDescriptor`.
5. Allow macro sequences to be assigned to keybindings.
6. Integrate with `CommandRecorder` for action journal logging.

**Dependencies:** Command Architecture Phases 3+ (SaveData, LoadData commands).

---

## 6. Open Questions

1. **Settings widget zone placement.** Is `Zone::Center` the right default? Some applications dock settings in a side panel. Given the potentially large keybinding table, Center seems appropriate for the view, but it could also work as a modal dialog instead of a docked widget.

2. **Settings vs. Preferences naming.** Qt conventions use "Preferences" (macOS) while Windows uses "Settings". The codebase already uses `AppPreferences` for the data model. The widget can be called "Settings" in the UI (matching modern convention) while the data model remains `AppPreferences`.

3. **Widget defaults granularity.** Should the Settings widget allow configuring every property of an `EditorState` as a default, or just a curated subset? Recommendation: curated subset. Each widget author chooses which fields make sense as "defaults" by controlling what their `create_settings` factory exposes.

4. **Keybinding conflict resolution.** When a user assigns a key combo that conflicts with an existing binding: show a warning and require explicit override? Or automatically unbind the old action? Recommendation: show a warning with the conflicting action name and let the user choose.

5. **Text input passthrough.** The global event filter must not intercept keystrokes intended for text editing. Rule: if the focused widget is a `QLineEdit`, `QTextEdit`, `QPlainTextEdit`, `QSpinBox`, `QDoubleSpinBox`, or `QComboBox` (editable), only process key sequences that include `Ctrl`, `Alt`, or `Meta` modifiers. Bare keys (letters, digits) pass through to the text widget.

---

## 7. Dependency Graph

```
Phase 1 ─── Phase 2
  │             │
  └──── Phase 3 ┘
           │
        Phase 4
           │
        Phase 5 ──── Command Architecture (Phase 3+)
```

Phases 1 and 2 are independent of each other but both must precede Phase 3 (which builds on the Settings widget shell). Phase 4 depends on the Settings widget existing. Phase 5 depends on both the keybinding system and the Command Architecture's persistence commands.

---

## 8. References

- [State Management Roadmap](STATE_MANAGEMENT_ROADMAP.md) — AppPreferences, SessionStore, WorkspaceManager design
- [Command Architecture Roadmap](COMMAND_ARCHITECTURE_ROADMAP.md) — ICommand, CommandFactory, variable substitution, triage workflows
- `src/WhiskerToolbox/EditorState/` — EditorState, EditorRegistry, ZoneTypes, SelectionContext
- `src/WhiskerToolbox/StateManagement/` — AppPreferences, AppPreferencesData, SessionStore
- `src/WhiskerToolbox/DataManager_Widget/OutputDirectoryWidget/` — Composable output dir picker (to be migrated)
