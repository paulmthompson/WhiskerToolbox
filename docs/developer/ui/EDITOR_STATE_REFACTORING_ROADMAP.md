# Editor State and Workspace Management Refactoring Roadmap

## Executive Summary

This document outlines a phased approach to refactor WhiskerToolbox's UI architecture toward a more unified, state-driven design. 

**Phase 1 (Core Infrastructure) is now complete and tested.**

The goals are:

1. **Reduce duplication**: Eliminate redundant Feature_Table_Widget instances across widgets
2. **Enable inter-widget communication**: Centralize selection and focus state
3. **Standardize property panels**: Create predictable UI zones for properties
4. **Support serialization**: Enable save/restore of complete application state
5. **Prepare for undo/redo**: Lay groundwork for command pattern

The refactoring introduces three core abstractions:
- **EditorState**: Base class for serializable widget state
- **WorkspaceManager**: Registry for all open editor states
- **SelectionContext**: Centralized selection and focus management

## Inspirations

### Blender's Architecture
- **Editors**: Each editor type (3D View, Outliner, Properties, etc.) has its own state
- **Context**: Global context propagates selection and active objects
- **Data-blocks**: Central registry of all data with observers
- **Operators**: Command pattern for undoable actions
- **Areas/Regions**: Predictable UI zones (main, properties, header, tools)

### Qt Creator / VS Code
- **Editor groups**: Multiple editors can view the same document
- **Selection service**: Centralized selection propagation
- **Command palette**: All actions as first-class commands

## Phase 1: Core Infrastructure (✅ COMPLETE)

**Status**: All core classes implemented and tested. All unit tests passing.

**Deliverables**:
- ✅ [EditorState.hpp](../../WhiskerToolbox/EditorState/EditorState.hpp) - Base class for serializable widget state
- ✅ [SelectionContext.hpp](../../WhiskerToolbox/EditorState/SelectionContext.hpp) - Centralized selection and focus management
- ✅ [WorkspaceManager.hpp](../../WhiskerToolbox/EditorState/WorkspaceManager.hpp) - Registry for editor states
- ✅ [MediaWidgetState.hpp](../../WhiskerToolbox/EditorState/states/MediaWidgetState.hpp) - Example concrete state (template for migrations)
- ✅ [EditorState.test.cpp](../../WhiskerToolbox/EditorState/EditorState.test.cpp) - Comprehensive unit tests (50+ passing)

**Test Results**: All tests passing
- EditorState: Instance IDs, dirty state tracking, JSON serialization
- SelectionContext: Data/entity selection, active editor tracking, properties context routing
- WorkspaceManager: State registry, factory system, workspace serialization
- Signal/slot behavior validated across all components

### 1.1 EditorState Base Class

Create a base class that all widget states will inherit from:

```cpp
// src/WhiskerToolbox/EditorState/EditorState.hpp

#ifndef EDITOR_STATE_HPP
#define EDITOR_STATE_HPP

#include <QObject>
#include <QString>
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <memory>
#include <string>

/**
 * @brief Base class for all editor/widget states
 * 
 * EditorState provides a common interface for:
 * - Serialization to/from JSON (via reflect-cpp)
 * - Unique identification
 * - State change notifications via Qt signals
 * 
 * Subclasses should:
 * 1. Define their state as reflect-cpp compatible structs
 * 2. Override getTypeName() to return a unique type identifier
 * 3. Emit stateChanged() when state is modified
 */
class EditorState : public QObject {
    Q_OBJECT

public:
    explicit EditorState(QObject* parent = nullptr);
    virtual ~EditorState() = default;

    /// @brief Get unique type name for this editor state (e.g., "MediaWidgetState")
    [[nodiscard]] virtual QString getTypeName() const = 0;

    /// @brief Get display name for UI (e.g., "Media Viewer 1")
    [[nodiscard]] virtual QString getDisplayName() const = 0;
    
    /// @brief Set display name (user-customizable)
    virtual void setDisplayName(QString const& name);

    /// @brief Get unique instance ID
    [[nodiscard]] QString getInstanceId() const { return _instance_id; }

    /// @brief Serialize state to JSON string
    [[nodiscard]] virtual std::string toJson() const = 0;

    /// @brief Restore state from JSON string
    /// @return true if successful, false if JSON was invalid
    virtual bool fromJson(std::string const& json) = 0;

    /// @brief Check if this state has unsaved changes
    [[nodiscard]] bool isDirty() const { return _is_dirty; }

    /// @brief Mark state as clean (after save)
    void markClean() { _is_dirty = false; }

signals:
    /// @brief Emitted when any state property changes
    void stateChanged();

    /// @brief Emitted when display name changes
    void displayNameChanged(QString const& newName);

    /// @brief Emitted when dirty state changes
    void dirtyChanged(bool isDirty);

protected:
    /// @brief Mark state as modified (call from setters)
    void markDirty();

    /// @brief Generate a new unique instance ID
    static QString generateInstanceId();

private:
    QString _instance_id;
    QString _display_name;
    bool _is_dirty = false;
};

#endif // EDITOR_STATE_HPP
```

