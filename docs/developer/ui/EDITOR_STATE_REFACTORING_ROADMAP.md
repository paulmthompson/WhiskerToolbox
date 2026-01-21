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

The refactoring introduces two core abstractions:
- **EditorState**: Base class for serializable widget state
- **EditorRegistry**: Central registry for editor types, instances, and selection (includes SelectionContext)

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
- ✅ [EditorRegistry.hpp](../../WhiskerToolbox/EditorState/EditorRegistry.hpp) - Central registry for types, instances, and selection
- ✅ [EditorRegistry.test.cpp](../../WhiskerToolbox/EditorState/EditorRegistry.test.cpp) - Comprehensive unit tests (50+ passing)

**Test Results**: All tests passing
- EditorState: Instance IDs, dirty state tracking, JSON serialization
- SelectionContext: Data/entity selection, active editor tracking, properties context routing
- EditorRegistry: Type registration, state registry, factory system, workspace serialization
- Signal/slot behavior validated across all components

**Key Design Points**:
- EditorState provides base functionality for all widget states
- SelectionContext enables widgets to communicate without direct dependencies
- EditorRegistry consolidates type registration, state registry, and factory functions into a single cohesive class
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
├── EditorState.hpp              # Base class for all states
├── EditorState.cpp
├── EditorRegistry.hpp           # Central registry (types + instances + selection)
├── EditorRegistry.cpp
├── SelectionContext.hpp         # Inter-widget communication
├── SelectionContext.cpp
├── EditorRegistry.test.cpp      # Unit tests for registry
├── EditorState.test.cpp         # Unit tests for state
└── EditorState.integration.test.cpp  # Cross-widget integration tests

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
- [x] Register state with EditorRegistry when widget is created
- [x] Unregister state when widget is destroyed
- [x] Widgets keep all existing member variables - state is additional

### 2.2 Integrate EditorRegistry into MainWindow (Week 4) ✅ COMPLETE

**Deliverables**:
- [x] Add EditorRegistry member to MainWindow
- [x] Initialize in constructor after DataManager
- [x] Provide accessor for child widgets (`editorRegistry()` method)
- [x] Update widget creation to pass EditorRegistry pointer

### 2.3 Connect DataManager_Widget Selection (Week 5) ✅ COMPLETE

Establish first communication path: DataManager_Widget → SelectionContext

**Deliverables**:
- [x] Update DataManager_Widget to create and register state
- [x] Connect Feature_Table_Widget signals to state
- [x] Connect state signals to SelectionContext
- [x] Test: Selecting in feature table should update SelectionContext
- [x] Integration tests verify signal propagation and SelectionSource identification

**Implementation Details**:
- Added `_state` (shared_ptr<DataManagerWidgetState>) member to DataManager_Widget
- Added `_selection_context` pointer to access EditorRegistry's SelectionContext
- Included necessary headers: `DataManagerWidgetState.hpp`, `EditorState/SelectionContext.hpp`, `EditorState/EditorRegistry.hpp`
- Connected Feature_Table_Widget::featureSelected → state->setSelectedDataKey()
- Connected DataManagerWidgetState::selectedDataKeyChanged → SelectionContext->setSelectedData()
- State unregistered from EditorRegistry in destructor

**Test Coverage**:
- Added 5 comprehensive integration tests in DataManagerWidgetState.test.cpp
- Tests verify: state signal propagation, SelectionContext updates, multiple state coordination, SelectionSource identification
- All tests passing

**Files Modified**:
- [DataManager_Widget.hpp](../../WhiskerToolbox/DataManager_Widget/DataManager_Widget.hpp) - Added state and context members, forward declarations
- [DataManager_Widget.cpp](../../WhiskerToolbox/DataManager_Widget/DataManager_Widget.cpp) - Implemented state initialization, signal connections, registration/unregistration
- [DataManagerWidgetState.test.cpp](../../WhiskerToolbox/DataManager_Widget/DataManagerWidgetState.test.cpp) - Added integration test suite

### 2.4 Connect Media_Widget as Listener (Week 6) ✅ COMPLETE

Media_Widget now responds to SelectionContext changes:

**Implementation Details**:
- Added `_state` (shared_ptr<MediaWidgetState>) member to Media_Widget
- Added `_selection_context` pointer to access EditorRegistry's SelectionContext
- Included necessary headers: `EditorState/SelectionContext.hpp`, `EditorState/EditorRegistry.hpp`, `MediaWidgetState.hpp`
- Implemented constructor initialization: state creation, EditorRegistry registration, SelectionContext connection
- Connected Feature_Table_Widget::featureSelected → state->setDisplayedDataKey() → SelectionContext::setSelectedData()
- Implemented `_onExternalSelectionChanged(SelectionSource const& source)` slot that:
  - Filters out own instance ID to prevent self-response
  - Gets selected data key from SelectionContext::primarySelectedData()
  - Updates MediaWidgetState with externally selected data
  - Intentionally decoupled from Feature_Table_Widget (will be removed in Phase 3)
- State unregistered from EditorRegistry in destructor

**Test Coverage**:
- Created `EditorState.integration.test.cpp` with comprehensive cross-widget tests
- Tests verify:
  - MediaWidgetState registration with EditorRegistry
  - DataManagerWidgetState registration
  - Cross-widget selection propagation (DataManager → SelectionContext → Media_Widget)
  - Circular update prevention via SelectionSource filtering
  - Multiple Media_Widget instances coordinating via SelectionContext
  - SelectionSource correctly identifying originating widget
  - Full signal chain: DM selection → state → context → external listener
  - Workspace serialization with mixed state types
- All 8+ integration tests passing
- Moved integration tests to EditorState to avoid cross-widget dependencies in unit tests

