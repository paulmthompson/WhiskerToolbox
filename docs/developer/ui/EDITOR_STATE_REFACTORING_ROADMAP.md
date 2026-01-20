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
- ✅ [EditorState.test.cpp](../../WhiskerToolbox/EditorState/EditorState.test.cpp) - Comprehensive unit tests (50+ passing)

**Test Results**: All tests passing
- EditorState: Instance IDs, dirty state tracking, JSON serialization
- SelectionContext: Data/entity selection, active editor tracking, properties context routing
- WorkspaceManager: State registry, factory system, workspace serialization
- Signal/slot behavior validated across all components

**Key Design Points**:
- EditorState provides base functionality for all widget states
- SelectionContext enables widgets to communicate without direct dependencies
- WorkspaceManager acts as central registry and provides workspace-level serialization
- All core classes use Qt signals for reactive updates
- Full reflect-cpp integration for JSON serialization

### 1.1 Concrete State Classes

**Important**: Concrete EditorState subclasses (like `MediaWidgetState`, `DataViewerState`, etc.) should be implemented alongside their corresponding widgets, **not** in the EditorState library itself. This keeps widget-specific state logic co-located with the widget implementation.

Example locations:
- `src/WhiskerToolbox/Media_Widget/MediaWidgetState.hpp` (not in EditorState/states/)
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerState.hpp`
- `src/WhiskerToolbox/Analysis_Dashboard/AnalysisDashboardState.hpp`

The EditorState library provides only the base infrastructure. Each widget module is responsible for defining its own state class.

### 1.2 Directory Structure

```
src/WhiskerToolbox/EditorState/
├── CMakeLists.txt
├── EditorState.hpp            # Base class for all states
├── EditorState.cpp
├── WorkspaceManager.hpp       # Central state registry
├── WorkspaceManager.cpp
├── SelectionContext.hpp       # Inter-widget communication
├── SelectionContext.cpp
└── EditorState.test.cpp       # Unit tests

src/WhiskerToolbox/Media_Widget/
├── Media_Widget.hpp
├── Media_Widget.cpp
├── MediaWidgetState.hpp       # State for Media_Widget
├── MediaWidgetState.cpp
└── ...