### 1.2 WorkspaceManager

Central registry for all editor states:

```cpp
// src/WhiskerToolbox/EditorState/WorkspaceManager.hpp

#ifndef WORKSPACE_MANAGER_HPP
#define WORKSPACE_MANAGER_HPP

#include <QObject>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

class EditorState;
class SelectionContext;
class DataManager;

/**
 * @brief Central manager for all open editor states
 * 
 * WorkspaceManager provides:
 * - Registry of all active EditorState instances
 * - Factory methods for creating new editor states
 * - Workspace serialization (all states + layout)
 * - Access to SelectionContext for inter-widget communication
 */
class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    explicit WorkspaceManager(std::shared_ptr<DataManager> data_manager,
                              QObject* parent = nullptr);
    ~WorkspaceManager();

    // === State Management ===
    
    /// @brief Register an editor state
    void registerState(std::shared_ptr<EditorState> state);

    /// @brief Unregister an editor state
    void unregisterState(QString const& instance_id);

    /// @brief Get state by instance ID
    [[nodiscard]] std::shared_ptr<EditorState> getState(QString const& instance_id) const;

    /// @brief Get all states of a specific type
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>> 
        getStatesByType(QString const& type_name) const;

    /// @brief Get all registered states
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>> getAllStates() const;

    // === Selection Context ===
    
    /// @brief Get the global selection context
    [[nodiscard]] SelectionContext* selectionContext() const { return _selection_context.get(); }

    // === Serialization ===
    
    /// @brief Serialize entire workspace to JSON
    [[nodiscard]] std::string toJson() const;

    /// @brief Restore workspace from JSON
    /// @return true if successful
    bool fromJson(std::string const& json);

    /// @brief Check if any state has unsaved changes
    [[nodiscard]] bool hasUnsavedChanges() const;

signals:
    /// @brief Emitted when a new state is registered
    void stateRegistered(QString const& instance_id, QString const& type_name);

    /// @brief Emitted when a state is unregistered
    void stateUnregistered(QString const& instance_id);

    /// @brief Emitted when any state changes
    void workspaceChanged();

private:
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<SelectionContext> _selection_context;
    std::unordered_map<std::string, std::shared_ptr<EditorState>> _states;
};

#endif // WORKSPACE_MANAGER_HPP
```

### 1.3 SelectionContext

Centralized selection and focus management:

```cpp
// src/WhiskerToolbox/EditorState/SelectionContext.hpp

#ifndef SELECTION_CONTEXT_HPP
#define SELECTION_CONTEXT_HPP

#include <QObject>
#include <QString>
#include <QPointer>
#include <set>
#include <string>
#include <vector>
#include <optional>

class EditorState;
class QWidget;

/**
 * @brief Source of a selection change
 * 
 * Allows widgets to know if they should respond to a selection change
 * (e.g., don't respond to your own selection).
 */
struct SelectionSource {
    QString editor_instance_id;  // Which editor made the selection
    QString widget_id;           // Specific widget within editor (optional)
};

/**
 * @brief Represents a selected data item
 */
struct SelectedItem {
    QString data_key;            // Key in DataManager
    std::optional<int64_t> entity_id;  // Specific entity (optional)
    std::optional<int> time_index;     // Specific time (optional)
    
    bool operator<(SelectedItem const& other) const;
    bool operator==(SelectedItem const& other) const;
};

/**
 * @brief Centralized selection and focus context
 * 
 * SelectionContext manages:
 * - Current data selection (which data keys are selected)
 * - Active editor state (which editor has focus)
 * - Last interaction context (for property panel routing)
 * - Entity selection within data objects
 * 
 * Widgets observe SelectionContext to stay synchronized.
 */
class SelectionContext : public QObject {
    Q_OBJECT

public:
    explicit SelectionContext(QObject* parent = nullptr);
    ~SelectionContext() = default;

    // === Data Selection ===
    
    /// @brief Set the primary selected data key
    void setSelectedData(QString const& data_key, SelectionSource const& source);

    /// @brief Add to the current selection (for multi-select)
    void addToSelection(QString const& data_key, SelectionSource const& source);

    /// @brief Remove from current selection
    void removeFromSelection(QString const& data_key, SelectionSource const& source);

    /// @brief Clear all selections
    void clearSelection(SelectionSource const& source);

    /// @brief Get primary selected data key
    [[nodiscard]] QString primarySelectedData() const;

    /// @brief Get all selected data keys
    [[nodiscard]] std::set<QString> allSelectedData() const;

    /// @brief Check if specific data is selected
    [[nodiscard]] bool isSelected(QString const& data_key) const;

    // === Entity Selection ===
    
    /// @brief Set selected entities within current data
    void setSelectedEntities(std::vector<int64_t> const& entity_ids, 
                            SelectionSource const& source);

    /// @brief Get selected entity IDs
    [[nodiscard]] std::vector<int64_t> selectedEntities() const;

    // === Active Editor ===
    
    /// @brief Set the currently active (focused) editor
    void setActiveEditor(QString const& instance_id);

    /// @brief Get the active editor instance ID
    [[nodiscard]] QString activeEditorId() const;

    /// @brief Get the active editor state (may be null)
    [[nodiscard]] EditorState* activeEditorState() const;

    // === Property Panel Context ===
    
    /**
     * @brief Context for determining which properties panel to show
     * 
     * This helps the properties zone know which editor's properties
     * to display based on user interaction patterns.
     */
    struct PropertiesContext {
        QString last_interacted_editor;  // Editor that had last meaningful interaction
        QString selected_data_key;       // Currently selected data
        QString data_type;               // Type of selected data
    };

    /// @brief Get current properties context
    [[nodiscard]] PropertiesContext propertiesContext() const;

    /// @brief Notify that an editor had meaningful user interaction
    void notifyInteraction(QString const& editor_instance_id);

signals:
    /// @brief Emitted when data selection changes
    void selectionChanged(SelectionSource const& source);

    /// @brief Emitted when entity selection changes
    void entitySelectionChanged(SelectionSource const& source);

    /// @brief Emitted when active editor changes
    void activeEditorChanged(QString const& instance_id);

    /// @brief Emitted when properties context changes
    void propertiesContextChanged();

private:
    QString _primary_selected;
    std::set<QString> _selected_data;
    std::vector<int64_t> _selected_entities;
    QString _active_editor_id;
    QString _last_interacted_editor;

    // Weak reference to workspace manager (set by WorkspaceManager)
    friend class WorkspaceManager;
    class WorkspaceManager* _workspace_manager = nullptr;
};

#endif // SELECTION_CONTEXT_HPP
```

### 1.4 Directory Structure

```
src/WhiskerToolbox/EditorState/
├── CMakeLists.txt
├── EditorState.hpp
├── EditorState.cpp
├── WorkspaceManager.hpp
├── WorkspaceManager.cpp
├── SelectionContext.hpp
├── SelectionContext.cpp
├── states/                    # Concrete state implementations
│   ├── MediaWidgetState.hpp
│   ├── DataViewerState.hpp
│   └── ...
└── EditorState.test.cpp
```

### 1.5 Integration with MainWindow

```cpp
// In MainWindow
class MainWindow : public QMainWindow {
    // ...
private:
    std::unique_ptr<WorkspaceManager> _workspace_manager;
    
    // Properties dock (always visible, content changes based on context)
    ads::CDockWidget* _properties_dock;
    QStackedWidget* _properties_stack;
};
```

## Phase 2: First Widget Migration - Media_Widget (Weeks 4-6)

### 2.1 MediaWidgetState