**Files Created**:
- [EditorState.integration.test.cpp](../../WhiskerToolbox/EditorState/EditorState.integration.test.cpp) - Cross-widget integration tests
- Updated [tests/WhiskerToolbox/EditorState/CMakeLists.txt](../../../tests/WhiskerToolbox/EditorState/CMakeLists.txt) - Added test_editor_state_integration executable

**Files Modified**:
- [Media_Widget.hpp](../../WhiskerToolbox/Media_Widget/Media_Widget.hpp) - Added state and context members, slot declaration, included SelectionContext header
- [Media_Widget.cpp](../../WhiskerToolbox/Media_Widget/Media_Widget.cpp) - Implemented state initialization, signal connections, registration/unregistration, external selection handler
- [MediaWidgetState.test.cpp](../../WhiskerToolbox/Media_Widget/MediaWidgetState.test.cpp) - Kept unit tests only (moved integration tests)

**Deliverables**:
- [x] Update Media_Widget to create and register state
- [x] Connect to SelectionContext::selectionChanged
- [x] Implement response to external selection changes
- [x] Integration tests verify cross-widget communication
- [x] Verify no circular update loops

### 2.5 Validate Communication (Week 7) 🔄 IN PROGRESS

**Test the communication chain**:
1. Open DataManager_Widget and Media_Widget
2. Select a feature in DataManager's feature table
3. Verify SelectionContext receives the selection
4. Verify Media_Widget's state responds to the selection
5. Verify Media_Widget doesn't respond to its own selections (no loops)

**Deliverables**:
- [ ] Manual integration testing with UI
- [ ] Add logging to trace selection flow
- [ ] Verify no circular update loops
- [ ] Document edge cases discovered

### 2.6 Gradually Migrate State (Weeks 8-9) ✅ COMPLETE (Media_Widget)

**Status**: Media_Widget has completed comprehensive state migration via hybrid architecture (see Phase 2.6.1 below).

**Original Plan**: Incrementally move state piece by piece

**What Actually Happened**: Media_Widget underwent a complete "hybrid architecture" refactoring that went beyond the original scope, creating a cleaner and more maintainable design:

#### Phase 2.6.1: Media_Widget Hybrid Architecture Refactoring ✅ COMPLETE

**See**: [MEDIA_WIDGET_HYBRID_STATE_PLAN.md](MEDIA_WIDGET_HYBRID_STATE_PLAN.md) for complete documentation

**Goal**: Refactor MediaWidgetState from ~60 methods to ~25 methods using a generic DisplayOptionsRegistry

**Phases Completed**:

1. **Phase 4A - DisplayOptionsRegistry** ✅
   - Created generic, type-safe registry for all 6 display option types
   - Replaced 6 sets of typed methods with single generic API
   - Template specializations ensure type safety
   - Comprehensive unit tests (set/get/remove/keys/enabledKeys)

2. **Phase 4B - Signal Consolidation** ✅  
   - Reduced ~20 signals → 8 signals
   - Consolidated fine-grained signals into 3 category signals:
     - `interactionPrefsChanged(category)` - for line/mask/point preferences
     - `textOverlaysChanged()` - for all overlay operations
     - `toolModesChanged(category)` - for line/mask/point tool modes
   - Display option signals from registry:
     - `optionsChanged(key, type)`
     - `optionsRemoved(key, type)` 
     - `visibilityChanged(key, type, visible)`

3. **Phase 4C - Media_Window Integration** ✅
   - Removed duplicate `_*_configs` maps from Media_Window
   - All `add*ToScene` and `_plot*Data` methods read from state
   - Accessor methods forward to state's DisplayOptionsRegistry
   - Single source of truth for display options

4. **Phase 4D - Sub-Widget Integration** ✅
   - All 6 sub-widgets wired to state:
     - MediaLine_Widget - reads/writes line options via state
     - MediaMask_Widget - reads/writes mask options via state  
     - MediaPoint_Widget - reads/writes point options via state
     - MediaTensor_Widget - reads/writes tensor options via state
     - MediaInterval_Widget - reads/writes interval options via state
     - MediaProcessing_Widget - reads/writes media options via state
   - Each widget reads options on show/setActiveKey
   - UI changes write back to state

5. **Phase 4E - Viewport Integration** ✅
   - Removed duplicate `_current_zoom` and `_user_zoom_active` from Media_Widget
   - State is single source of truth for zoom/pan values
   - Added `_isUserZoomActive()` helper deriving from state
   - Removed intermediate sync methods (`_syncZoomToState`, `_syncPanToState`)
   - Direct read/write to state in zoom/pan operations
   - Simplified signal handlers (read from QGraphicsView transform)

**State Coverage**: MediaWidgetState now manages:
- ✅ Display options for 6 data types (line, mask, point, tensor, interval, media)
- ✅ Viewport state (zoom, pan, canvas size)
- ✅ Interaction preferences (line tools, mask brush, point selection)
- ✅ Text overlays (labels, annotations)
- ✅ Active tool modes (line/mask/point editing modes)
- ✅ Full workspace serialization support

**Files Created**:
- `DisplayOptionsRegistry.hpp/.cpp/.test.cpp` - Generic registry implementation

**Files Modified**:
- `MediaWidgetState.hpp/.cpp` - Added DisplayOptionsRegistry, consolidated signals
- `MediaWidgetStateData.hpp` - Complete state structure with all options
- `Media_Window.hpp/.cpp` - Removed duplicate storage, use state
- `Media_Widget.hpp/.cpp` - Removed duplicate zoom/pan members, state as source of truth
- All 6 sub-widgets - Read/write via state

**Test Results**: All tests passing
- DisplayOptionsRegistry unit tests (50+ test cases)
- MediaWidgetState serialization tests
- Integration tests verify state synchronization
- Workspace save/restore validated