src/WhiskerToolbox/DataViewer_Widget/
├── DataViewer_Widget.hpp
├── DataViewer_Widget.cpp
├── DataViewerState.hpp        # State for DataViewer_Widget
├── DataViewerState.cpp
└── ...
```

## Phase 2: Incremental State Integration and Communication (Weeks 4-8)

**Strategy**: Create minimal EditorState subclasses first, establish inter-widget communication, then gradually migrate state. Do NOT attempt to replace widget internals all at once.

### 2.1 Create Minimal State Classes (Week 4) ✅ COMPLETE

Create empty/minimal state classes that widgets can hold alongside existing members:

**Deliverables**:
- [x] Create `DataManagerWidgetState.hpp/.cpp` in DataManager_Widget directory
- [x] Create `MediaWidgetState.hpp/.cpp` in Media_Widget directory  
- [x] Update widget constructors to create and hold state object
- [x] Register state with WorkspaceManager when widget is created
- [x] Unregister state when widget is destroyed
- [x] Widgets keep all existing member variables - state is additional

### 2.2 Integrate WorkspaceManager into MainWindow (Week 4) ✅ COMPLETE

**Deliverables**:
- [x] Add WorkspaceManager member to MainWindow
- [x] Initialize in constructor after DataManager
- [x] Provide accessor for child widgets
- [x] Update widget creation to pass workspace_manager pointer

### 2.3 Connect DataManager_Widget Selection (Week 5) ✅ COMPLETE

Establish first communication path: DataManager_Widget → SelectionContext

```cpp
// In DataManager_Widget constructor
DataManager_Widget::DataManager_Widget(WorkspaceManager* workspace_manager, ...) {
    _state = std::make_shared<DataManagerWidgetState>();
    workspace_manager->registerState(_state);
    _selection_context = workspace_manager->selectionContext();
    
    // When feature table selection changes, update state
    connect(_feature_table, &Feature_Table_Widget::highlightedFeatureChanged,
            this, [this](QString const& key) {
        _state->setSelectedDataKey(key);
    });
    
    // When state changes, notify SelectionContext
    connect(_state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
            this, [this](QString const& key) {
        SelectionSource source{_state->getInstanceId(), "feature_table"};
        _selection_context->setSelectedData(key, source);
    });
}
```

**Deliverables**:
- [x] Update DataManager_Widget to create and register state
- [x] Connect Feature_Table_Widget signals to state
- [x] Connect state signals to SelectionContext
- [x] Test: Selecting in feature table should update SelectionContext
- [x] Integration tests verify signal propagation and SelectionSource identification

**Implementation Details**:
- Added `_state` (shared_ptr<DataManagerWidgetState>) member to DataManager_Widget
- Added `_selection_context` pointer to access WorkspaceManager's SelectionContext
- Included necessary headers: `DataManagerWidgetState.hpp`, `EditorState/SelectionContext.hpp`, `EditorState/WorkspaceManager.hpp`
- Connected Feature_Table_Widget::featureSelected → state->setSelectedDataKey()
- Connected DataManagerWidgetState::selectedDataKeyChanged → SelectionContext->setSelectedData()
- State unregistered from WorkspaceManager in destructor

**Test Coverage**:
- Added 5 comprehensive integration tests in DataManagerWidgetState.test.cpp
- Tests verify: state signal propagation, SelectionContext updates, multiple state coordination, SelectionSource identification
- All tests passing

**Files Modified**:
- [DataManager_Widget.hpp](../../WhiskerToolbox/DataManager_Widget/DataManager_Widget.hpp) - Added state and context members, forward declarations
- [DataManager_Widget.cpp](../../WhiskerToolbox/DataManager_Widget/DataManager_Widget.cpp) - Implemented state initialization, signal connections, registration/unregistration
- [DataManagerWidgetState.test.cpp](../../WhiskerToolbox/DataManager_Widget/DataManagerWidgetState.test.cpp) - Added integration test suite

### 2.4 Connect Media_Widget as Listener (Week 6)

Make Media_Widget respond to SelectionContext changes:

```cpp
// In Media_Widget constructor
Media_Widget::Media_Widget(WorkspaceManager* workspace_manager, ...) {
    _state = std::make_shared<MediaWidgetState>();
    workspace_manager->registerState(_state);
    _selection_context = workspace_manager->selectionContext();
    
    // Listen to SelectionContext
    connect(_selection_context, &SelectionContext::selectionChanged,
            this, [this](SelectionSource const& source) {
        // Don't respond to our own changes
        if (source.editor_instance_id == _state->getInstanceId()) {
            return;
        }
        
        // Get selected data from context
        QString selected_key = _selection_context->primarySelectedData();
        
        // If we can display this data type, offer to load it
        // (for now, just highlight it if already loaded)
        highlightFeatureIfPresent(selected_key);
    });
}
```

**Deliverables**:
- [ ] Update Media_Widget to create and register state
- [ ] Connect to SelectionContext::selectionChanged
- [ ] Implement response to external selection changes
- [ ] Test: Selecting in DataManager should highlight in Media_Widget

### 2.5 Validate Communication (Week 6)

**Test the communication chain**:
1. Open DataManager_Widget and Media_Widget
2. Select a feature in DataManager's feature table
3. Verify SelectionContext receives the selection
4. Verify Media_Widget responds to the selection
5. Verify Media_Widget doesn't respond to its own selections (no loops)

**Deliverables**:
- [ ] Write integration test for selection propagation
- [ ] Add logging to track selection flow
- [ ] Verify no circular update loops
- [ ] Document any edge cases discovered

### 2.6 Gradually Migrate State (Weeks 7-8)

**Only after communication works**, start moving individual pieces of state:

**Week 7 - Simple state migration**:
```cpp
// Add to MediaWidgetState incrementally
struct MediaWidgetStateData {
    QString displayed_data_key;  // Already there
    double zoom_level = 1.0;     // Add zoom
    double pan_x = 0.0;          // Add pan
    double pan_y = 0.0;
};

// In Media_Widget, replace member variables one at a time:
// Before: double _zoom_level;
// After:  Use _state->getZoom() instead

// Keep existing Feature_Table_Widget for now!
```

**Week 8 - Feature visibility state**:
```cpp
// Add feature configuration to state
struct MediaWidgetStateData {
    QString displayed_data_key;
    double zoom_level = 1.0;
    double pan_x = 0.0;
    double pan_y = 0.0;
    std::map<std::string, bool> feature_enabled;     // New
    std::map<std::string, std::string> feature_colors;  // New
};