Extract state from Media_Widget:

```cpp
// src/WhiskerToolbox/EditorState/states/MediaWidgetState.hpp

struct MediaWidgetStateData {
    // Displayed features with their enable state
    std::map<std::string, bool> displayed_features;
    
    // Per-feature colors
    std::map<std::string, std::string> feature_colors;
    
    // Viewport state
    double zoom_level = 1.0;
    double pan_x = 0.0;
    double pan_y = 0.0;
    
    // Canvas size
    int canvas_width = 800;
    int canvas_height = 600;
    
    // Current frame (synchronized via TimeScrollBar)
    // Note: This is shared state, may not be serialized per-widget
};

class MediaWidgetState : public EditorState {
    Q_OBJECT
public:
    // ... standard interface ...
    
    // Typed accessors
    void setFeatureEnabled(std::string const& key, bool enabled);
    bool isFeatureEnabled(std::string const& key) const;
    
    void setFeatureColor(std::string const& key, std::string const& hex);
    std::string getFeatureColor(std::string const& key) const;
    
    void setZoom(double zoom);
    double getZoom() const;

signals:
    void featureEnabledChanged(QString const& key, bool enabled);
    void featureColorChanged(QString const& key, QString const& color);
    void viewportChanged();

private:
    MediaWidgetStateData _data;
};
```

### 2.2 Refactored Media_Widget

```cpp
class Media_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Media_Widget(std::shared_ptr<MediaWidgetState> state,
                          std::shared_ptr<DataManager> data_manager,
                          SelectionContext* selection_context,
                          QWidget* parent = nullptr);

    // State is now external - widget observes it
    MediaWidgetState* state() const { return _state.get(); }

private:
    std::shared_ptr<MediaWidgetState> _state;
    SelectionContext* _selection_context;
    
    // Connect state signals to widget updates
    void connectStateSignals();
    
    // Connect selection context
    void connectSelectionContext();
};
```

### 2.3 Central Feature Table

Replace per-widget Feature_Table_Widget with a central one in MainWindow:

```cpp
// Properties panel zone contains Feature_Table_Widget
// It observes SelectionContext to show appropriate properties

class CentralFeatureTable : public QWidget {
    Q_OBJECT
public:
    CentralFeatureTable(WorkspaceManager* workspace_manager,
                        std::shared_ptr<DataManager> data_manager,
                        QWidget* parent = nullptr);

private slots:
    // When user selects data in table
    void onFeatureSelected(QString const& feature);
    
    // When selection context changes
    void onSelectionContextChanged();
    
    // When active editor changes
    void onActiveEditorChanged(QString const& editor_id);

private:
    WorkspaceManager* _workspace_manager;
    Feature_Table_Widget* _feature_table;
    
    // Filter based on active editor's capabilities
    void updateFilterForEditor(EditorState* editor);
};
```

## Phase 3: Properties Zone Architecture (Weeks 7-9)

### 3.1 Properties Panel System

```cpp
// src/WhiskerToolbox/PropertiesPanel/PropertiesHost.hpp

/**
 * @brief Widget that hosts context-sensitive properties panels
 * 
 * PropertiesHost observes SelectionContext and shows the appropriate
 * properties panel based on:
 * 1. Active editor type
 * 2. Selected data type
 * 3. Last interaction context
 */
class PropertiesHost : public QWidget {
    Q_OBJECT

public:
    PropertiesHost(WorkspaceManager* workspace_manager,
                   QWidget* parent = nullptr);

    /// @brief Register a properties widget for a specific editor type
    void registerPropertiesWidget(QString const& editor_type,
                                  std::function<QWidget*(EditorState*)> factory);

    /// @brief Register a properties widget for a specific data type
    void registerDataPropertiesWidget(QString const& data_type,
                                      std::function<QWidget*(QString const& data_key)> factory);

private slots:
    void onPropertiesContextChanged();

private:
    WorkspaceManager* _workspace_manager;
    QStackedWidget* _stack;
    
    // Cache of created property widgets
    std::map<QString, QWidget*> _editor_properties;
    std::map<QString, QWidget*> _data_properties;
    
    // Factories
    std::map<QString, std::function<QWidget*(EditorState*)>> _editor_factories;
    std::map<QString, std::function<QWidget*(QString)>> _data_factories;
};
```