**Success Criteria Achieved**:
1. ✅ Method reduction: ~60 methods → ~25 methods (58% reduction)
2. ✅ Signal reduction: ~20 signals → 8 signals (60% reduction)  
3. ✅ Single source of truth: All state in MediaWidgetStateData only
4. ✅ No duplicate storage: Media_Window and Media_Widget don't cache state
5. ✅ All tests passing: Unit and integration tests validated
6. ✅ Serialization works: Full workspace persistence with viewport state

**Significance**: Media_Widget serves as the **reference implementation** for comprehensive state migration. Other widgets can follow this pattern:
- Generic registry approach for similar option types
- Signal consolidation strategies
- Single source of truth enforcement
- Incremental sub-component integration

**Deliverables (Original - Now Complete for Media_Widget)**:
- [x] Move viewport state (zoom/pan) to MediaWidgetState
- [x] Update Media_Widget to use state for viewport (no duplicate members)
- [x] Move feature enable/color/visibility state to MediaWidgetState  
- [x] Update Media_Widget to use state for features (via DisplayOptionsRegistry)
- [x] Feature_Table_Widget remains but reads/writes state
- [x] Test serialization of migrated state (comprehensive coverage)
- [x] Move interaction preferences to state
- [x] Move text overlays to state
- [x] Move tool modes to state
- [x] Generic display options registry
- [x] Signal consolidation

**Next Widget**: DataViewer_Widget should follow similar hybrid architecture pattern

### 2.7 DataTransform_Widget: First Feature_Table_Widget Removal (Week 9) ✅ COMPLETE

**Goal**: Make DataTransform_Widget the first widget to fully remove its embedded Feature_Table_Widget and rely entirely on external selection via SelectionContext.

**Rationale**: DataTransform_Widget has a simple Feature_Table_Widget (no filters, straightforward selection model), making it the ideal candidate for proving the pattern works before migrating more complex widgets.

**Implementation Steps**:

#### Step 1: Create DataTransformWidgetState (Days 1-2) ✅
```cpp
struct DataTransformWidgetStateData {
    std::string selected_input_data_key;  // Input data for transform
    std::string transform_type;           // Currently selected transform
    rfl::json::Value transform_config;    // Transform-specific config
};

class DataTransformWidgetState : public EditorState {
    Q_OBJECT
public:
    // ... similar pattern to MediaWidgetState/DataManagerWidgetState
    
signals:
    void selectedInputDataChanged(QString const& key);
    void transformTypeChanged(QString const& type);
    void transformConfigChanged();
};
```

#### Step 2: Integrate State and Connect as Listener (Days 3-4) ✅
- Add `_state` and `_selection_context` members to DataTransform_Widget
- Register state with EditorRegistry
- Connect to SelectionContext::selectionChanged
- Implement response to external selection changes
- Update internal logic to read from state instead of embedded feature table

#### Step 3: Create Integration Tests (Day 5) ✅
- Test state registration with EditorRegistry
- Test cross-widget selection propagation (DataManager → DataTransform)
- Test circular update prevention
- Test transform execution with externally selected data
- Verify state serialization with transform configuration

#### Step 4: Remove Embedded Feature_Table_Widget (Days 6-7) ✅
- Remove Feature_Table_Widget instantiation from DataTransform_Widget
- Update UI layout to remove feature table zone
- Replace with simple label showing selected data from SelectionContext
- Rely entirely on SelectionContext for input data selection
- Update documentation to note external selection requirement
- Verify all transform workflows still function correctly

**Deliverables**:
- [x] DataTransformWidgetState class with transform configuration
- [x] State integration in DataTransform_Widget constructor
- [x] SelectionContext listener implementation
- [x] Integration tests verifying cross-widget communication
- [x] Feature_Table_Widget removed from DataTransform_Widget
- [x] Documentation updated for external selection pattern

**Files Created**:
- [DataTransformWidgetState.hpp](../../WhiskerToolbox/DataTransform_Widget/DataTransformWidgetState.hpp) - State class for transform widget
- [DataTransformWidgetState.cpp](../../WhiskerToolbox/DataTransform_Widget/DataTransformWidgetState.cpp) - State implementation

**Files Modified**:
- [DataTransform_Widget.hpp](../../WhiskerToolbox/DataTransform_Widget/DataTransform_Widget.hpp) - Added state members, EditorRegistry parameter
- [DataTransform_Widget.cpp](../../WhiskerToolbox/DataTransform_Widget/DataTransform_Widget.cpp) - Implemented state integration, external selection handler
- [DataTransform_Widget.ui](../../WhiskerToolbox/DataTransform_Widget/DataTransform_Widget.ui) - Removed Feature_Table_Widget, added input selection labels
- [DataTransform_Widget/CMakeLists.txt](../../WhiskerToolbox/DataTransform_Widget/CMakeLists.txt) - Added state files, EditorState dependency, removed Feature_Table_Widget dependency
- [mainwindow.cpp](../../WhiskerToolbox/Main_Window/mainwindow.cpp) - Pass EditorRegistry to DataTransform_Widget constructor

**Significance**: This phase proves that widgets can successfully rely on external selection via SelectionContext without embedded feature tables. This validates the architecture for Phase 3's Central Properties Zone.

### 2.8 Summary of Phase 2 Approach

**Key principles**:
1. ✅ State classes start minimal - just enough for communication
2. ✅ Widgets hold state alongside existing members - not replacing them
3. ✅ Establish communication first, validate it works
4. ✅ Migrate state piece by piece, not all at once
5. ✅ Keep existing UI elements - they become views of state
6. ✅ Prove external selection pattern works (DataTransform_Widget) ✅ **VALIDATED**

## Phase 3: Central Properties Zone (Weeks 9-11)

**Prerequisites**: Phase 2 complete - widgets have state objects and communicate via SelectionContext.

**Goal**: Create a unified properties panel that replaces per-widget Feature_Table_Widget instances.

### 3.0 Widget Migration Tracking Table