// Widget still has Feature_Table_Widget - it now reads/writes state
```

**Deliverables**:
- [ ] Move viewport state (zoom/pan) to MediaWidgetState
- [ ] Update Media_Widget to use state for viewport
- [ ] Move feature enable/color state to MediaWidgetState
- [ ] Update Media_Widget to use state for features
- [ ] Keep existing Feature_Table_Widget - it becomes a view of state
- [ ] Test serialization of migrated state

### 2.7 Summary of Phase 2 Approach

**Key principles**:
1. ✅ State classes start minimal - just enough for communication
2. ✅ Widgets hold state alongside existing members - not replacing them
3. ✅ Establish communication first, validate it works
4. ✅ Migrate state piece by piece, not all at once
5. ✅ Keep existing UI elements - they become views of state
6. ❌ Don't remove Feature_Table_Widget yet - that's Phase 3

## Phase 3: Central Properties Zone (Weeks 9-11)

**Prerequisites**: Phase 2 complete - widgets have state objects and communicate via SelectionContext.

**Goal**: Create a unified properties panel that replaces per-widget Feature_Table_Widget instances.

### 3.1 PropertiesHost Widget (Week 9)

Create a context-sensitive properties container:
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
| 2. State Integration & Communication | 🔄 IN PROGRESS | 5 weeks | Minimal states, inter-widget communication, incremental migration |
| 3. Central Properties Zone | 📋 PLANNED | 3 weeks | PropertiesHost, unified Feature_Table_Widget |
| 4. Command Pattern | 📋 PLANNED | 3 weeks | Command base, CommandManager, example commands |
| 5. Widget Migrations | 📋 PLANNED | 8 weeks | Remaining widgets (DataViewer, Analysis, Tables) |
| 6. Advanced Features | 📋 PLANNED | 4 weeks | Drag/drop, session management |

**Elapsed: ~5 weeks | Remaining: ~21 weeks (~5 months)**

**Progress**: 30% Complete (Phase 1 complete, Phase 2.1-2.3 complete)

## Next Steps (Phase 2.4 - Connect Media_Widget as Listener)

### Overview
Media_Widget will become the first listener to SelectionContext changes, demonstrating the cross-widget communication pattern established in Phase 2.3. When DataManager_Widget broadcasts a selection, Media_Widget will highlight or load the selected data.

### Step 1: Create MediaWidgetState (Week 6, Days 1-2)
- Define `MediaWidgetStateData` struct with reflect-cpp for serialization
- Create `MediaWidgetState : EditorState` class in Media_Widget directory
- Implement basic state management for displayed data key
- Add signals for state changes (similar to DataManagerWidgetState)

### Step 2: Integrate State into Media_Widget (Week 6, Days 3-4)
- Add `_state` (shared_ptr<MediaWidgetState>) member to Media_Widget
- Add `_selection_context` pointer for external selection access
- Initialize state in constructor and register with WorkspaceManager
- Store reference to SelectionContext from WorkspaceManager
- Unregister state in destructor

### Step 3: Connect SelectionContext Changes (Week 6, Days 5 & Week 7, Days 1-2)
- Connect SelectionContext::selectionChanged to Media_Widget slot
- Implement `onSelectionChanged(SelectionSource const& source)` that:
  - Filters out own instance ID (avoid self-response)
  - Gets selected data key from SelectionContext::primarySelectedData()
  - Checks if data is displayable (image, point, line, or mask data)
  - Highlights feature if already loaded
  - Optionally displays notification to load data if not present
- Test with DataManager_Widget selections

### Step 4: Verify No Circular Loops (Week 7, Days 3-4)
- Ensure Media_Widget selections don't trigger infinite loops
- Verify SelectionSource filtering prevents self-response
- Add logging to trace selection flow
- Document expected behavior in comments

### Step 5: Write Integration Tests (Week 7, Day 5)
- Test: DataManager selects data → Media_Widget highlights
- Test: Media_Widget doesn't highlight own selections
- Test: Multiple Media_Widget instances coordinate via SelectionContext
- Test: Selection source correctly identifies originating widget

**Next Milestone**: Cross-widget selection working - selecting in DataManager_Widget automatically highlights in Media_Widget without circular loops.

## References

- [Blender Architecture](https://wiki.blender.org/wiki/Source/Architecture)
- [Qt Model/View Architecture](https://doc.qt.io/qt-6/model-view-programming.html)
- [Command Pattern](https://refactoring.guru/design-patterns/command)
- [reflect-cpp Documentation](https://github.com/getml/reflect-cpp)
- [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System)