### 3.2 Standard UI Zones

Define standard zones in MainWindow layout:

```
┌──────────────────────────────────────────────────────────────────┐
│  Menu Bar                                                        │
├────────────────┬─────────────────────────────┬───────────────────┤
│                │                             │                   │
│   Toolbox      │     Main Editor Area        │   Properties      │
│   (Outliner)   │     (Media, DataViewer,     │   (Context-       │
│                │      Plots, etc.)           │    sensitive)     │
│   - Data Tree  │                             │                   │
│   - Groups     │                             │   - Feature       │
│                │                             │     Table         │
│                │                             │   - Editor        │
│                │                             │     Properties    │
│                │                             │   - Data          │
│                │                             │     Properties    │
│                │                             │                   │
├────────────────┴─────────────────────────────┴───────────────────┤
│  Timeline / DataViewer Strip (optional)                          │
├──────────────────────────────────────────────────────────────────┤
│  Status Bar / Terminal                                           │
└──────────────────────────────────────────────────────────────────┘
```

### 3.3 Zone Manager

```cpp
// Manages standard dock zones
class ZoneManager {
public:
    enum class Zone {
        Toolbox,      // Left panel - outliner, data tree
        MainEditor,   // Center - primary editing area
        Properties,   // Right panel - context properties
        Timeline,     // Bottom - time-based views
        Output        // Bottom - terminal, logs
    };

    /// @brief Get the dock area for a zone
    ads::CDockAreaWidget* getZoneArea(Zone zone) const;
    
    /// @brief Add a dock widget to a specific zone
    void addToZone(ads::CDockWidget* dock, Zone zone);
    
    /// @brief Get default zone for an editor type
    Zone getDefaultZone(QString const& editor_type) const;
};
```

## Phase 4: Command Pattern Foundation (Weeks 10-12)

### 4.1 Command Base Class

```cpp
// src/WhiskerToolbox/Commands/Command.hpp

/**
 * @brief Base class for undoable commands
 * 
 * Commands encapsulate actions that can be:
 * - Executed (do)
 * - Undone (undo)
 * - Redone (redo)
 * - Serialized to JSON
 * - Merged with subsequent commands (optional)
 */
class Command {
public:
    virtual ~Command() = default;

    /// @brief Execute the command
    virtual void execute() = 0;

    /// @brief Undo the command
    virtual void undo() = 0;

    /// @brief Get command name for display
    [[nodiscard]] virtual QString name() const = 0;

    /// @brief Get command ID for serialization
    [[nodiscard]] virtual QString typeId() const = 0;

    /// @brief Serialize command to JSON
    [[nodiscard]] virtual std::string toJson() const = 0;

    /// @brief Can this command be merged with another?
    [[nodiscard]] virtual bool canMergeWith(Command const* other) const { return false; }

    /// @brief Merge with another command (for compression)
    virtual void mergeWith(Command const* other) {}

    /// @brief Timestamp when command was created
    [[nodiscard]] std::chrono::system_clock::time_point timestamp() const { return _timestamp; }

protected:
    std::chrono::system_clock::time_point _timestamp = std::chrono::system_clock::now();
};
```

### 4.2 Command Manager

```cpp
// src/WhiskerToolbox/Commands/CommandManager.hpp

class CommandManager : public QObject {
    Q_OBJECT

public:
    explicit CommandManager(QObject* parent = nullptr);

    /// @brief Execute a command and add to history
    void execute(std::unique_ptr<Command> command);

    /// @brief Undo last command
    void undo();

    /// @brief Redo last undone command
    void redo();

    /// @brief Check if undo is available
    [[nodiscard]] bool canUndo() const;

    /// @brief Check if redo is available
    [[nodiscard]] bool canRedo() const;

    /// @brief Get command history for serialization
    [[nodiscard]] std::vector<std::string> historyToJson() const;

    /// @brief Restore history from JSON
    void historyFromJson(std::vector<std::string> const& commands);

signals:
    void commandExecuted(QString const& command_name);
    void undone(QString const& command_name);
    void redone(QString const& command_name);
    void historyChanged();

private:
    std::vector<std::unique_ptr<Command>> _history;
    size_t _current_index = 0;
    static constexpr size_t MAX_HISTORY = 100;
};
```