This table tracks all widgets managed by MainWindow and their migration progress toward the EditorState architecture.

#### Primary Widgets (Dock-based, Core Functionality)

| Widget | EditorState Class | SelectionContext | Factory | Notes |
|--------|:------------------:|:----------------:|:-------:|-------|
| **Media_Widget** | ✅ `MediaWidgetState` | ✅ | ❌ | Reference implementation - full hybrid architecture |
| **DataManager_Widget** | ✅ `DataManagerWidgetState` | ✅ | ❌ | Basic state, selection propagation working |
| **DataTransform_Widget** | ✅ `DataTransformWidgetState` | ✅ | ❌ | First Feature_Table_Widget removal - proved external selection |
| **DataViewer_Widget** | ❌ | ❌ | ❌ | Next candidate for hybrid architecture migration |
| **Analysis_Dashboard** | ❌ | ❌ | ❌ | Multiple sub-plot widgets, complex coordination |
| **GroupManagementWidget** | ❌ | ❌ | ❌ | Interacts with EntityGroupManager |
| **TableDesignerWidget** | ❌ | ❌ | ❌ | Complex table design state |
| **TimeScrollBar** | ❌ | ❌ | ❌ | Global time navigation, may not need full state |
| **Terminal_Widget** | ❌ | ❌ | ❌ | Output widget, minimal state needed |

#### Secondary Widgets (Feature-specific)

| Widget | EditorState Class | SelectionContext | Factory | Notes |
|--------|:------------------:|:----------------:|:-------:|-------|
| **Whisker_Widget** | ❌ | ❌ | ❌ | Whisker tracking controls |
| **Tongue_Widget** | ❌ | ❌ | ❌ | Tongue tracking controls |
| **ML_Widget** | ❌ | ❌ | ❌ | Machine learning inference |
| **BatchProcessing_Widget** | ❌ | ❌ | ❌ | Batch pipeline execution |
| **Export_Video_Widget** | ❌ | ❌ | ❌ | Video export configuration |
| **Test_Widget** | ✅ `TestWidgetState` | ✅ | ✅ | **Proof-of-concept complete** - View/Properties split validated, EditorRegistry integration complete |

#### Loader Widgets (Modal/Semi-modal Data Import)

| Widget | EditorState Class | SelectionContext | Factory | Notes |
|--------|:------------------:|:----------------:|:-------:|-------|
| **Point_Loader_Widget** | ❌ | ❌ | ❌ | Point data import |
| **Mask_Loader_Widget** | ❌ | ❌ | ❌ | Mask data import |
| **Line_Loader_Widget** | ❌ | ❌ | ❌ | Line data import |
| **Digital_Interval_Loader_Widget** | ❌ | ❌ | ❌ | Interval data import |
| **Digital_Event_Loader_Widget** | ❌ | ❌ | ❌ | Event data import |
| **Tensor_Loader_Widget** | ❌ | ❌ | ❌ | Tensor data import |

#### Legend
- ✅ Implemented and tested
- 🔄 In progress
- ❌ Not yet implemented

#### Migration Statistics
- **Total Primary Widgets**: 9
- **With EditorState**: 3 (33%)
- **With SelectionContext**: 3 (33%)
- **With Factory**: 0 (0%)
- **Total Secondary Widgets**: 6
- **Secondary with EditorState**: 1 (17%) - Test_Widget proof-of-concept
- **View/Properties Split Validated**: ✅ Test_Widget

#### Migration Priority for Phase 3

1. **High Priority (Phase 3 prerequisites)**:
   - DataViewer_Widget - Similar complexity to Media_Widget, good test of hybrid pattern
   - Test_Widget - Ideal for PropertiesHost proof-of-concept (simple, isolated)

2. **Medium Priority (Phase 5)**:
   - Analysis_Dashboard - Benefits from state serialization for plot layouts
   - GroupManagementWidget - Selection coordination with other widgets
   - TableDesignerWidget - Complex state benefits from separation

3. **Lower Priority (Phase 6+)**:
   - Secondary widgets (Whisker, Tongue, ML, Batch, Export)
   - Loader widgets (may only need minimal state for "recent" configurations)
   - TimeScrollBar, Terminal_Widget (utility widgets with minimal state)

#### Notes on Loader Widgets

Loader widgets have a different lifecycle than primary widgets:
- Often modal or transient (open, configure, load, close)
- State needed primarily for "remember last configuration"
- May not need full EditorState integration
- Consider: `LoaderConfiguration` base class with simpler serialization

**Decision (2026-01-21)**: Loader widgets remain as-is for now (lower priority).
- Focus on primary widgets first
- Loaders can emit signal on successful load
- DataManager_Widget or SelectionContext can listen and auto-select newly loaded data
- Revisit loader state management in Phase 6+ if needed

#### Design Decisions Record

| Decision | Date | Choice | Rationale |
|----------|------|--------|-----------|
| Loader widget priority | 2026-01-21 | Defer (Phase 6+) | Focus on primary widgets, loaders are transient |
| EditorRegistry adoption | 2026-01-21 | Incremental (new/refactored only) | Prove pattern first, special needs will reduce with refactoring |
| State ownership | 2026-01-21 | shared_ptr in both EditorRegistry and widgets | Registry needs ownership for serialization/undo; widgets need direct access |
| UI zones layout | 2026-01-21 | Left: Feature_Table + Groups; Right: Properties | Clear separation of selection (left) vs configuration (right) |

### 3.1 PropertiesHost Widget (Week 9)

**Goal**: Create a context-sensitive properties container that can display appropriate property panels based on the active editor and selected data.

**Core Concept**: Instead of each widget having its own embedded properties/Feature_Table_Widget, we create a single PropertiesHost that:
1. Observes SelectionContext for active editor and selected data
2. Dynamically swaps the appropriate properties panel
3. Allows widgets to be "split" into view + properties components sharing the same state

#### 3.1.1 PropertiesHost Design

```cpp
// src/WhiskerToolbox/EditorState/PropertiesHost.hpp

/**
 * @brief Context-sensitive properties container
 * 
 * PropertiesHost observes SelectionContext and shows the appropriate
 * properties panel based on:
 * 1. Active editor type (e.g., MediaWidget properties when Media_Widget is focused)
 * 2. Selected data type (e.g., LineData properties when a line is selected)
 * 3. Last interaction context (prioritizes most recent meaningful interaction)
 * 
 * The PropertiesHost uses a QStackedWidget internally to efficiently swap
 * between property panels without recreating them.
 * 
 * ## Usage Pattern
 * 
 * 1. MainWindow creates PropertiesHost and places it in the Properties zone
 * 2. Each widget type registers its properties factory
 * 3. When user interacts with a widget, SelectionContext updates
 * 4. PropertiesHost responds by showing the appropriate panel
 * 
 * ## View/Properties Split
 * 
 * Widgets that want to support the split pattern must:
 * - Derive their EditorState from EditorState
 * - Implement a "View" component that displays data
 * - Implement a "Properties" component that edits state
 * - Both components reference the SAME state instance
 * 
 * Example: Test_Widget could be split into:
 * - TestWidgetView: Shows visualization, receives state
 * - TestWidgetProperties: Shows controls, modifies state
 * - TestWidgetState: Shared between both, owned by EditorRegistry
 */
class PropertiesHost : public QWidget {
    Q_OBJECT

public:
    explicit PropertiesHost(EditorRegistry* editor_registry,
                            QWidget* parent = nullptr);
    ~PropertiesHost() override;

    // ========== Factory Registration ==========

    /**
     * @brief Register a properties widget factory for an editor type
     * 
     * When the active editor is of this type, the factory is called
     * to create (or reuse) the properties widget for that editor.
     * 
     * @param editor_type The EditorState::typeId() this factory handles
     * @param factory Function that creates properties widget given the state
     */
    void registerEditorPropertiesFactory(
        QString const& editor_type,
        std::function<QWidget*(std::shared_ptr<EditorState>)> factory);

    /**
     * @brief Register a properties widget factory for a data type
     * 
     * When data of this type is selected, the factory creates a
     * properties widget for inspecting/editing that data.
     * 
     * @param data_type DM_DataType string representation
     * @param factory Function that creates properties widget given data key
     */
    void registerDataPropertiesFactory(
        QString const& data_type,
        std::function<QWidget*(QString const& data_key, DataManager*)> factory);

    // ========== Mode Control ==========

    enum class DisplayMode {
        EditorProperties,    // Show active editor's properties
        DataProperties,      // Show selected data's properties
        Auto                 // Automatically choose based on context
    };

    void setDisplayMode(DisplayMode mode);
    [[nodiscard]] DisplayMode displayMode() const;

private slots:
    void onActiveEditorChanged(QString const& editor_id);
    void onSelectionChanged(SelectionSource const& source);
    void onPropertiesContextChanged(QString const& context_id);

private:
    EditorRegistry* _editor_registry;
    QStackedWidget* _stack;
    QLabel* _empty_label;  // Shown when no context available
    
    DisplayMode _mode = DisplayMode::Auto;
    
    // Cache of created property widgets (avoid recreation)
    // Key: editor instance ID or "data:" + data_key
    std::map<QString, QWidget*> _cached_widgets;
    
    // Factories
    std::map<QString, std::function<QWidget*(std::shared_ptr<EditorState>)>> _editor_factories;
    std::map<QString, std::function<QWidget*(QString, DataManager*)>> _data_factories;

    void updateDisplay();
    QWidget* getOrCreateEditorProperties(std::shared_ptr<EditorState> state);
    QWidget* getOrCreateDataProperties(QString const& data_key, QString const& data_type);
};
```

#### 3.1.2 View/Properties Split Pattern

The key architectural pattern is that a "widget" becomes two components sharing state:

```
┌─────────────────────────────────────────────────────────────┐
│                     EditorState                             │
│  (e.g., TestWidgetState - holds all serializable state)    │
└─────────────────────────────────────────────────────────────┘
         │                                    │
         │ shared_ptr                         │ shared_ptr
         ▼                                    ▼
┌─────────────────────┐          ┌─────────────────────────────┐
│   View Component    │          │   Properties Component      │
│  (TestWidgetView)   │          │  (TestWidgetProperties)     │
│                     │          │                             │
│  - Displays data    │          │  - Controls/settings        │
│  - User interaction │          │  - Feature enable/disable   │
│  - Canvas/viewport  │          │  - Color pickers            │
│                     │          │  - Configuration            │
└─────────────────────┘          └─────────────────────────────┘
         │                                    │
         │ reads state                        │ writes state
         │ emits user actions                 │ (through setters)
         │                                    │
         ▼                                    ▼
┌─────────────────────────────────────────────────────────────┐
│                 State signals notify both                   │
│           (e.g., zoomChanged, featureEnabledChanged)       │
└─────────────────────────────────────────────────────────────┘
```

**State Ownership Model**:

Both `EditorRegistry` and widgets hold `shared_ptr<EditorState>`. This is intentional:

```
┌─────────────────────────────────────────────────────────────┐
│                    EditorRegistry                           │
│  (holds shared_ptr for each registered state)              │
│                                                             │
│  Responsibilities:                                          │
│  - Registry of all active states                           │
│  - Workspace serialization (save/restore)                  │
│  - State restoration after undo/redo                       │
│  - Can send restored state to widgets                      │
└─────────────────────────────────────────────────────────────┘
         │
         │ shared_ptr (EditorRegistry owns for persistence)
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│                     EditorState                             │
│  (e.g., TestWidgetState, MediaWidgetState)                 │
└─────────────────────────────────────────────────────────────┘
         ▲
         │ shared_ptr (widgets also hold for direct access)
         │
┌────────┴────────┐
│   View/Props    │
│    Widgets      │
└─────────────────┘
```