### 4.3 Example Commands

```cpp
// Change data enable state in media widget
class SetFeatureEnabledCommand : public Command {
public:
    SetFeatureEnabledCommand(std::shared_ptr<MediaWidgetState> state,
                             std::string feature_key,
                             bool enabled);

    void execute() override;
    void undo() override;
    QString name() const override;
    QString typeId() const override { return "SetFeatureEnabled"; }
    std::string toJson() const override;

private:
    std::weak_ptr<MediaWidgetState> _state;
    std::string _feature_key;
    bool _new_value;
    bool _old_value;
};

// Create new data object
class CreateDataCommand : public Command {
public:
    CreateDataCommand(std::shared_ptr<DataManager> data_manager,
                     std::string key,
                     DM_DataType type);

    void execute() override;
    void undo() override;
    QString name() const override { return QString("Create %1").arg(QString::fromStdString(_key)); }
    QString typeId() const override { return "CreateData"; }
    std::string toJson() const override;

private:
    std::weak_ptr<DataManager> _data_manager;
    std::string _key;
    DM_DataType _type;
};
```

## Phase 5: Widget Migrations (Weeks 13-20)

### 5.1 Migration Order (by complexity and dependencies)

1. **Media_Widget** (Phase 2) - Foundation widget
2. **DataViewer_Widget** - Similar pattern to Media_Widget
3. **DataManager_Widget** - Extract state, simplify to data browser
4. **Analysis_Dashboard** - Already has some patterns, refine
5. **TableDesignerWidget** - Complex, benefits most from state separation
6. **DataTransform_Widget** - Migrate to transform v2 simultaneously

### 5.2 DataViewerState Example

```cpp
struct DataViewerStateData {
    // Displayed series
    std::vector<std::string> displayed_series;
    
    // Per-series configuration
    std::map<std::string, AnalogDisplayConfig> analog_configs;
    std::map<std::string, EventDisplayConfig> event_configs;
    std::map<std::string, IntervalDisplayConfig> interval_configs;
    
    // View settings
    int x_axis_samples = 1000;
    double global_scale = 1.0;
    int theme_index = 0;
    bool grid_enabled = true;
    int grid_spacing = 100;
    double vertical_spacing = 1.0;
};

class DataViewerState : public EditorState {
    // ... similar pattern to MediaWidgetState
};
```

### 5.3 Widget Factory

```cpp
// Centralized widget creation
class EditorFactory {
public:
    struct EditorInfo {
        QString type_id;
        QString display_name;
        QString icon_path;
        ZoneManager::Zone default_zone;
    };

    /// @brief Register an editor type
    template<typename StateT, typename WidgetT>
    void registerEditor(EditorInfo info);

    /// @brief Get available editor types
    std::vector<EditorInfo> availableEditors() const;

    /// @brief Create new editor with state
    std::pair<std::shared_ptr<EditorState>, QWidget*> 
        createEditor(QString const& type_id, WorkspaceManager* workspace);
};
```

## Phase 6: Drag and Drop & Advanced Features (Weeks 21-24)

### 6.1 Drag and Drop from Feature Table

```cpp
// In Feature_Table_Widget
void Feature_Table_Widget::startDrag(Qt::DropActions supportedActions) {
    auto* drag = new QDrag(this);
    auto* mime = new QMimeData();
    
    // Custom MIME type for data keys
    mime->setData("application/x-whiskertoolbox-datakey", 
                  _highlighted_feature.toUtf8());
    
    drag->setMimeData(mime);
    drag->exec(supportedActions);
}

// In target widgets (e.g., Media_Widget)
void Media_Widget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat("application/x-whiskertoolbox-datakey")) {
        QString key = QString::fromUtf8(
            event->mimeData()->data("application/x-whiskertoolbox-datakey"));
        
        // Add to this widget's state
        _state->setFeatureEnabled(key.toStdString(), true);
        event->acceptProposedAction();
    }
}
```

### 6.2 Session Serialization