**Why shared_ptr for both?**
1. **EditorRegistry needs ownership** for serialization, command pattern, undo/redo
2. **Widgets need direct access** without going through EditorRegistry for every read/write
3. **State outlives widgets** - when a dock is closed and reopened, state persists
4. **Restoration flow**: EditorRegistry can restore state → widgets automatically see changes via signals

**Implementation Notes**:

✅ See actual implementation in:
- `src/WhiskerToolbox/Test_Widget/TestWidgetState.hpp/.cpp`
- `src/WhiskerToolbox/Test_Widget/TestWidgetView.hpp/.cpp`
- `src/WhiskerToolbox/Test_Widget/TestWidgetProperties.hpp/.cpp`

#### 3.1.3 Test_Widget Proof-of-Concept ✅ COMPLETE

**Status**: Implementation complete and validated. All tests passing.

**Rationale**: Test_Widget is the ideal candidate for proving the View/Properties split because:
1. It's simple and isolated (no complex data dependencies)
2. It's already designed for testing new features
3. Changes won't affect production workflows
4. We can iterate quickly on the pattern

**Implemented Features** (all 7 properties demonstrated):
- Booleans: `show_grid`, `show_crosshair`, `enable_animation`
- Color: `highlight_color` with picker
- Numeric: `zoom_level` (slider), `grid_spacing` (spinbox)
- Text: `label_text` (line edit)

### 3.1.4 EditorRegistry and Widget Creation

✅ **IMPLEMENTED** - See [EditorRegistry.hpp](../../WhiskerToolbox/EditorState/EditorRegistry.hpp)

EditorRegistry consolidates what was originally planned as separate WorkspaceManager and EditorFactory classes into a single cohesive class. Factory functions are stored as part of type metadata.

#### 3.1.5 Registration Example

✅ **IMPLEMENTED** - See [mainwindow.cpp](../../WhiskerToolbox/Main_Window/mainwindow.cpp) `_registerEditorTypes()`

When the application starts, MainWindow registers editor types with EditorRegistry. Factory functions are embedded in the type definition:

#### 3.1.6 MainWindow Simplification

✅ **IMPLEMENTED** - See [mainwindow.cpp](../../WhiskerToolbox/Main_Window/mainwindow.cpp) `openEditor()`

With EditorRegistry, MainWindow's widget creation becomes uniform:

### 3.1.7 Phase 3.1 Deliverables

| Deliverable | Priority | Status |
|-------------|----------|--------|
| `PropertiesHost.hpp/.cpp` | High | 📋 (deferred to Phase 3.2) |
| `EditorRegistry.hpp/.cpp` | High | ✅ |
| `TestWidgetState.hpp/.cpp` | High | ✅ |
| `TestWidgetStateData.hpp` | High | ✅ |
| `TestWidgetView.hpp/.cpp` | High | ✅ |
| `TestWidgetProperties.hpp/.cpp` | High | ✅ |
| Register Test_Widget with EditorRegistry | High | ✅ |
| Integrate PropertiesHost into MainWindow layout | High | 📋 (deferred to Phase 3.2) |
| Unit tests for PropertiesHost | Medium | 📋 (deferred to Phase 3.2) |
| Unit tests for EditorRegistry | Medium | ✅ |
| Integration test: TestWidget view/properties split | High | ✅ |
| Documentation update | Medium | ✅ |

**Success Criteria**:
1. ✅ Test_Widget can be opened via EditorRegistry - **COMPLETE**
2. ✅ TestWidgetView appears in dock widget
3. ✅ TestWidgetProperties shown side-by-side with view (via QSplitter)
4. ✅ State changes in properties reflect in view (and vice versa) - **VALIDATED**
5. ✅ State serializes/deserializes correctly - **VALIDATED**
6. ✅ Pattern documented for other widgets to follow

**Implementation Summary** (2026-01-21):

**Files Created**:
- `TestWidgetStateData.hpp` - Serializable state structure (7 properties)
- `TestWidgetState.hpp/.cpp` - EditorState subclass with full signal support
- `TestWidgetView.hpp/.cpp` - QGraphicsView-based visualization component
- `TestWidgetProperties.hpp/.cpp` - Properties panel with UI controls
- `EditorRegistry.hpp/.cpp` - Consolidated type/instance/selection registry
- `EditorRegistry.test.cpp` - Comprehensive unit tests for registry

**Integration**:
- EditorRegistry registered with Test_Widget factories in MainWindow
- MainWindow::openTestWidget() delegates to openEditor("TestWidget")
- Registry creates state, view, and properties in single call
- Automatic state registration with EditorRegistry
- Single-instance enforcement for Test_Widget
- Demonstrated features: grid, crosshair, animation, color picker, zoom slider, text edit

**Key Learnings**:
1. ✅ View/Properties split pattern works as designed
2. ✅ Shared state via shared_ptr is effective
3. ✅ Signal-based synchronization prevents update loops with `_updating_from_state` flag
4. ✅ reflect-cpp serialization works seamlessly
5. ✅ EditorRegistry simplifies widget creation significantly
6. ✅ Pattern is proven and ready for other widgets
7. 📋 PropertiesHost deferred to Phase 3.2 (current QSplitter approach works for proof-of-concept)


### 3.2 Standard UI Zones

Define standard zones in MainWindow layout:

```
┌──────────────────────────────────────────────────────────────────┐
│  Menu Bar                                                        │
├────────────────┬─────────────────────────────┬───────────────────┤
│                │                             │                   │
│   Outliner     │     Main Editor Area        │   Properties      │
│   (Left)       │     (Center)                │   (Right)         │
│                │                             │                   │
│   - Feature    │     Media_Widget            │   - Editor-       │
│     Table      │     DataViewer_Widget       │     specific      │
│     (from DM)  │     Analysis plots          │     properties    │
│                │     Test_Widget view        │   - Data-type     │
│   - Group      │     etc.                    │     properties    │
│     Manager    │                             │   - Context-      │
│                │                             │     sensitive     │
│                │                             │     controls      │
│                │                             │                   │
├────────────────┴─────────────────────────────┴───────────────────┤
│  Timeline / DataViewer Strip (optional)                          │
├──────────────────────────────────────────────────────────────────┤
│  Status Bar / Terminal                                           │
└──────────────────────────────────────────────────────────────────┘
```

**Zone Responsibilities**:

| Zone | Contents | Purpose |
|------|----------|--------|
| **Left (Outliner)** | Feature_Table_Widget, GroupManagementWidget | Data selection, entity management, global navigation |
| **Center (Main Editor)** | Media_Widget, DataViewer_Widget, plots, views | Primary visualization and interaction |
| **Right (Properties)** | PropertiesHost with context-sensitive panels | Editor-specific settings, data-type properties |
| **Bottom (Timeline)** | TimeScrollBar, optional DataViewer strip | Time navigation, temporal context |
| **Bottom (Output)** | Terminal_Widget | Logs, command output |

**Key Design Decision**: Feature_Table_Widget (currently in DataManager_Widget) and GroupManagementWidget move to the **left** panel as the primary data selection interface. The **right** panel becomes the unified PropertiesHost that shows context-sensitive properties based on:
1. Which editor is active (e.g., MediaWidgetProperties when Media_Widget is focused)
2. What data type is selected (e.g., LineDataProperties when a line key is selected)
3. Last meaningful interaction context

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

1. **Media_Widget** (Phase 2.4 + 2.6.1) - Foundation widget with comprehensive state migration ✅ **COMPLETE**
   - State integration: ✅ Complete (Phase 2.4)
   - Hybrid architecture refactoring: ✅ Complete (Phase 2.6.1) 
   - DisplayOptionsRegistry for 6 data types
   - Full viewport/preferences/overlays/modes in state
   - Reference implementation for other widgets
   
2. **DataTransform_Widget** (Phase 2.7) - First Feature_Table_Widget removal ✅ **COMPLETE**
   - Proven external selection via SelectionContext works
   - Validated architecture for Phase 3's Central Properties Zone
   
3. **DataViewer_Widget** - Similar pattern to Media_Widget
   - Follow Media_Widget's hybrid architecture pattern
   - Generic registry for analog/event/interval configs
   - Signal consolidation opportunities
   
4. **DataManager_Widget** - Extract remaining state, simplify to data browser
   - Already has basic state (Phase 2.3)
   - Potential for registry pattern with data type filters
   
5. **Analysis_Dashboard** - Already has some patterns, refine
   - Multiple plot widgets, coordinate via state
   - May benefit from plot configuration registry
   
6. **TableDesignerWidget** - Complex, benefits most from state separation
   - Table design state separate from execution
   - Row/column selector state management

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

### 5.3 Widget Factory Pattern