```cpp
// Complete session state
struct SessionData {
    // Workspace state
    std::string workspace_json;
    
    // Layout state (ADS dock state)
    std::string dock_layout;
    
    // Command history
    std::vector<std::string> command_history;
    
    // Recent files
    std::vector<std::string> recent_files;
    
    // Window geometry
    int window_x, window_y, window_w, window_h;
    bool maximized;
};

class SessionManager {
public:
    void saveSession(QString const& path);
    void loadSession(QString const& path);
    void autoSave();
    void restoreLastSession();
};
```

## Implementation Guidelines

### State/Widget Separation Rules

1. **State holds data, Widget holds appearance**
   - State: Which features are enabled, their colors, zoom level
   - Widget: QGraphicsView, layout, signal connections

2. **State is the single source of truth**
   - Widget reads from state, never caches state data
   - Widget writes to state via setters (which may create commands)

3. **State is serializable, Widget is not**
   - State classes use reflect-cpp structs
   - All state changes must be representable as JSON

4. **State signals are typed and specific**
   - `featureEnabledChanged(key, enabled)` not just `stateChanged()`
   - Allows efficient widget updates

### SelectionContext Usage Patterns

```cpp
// Widget receiving selection changes
void Media_Widget::connectSelectionContext() {
    connect(_selection_context, &SelectionContext::selectionChanged,
            this, [this](SelectionSource const& source) {
        // Don't respond to our own selections
        if (source.editor_instance_id == _state->getInstanceId()) {
            return;
        }
        
        // Update display based on new selection
        updateSelectionHighlight();
    });
}

// Widget making selections
void Media_Widget::onFeatureClicked(QString const& key) {
    SelectionSource source{_state->getInstanceId(), "canvas"};
    _selection_context->setSelectedData(key, source);
}
```

### Command Pattern Usage

```cpp
// In widget event handler
void Media_Widget::onColorChangeRequested(QString const& key, QColor color) {
    auto cmd = std::make_unique<SetFeatureColorCommand>(
        _state, key.toStdString(), color.name().toStdString());
    
    // Execute via command manager for undo support
    _command_manager->execute(std::move(cmd));
}
```

## Testing Strategy

### Unit Tests

```cpp
// EditorState serialization round-trip
TEST_CASE("MediaWidgetState serialization") {
    MediaWidgetState state;
    state.setFeatureEnabled("whiskers", true);
    state.setFeatureColor("whiskers", "#FF0000");
    state.setZoom(2.5);
    
    auto json = state.toJson();
    
    MediaWidgetState restored;
    REQUIRE(restored.fromJson(json));
    REQUIRE(restored.isFeatureEnabled("whiskers") == true);
    REQUIRE(restored.getFeatureColor("whiskers") == "#FF0000");
    REQUIRE(restored.getZoom() == Approx(2.5));
}

// SelectionContext behavior
TEST_CASE("SelectionContext multi-select") {
    SelectionContext ctx;
    SelectionSource src{"editor1", "table"};
    
    ctx.setSelectedData("data1", src);
    ctx.addToSelection("data2", src);
    
    REQUIRE(ctx.primarySelectedData() == "data1");
    REQUIRE(ctx.allSelectedData().size() == 2);
    REQUIRE(ctx.isSelected("data2"));
}

// Command undo/redo
TEST_CASE("Command undo/redo") {
    auto state = std::make_shared<MediaWidgetState>();
    CommandManager mgr;
    
    mgr.execute(std::make_unique<SetFeatureEnabledCommand>(state, "data", true));
    REQUIRE(state->isFeatureEnabled("data"));
    
    mgr.undo();
    REQUIRE_FALSE(state->isFeatureEnabled("data"));
    
    mgr.redo();
    REQUIRE(state->isFeatureEnabled("data"));
}
```

### Integration Tests

```cpp
// Workspace serialization with multiple editors
TEST_CASE("Workspace round-trip") {
    auto dm = std::make_shared<DataManager>();
    WorkspaceManager workspace(dm);
    
    // Create multiple editors
    auto media_state = std::make_shared<MediaWidgetState>();
    media_state->setDisplayName("Media 1");
    workspace.registerState(media_state);
    
    auto viewer_state = std::make_shared<DataViewerState>();
    viewer_state->setDisplayName("Viewer 1");
    workspace.registerState(viewer_state);
    
    // Serialize
    auto json = workspace.toJson();
    
    // Restore to new workspace
    WorkspaceManager restored(dm);
    REQUIRE(restored.fromJson(json));
    REQUIRE(restored.getAllStates().size() == 2);
}
```