Widget creation is now handled by EditorRegistry. See [Section 3.1.4](#314-editorregistry-and-widget-creation) for the actual implementation.

**Key API Summary:**
```cpp
// Register an editor type with embedded factories
registry.registerType({
    .type_id = "DataViewer",
    .display_name = "Data Viewer",
    .default_zone = "main",
    .allow_multiple = true,
    .create_state = []() { return std::make_shared<DataViewerState>(); },
    .create_view = [](auto s) { return new DataViewerView(s); },
    .create_properties = nullptr  // optional
});

// Create editor (state is auto-registered)
auto [state, view, props] = registry.createEditor("DataViewer");
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
    EditorRegistry registry(dm);
    
    // Register types first
    registry.registerType({
        .type_id = "MediaWidget",
        .create_state = []() { return std::make_shared<MediaWidgetState>(); },
        // ... other factories
    });
    
    // Create multiple editors
    auto media_state = registry.createState("MediaWidget");
    media_state->setDisplayName("Media 1");
    registry.registerState(media_state);
    
    auto viewer_state = std::make_shared<DataViewerState>();
    viewer_state->setDisplayName("Viewer 1");
    registry.registerState(viewer_state);
    
    // Serialize
    auto json = registry.toJson();
    
    // Restore to new registry
    EditorRegistry restored(dm);
    // Register types before restoring
    restored.registerType({/* same types */});
    REQUIRE(restored.fromJson(json));
    REQUIRE(restored.allStates().size() == 2);
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
- [ ] Register with EditorRegistry
- [ ] Add unit tests for state serialization
- [ ] Add integration tests with EditorRegistry
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
| 1. Core Infrastructure | ✅ COMPLETE | 3 weeks | EditorState, EditorRegistry, SelectionContext |
| 2. State Integration & Communication | ✅ COMPLETE | 8 weeks | Minimal states, inter-widget communication, **comprehensive Media_Widget migration**, first Feature_Table removal |
| 2.6.1 Media_Widget Hybrid Architecture | ✅ COMPLETE | 2 weeks | DisplayOptionsRegistry, signal consolidation, viewport state, 6 sub-widgets integrated |
| 3.0 Widget Migration Tracking | ✅ COMPLETE | - | Migration tracking table, priority assessment |
| 3.1 EditorRegistry & Test_Widget Integration | ✅ COMPLETE | 1 week | EditorRegistry implementation, Test_Widget registration, unified widget creation |
| 3.2-3.3 UI Zones & PropertiesHost | 📋 PLANNED | 2 weeks | Standard UI zones, ZoneManager, PropertiesHost |
| 4. Command Pattern | 📋 PLANNED | 3 weeks | Command base, CommandManager, example commands |
| 5. Widget Migrations | 🔄 IN PROGRESS | 8 weeks | DataViewer (follow Media pattern), Analysis, Tables |
| 6. Advanced Features | 📋 PLANNED | 4 weeks | Drag/drop, session management |

**Elapsed: ~12 weeks | Remaining: ~20 weeks (~5 months)**

**Progress**: 56% Complete
- Phase 1: Core infrastructure ✅
- Phase 2: State integration & communication ✅  
  - Phase 2.6.1: Media_Widget hybrid architecture ✅ (reference implementation)
  - Phase 2.7: DataTransform external selection ✅
- Phase 3: Central Properties Zone 🔄
  - Phase 3.0: Widget tracking table ✅
  - Phase 3.1: EditorRegistry & Test_Widget integration ✅ **Factory pattern validated**
  - Phase 3.2-3.3: UI Zones & PropertiesHost 📋
- Phase 5: Widget migrations 🔄 (1 of 6 widgets complete)

**Key Accomplishment**: Media_Widget serves as comprehensive reference implementation with:
- Generic DisplayOptionsRegistry pattern
- Signal consolidation strategies (60% reduction)
- Single source of truth enforcement (no duplicate storage)
- Full state serialization (viewport, options, preferences, overlays, modes)

**Widget Migration Status** (from Phase 3.0 tracking table):
- Primary widgets with EditorState: 3/9 (33%)
- Primary widgets with SelectionContext: 3/9 (33%)
- Primary widgets with EditorRegistry: 1/9 (11%) - Test_Widget ✅
- Secondary widgets with EditorRegistry: 0/6 (0%)

**Next Steps**: 
1. **Immediate**: Implement PropertiesHost for unified properties panel (Phase 3.2)
2. **Then**: Migrate more widgets to EditorRegistry pattern (Media_Widget, DataViewer_Widget)
3. **Parallel**: Continue DataViewer_Widget state migration (Phase 5)

## Next Steps

### Immediate: Phase 3.1 - Test_Widget Proof-of-Concept ✅ COMPLETE (Week 12)

**Status**: Implementation complete (2026-01-21). View/Properties split pattern validated.

**What Was Accomplished**:
1. ✅ Created TestWidgetState with 7 properties (booleans, color, numeric, text)
2. ✅ Implemented TestWidgetView (QGraphicsView with grid, crosshair, animation)
3. ✅ Implemented TestWidgetProperties (form with checkboxes, color picker, sliders)
4. ✅ Both components share TestWidgetState via shared_ptr
5. ✅ State registered with EditorRegistry
6. ✅ Full signal-based synchronization working
7. ✅ All tests passing

**Key Validation**:
- ✅ State changes in properties immediately reflect in view
- ✅ Multiple property types demonstrated (boolean, color, numeric, text)
- ✅ No circular update loops (prevented with `_updating_from_state` flag)
- ✅ Pattern is clean and maintainable

### Then: Phase 3.2 - PropertiesHost Implementation (Week 13)

**Goal**: Create unified properties panel that replaces per-widget embedded properties.

**Approach**:
1. Create PropertiesHost widget with factory registration system
2. Register Test_Widget properties factory with PropertiesHost
3. Integrate PropertiesHost into MainWindow right panel
4. Connect to SelectionContext for context-sensitive display
5. Update Test_Widget to NOT create QSplitter (view only in dock, properties in PropertiesHost)
6. Validate dynamic properties swapping works

**Success Criteria**:
- PropertiesHost appears in right dock area
- Selecting Test_Widget shows TestWidgetProperties in PropertiesHost
- Switching between different widget types swaps properties panels
- No performance issues with panel switching

### Parallel: Phase 5 - DataViewer_Widget Migration (Weeks 11-14)

**Prerequisites**: 
- Phase 2 complete ✅
- Media_Widget hybrid architecture complete ✅ (reference implementation)
- DataTransform_Widget external selection validated ✅

**Goal**: Apply Media_Widget's hybrid architecture pattern to DataViewer_Widget

**Approach**: Follow the proven Media_Widget pattern:
1. Create DisplayOptionsRegistry equivalent for analog/event/interval configs
2. Consolidate signals (multiple `*ConfigChanged` → category-based signals)
3. Remove duplicate storage from DataViewer class
4. Integrate sub-components (analog plots, event plots, interval plots)
5. Make viewport/scale state single source of truth
6. Comprehensive testing and serialization validation

**Reference**: Use [MEDIA_WIDGET_HYBRID_STATE_PLAN.md](MEDIA_WIDGET_HYBRID_STATE_PLAN.md) as template

**Expected Outcome**: 
- Similar method/signal reduction (~60%)
- Clean state separation
- Full serialization support
- Second reference implementation validated

### Future: Phase 3.2-3.3 - UI Zones (Weeks 15-16)

**Prerequisites**: 
- Test_Widget proof-of-concept complete
- PropertiesHost and EditorRegistry working
- Pattern validated

**Goal**: Create unified properties panel that replaces per-widget Feature_Table_Widget instances

**Timing Rationale**: Delaying Phase 3 until more widgets have mature state allows us to:
- Identify common patterns across widget types
- Design PropertiesHost based on real usage patterns  
- Avoid premature abstraction
- Ensure external selection pattern is proven at scale

### Phase 3.1 PropertiesHost Widget (Week 14)

Create a context-sensitive properties container that observes SelectionContext and displays appropriate properties based on active editor and selected data type.

## References

- [Blender Architecture](https://wiki.blender.org/wiki/Source/Architecture)
- [Qt Model/View Architecture](https://doc.qt.io/qt-6/model-view-programming.html)
- [Command Pattern](https://refactoring.guru/design-patterns/command)
- [reflect-cpp Documentation](https://github.com/getml/reflect-cpp)
- [Qt Advanced Docking System](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System)