### Fuzz Testing

JSON schemas generated by reflect-cpp enable fuzzing:

```cpp
// Generate corpus from schema
auto schema = rfl::json::to_schema<MediaWidgetStateData>();
// Use schema-aware fuzzer to generate valid JSON variants
```

## Migration Checklist Per Widget

- [ ] Define `{WidgetName}StateData` struct with reflect-cpp
- [ ] Create `{WidgetName}State : EditorState` class
- [ ] Extract state from existing widget to state class
- [ ] Refactor widget constructor to accept shared_ptr<State>
- [ ] Connect widget to state signals
- [ ] Connect widget to SelectionContext
- [ ] Create properties widget for this editor type
- [ ] Register with EditorFactory
- [ ] Add unit tests for state serialization
- [ ] Add integration tests with WorkspaceManager
- [ ] Update MainWindow to use new pattern
- [ ] Remove redundant Feature_Table_Widget instances

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Large refactoring scope | High | Phased approach, one widget at a time |
| Breaking existing functionality | High | Comprehensive testing, feature flags |
| Performance overhead from indirection | Medium | Profile hot paths, optimize if needed |
| Signal/slot connection complexity | Medium | Clear ownership rules, weak pointers |
| JSON schema evolution | Medium | Version field in schemas, migration code |

## Success Metrics

1. **Code reduction**: 30%+ reduction in Feature_Table_Widget instances
2. **Properties consolidation**: Single properties zone in UI
3. **Session restore**: Can save/restore complete application state
4. **Test coverage**: 80%+ coverage on EditorState classes
5. **User feedback**: Improved perceived organization of UI

## Timeline Summary

| Phase | Status | Duration | Deliverables |
|-------|--------|----------|--------------|
| 1. Core Infrastructure | ✅ COMPLETE | 3 weeks | EditorState, WorkspaceManager, SelectionContext |
| 2. Media_Widget Migration | ⏭️ NEXT | 3 weeks | MediaWidgetState, refactored Media_Widget |
| 3. Properties Zone | 📋 PLANNED | 3 weeks | PropertiesHost, ZoneManager, central Feature_Table |
| 4. Command Pattern | 📋 PLANNED | 3 weeks | Command base, CommandManager, example commands |
| 5. Widget Migrations | 📋 PLANNED | 8 weeks | All major widgets migrated |
| 6. Advanced Features | 📋 PLANNED | 4 weeks | Drag/drop, session management |

**Elapsed: ~3 weeks | Remaining: ~21 weeks (~5 months)**

**Progress**: 12.5% Complete (Phase 1 of 6)

## Next Steps (Phase 2 - Media_Widget Migration)

1. **Integrate WorkspaceManager into MainWindow**
   - Create workspace manager instance as member of MainWindow
   - Register MediaWidgetState factory with editor type info
   - Initialize SelectionContext observers for UI updates
   - Set up signal routing between selection context and widgets

2. **Begin Media_Widget Refactoring**
   - Update Media_Widget constructor to accept shared_ptr<MediaWidgetState>
   - Connect widget to state signals for reactive updates (featureEnabledChanged, zoomChanged, etc.)
   - Connect widget to SelectionContext for inter-widget communication
   - Preserve all existing functionality during transition
   - Add tests verifying state/widget integration

3. **Create Properties Zone Infrastructure**
   - Implement PropertiesHost widget (context-sensitive properties panel)
   - Design properties panel routing logic based on SelectionContext
   - Create central Feature_Table_Widget for properties zone (replacing per-widget copies)
   - Connect central feature table to SelectionContext

4. **Post-Phase 2 Validation**
   - Verify Media_Widget state can be serialized/deserialized
   - Test workspace save/load with media viewer state
   - Validate multi-widget communication via SelectionContext
   - Profile for any performance regressions

## References

- [Blender Architecture](https://wiki.blender.org/wiki/Source/Architecture)
- [Qt Model/View Architecture](https://doc.qt.io/qt-6/model-view-programming.html)
- [Command Pattern](https://refactoring.guru/design-patterns/command)
- [reflect-cpp Documentation](https://github.com/getml/reflect-cpp)
- [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System)
