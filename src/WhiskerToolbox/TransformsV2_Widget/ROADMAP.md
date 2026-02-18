# TransformsV2 Widget — Implementation Roadmap

## Overview

The TransformsV2 Widget replaces the V1 `DataTransform_Widget` with a pipeline-centric UI for composing, configuring, executing, and sharing V2 transform pipelines. Key improvements over V1:

| Concern | V1 | V2 |
|---|---|---|
| Transform model | Single operation in → out | Multi-step composable pipeline |
| Parameter UI | Hand-written widget per transform | Auto-generated from reflect-cpp, with override support |
| JSON integration | Load/execute only | Bidirectional: UI ↔ JSON, live serialization |
| Reusability | Standalone widget only | Embeddable; JSON pipeline connects to TensorData lazy columns, TableView, etc. |
| State | Minimal (selected key/operation) | Pipeline descriptor as serializable state |
| Intermediate data | Materialized to DataManager | Optional; pipeline fusion avoids allocation |

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│              TransformsV2Properties_Widget               │
│  ┌───────────────────────┐  ┌────────────────────────┐  │
│  │   Pipeline Builder    │  │   JSON Panel           │  │
│  │  ┌─────────────────┐  │  │  ┌──────────────────┐  │  │
│  │  │ Input Selector   │  │  │  │ Live JSON View   │  │  │
│  │  │ (DataFocusAware) │  │  │  │ (auto-synced)    │  │  │
│  │  ├─────────────────┤  │  │  ├──────────────────┤  │  │
│  │  │ Step List        │  │  │  │ Load/Save        │  │  │
│  │  │ (add/remove/     │  │  │  │ Validate         │  │  │
│  │  │  reorder steps)  │  │  │  │ Execute          │  │  │
│  │  ├─────────────────┤  │  │  └──────────────────┘  │  │
│  │  │ Step Config      │  │  └────────────────────────┘  │
│  │  │ (auto-generated  │  │                              │
│  │  │  or custom)      │  │  ┌────────────────────────┐  │
│  │  ├─────────────────┤  │  │   Output Config         │  │
│  │  │ Pre-Reductions   │  │  │  ┌──────────────────┐  │  │
│  │  └─────────────────┘  │  │  │ Output key name   │  │  │
│  └───────────────────────┘  │  │ Mode: DataManager  │  │  │
│                              │  │   or JSON-only     │  │  │
│                              │  │ Execute button     │  │  │
│                              │  └──────────────────┘  │  │
│                              └────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
         │                                    │
         │ builds                             │ serializes to/from
         ▼                                    ▼
  TransformPipeline                    PipelineDescriptor
  (executable)                         (JSON-serializable)
         │
         ▼
  DataManager / External Consumer
```

---

## Phase 0: Auto-Generated Parameter UI Infrastructure ✅ COMPLETE

**Goal:** Build the compile-time → runtime bridge that converts any reflect-cpp parameter struct into a Qt form, eliminating the need for a hand-written widget per transform.

### 0.1 — `ParameterFieldDescriptor` Runtime Schema ✅

Implemented in `src/TransformsV2/core/ParameterSchema.hpp`.

`ParameterFieldDescriptor` captures field name, underlying type (parsed from the reflect-cpp type string), display name (snake_case → "Title Case"), optional validator constraints (min/max, exclusive flags), allowed values for enum-like string fields, default value JSON, and UI hints (tooltip, group, is_advanced, display_order, is_optional, is_bound).

`ParameterSchema` wraps a vector of descriptors and provides `field(name)` lookup.

### 0.2 — Compile-Time Schema Extraction ✅

Implemented via `extractParameterSchema<Params>()` in `src/TransformsV2/core/ParameterSchema.hpp`.

Uses `rfl::fields<Params>()` to enumerate fields at compile time. The type string from each `MetaField` is parsed by helpers in `src/TransformsV2/core/ParameterSchema.cpp`:
- `snakeCaseToDisplay()` — "scale_factor" → "Scale Factor"
- `parseUnderlyingType()` — strips optional/Validator wrappers to find the base type
- `isOptionalType()`, `hasValidator()` — type string classification
- `extractConstraints()` — parses `rfl::ExclusiveMinimum`, `rfl::Minimum`, etc. into `ConstraintInfo`

Schema factories are registered automatically in `ElementRegistry::registerParamDeserializerIfNeeded<In, Out, Params>()` alongside the existing JSON deserializers. Schemas are lazily cached on first access. New public methods on `ElementRegistry`: `getParameterSchema(name)` and `getParameterSchemaByType(type_index)`.

### 0.3 — UI Hint Annotations (Optional Override) ✅

Implemented via `ParameterUIHints<Params>` template trait in `src/TransformsV2/core/ParameterSchema.hpp`. Specialize the template alongside a params struct to annotate fields with tooltips, allowed values, grouping, and advanced flags. The default (unspecialized) version is a no-op. `extractParameterSchema<T>()` calls `ParameterUIHints<T>::annotate(schema)` automatically after auto-extraction.

### 0.4 — `AutoParamWidget`: Generic Qt Form Generator ✅

Implemented in `src/WhiskerToolbox/TransformsV2_Widget/UI/AutoParamWidget.hpp/.cpp`.

Field type → widget mapping:

| Field Type | Widget |
|---|---|
| `float` / `double` | `QDoubleSpinBox` (range set from validator constraints) |
| `int` | `QSpinBox` |
| `bool` | `QCheckBox` |
| `std::string` (with `allowed_values`) | `QComboBox` |
| `std::string` (free-form) | `QLineEdit` |
| `std::optional<T>` | Checkbox-gated inner widget (unchecked = use default) |

Advanced fields are placed in a collapsible `QGroupBox`. Exposes `setSchema()`, `toJson()`, `fromJson()`, and `parametersChanged()` signal. Uses `rfl::Object::get()` (public API) for JSON field lookup.

### 0.5 — Custom Widget Override Registry ✅

Implemented as `ParamWidgetRegistry` singleton in `src/WhiskerToolbox/TransformsV2_Widget/UI/ParamWidgetRegistry.hpp`. Register a factory with `registerCustomWidget<Params>(factory)`. Query with `hasCustomWidget(type_index)` and `createCustomWidget(type_index, parent)`. Header-only; no `.cpp` needed.

**Implemented files:**
- `src/TransformsV2/core/ParameterSchema.hpp` — `ParameterFieldDescriptor`, `ParameterSchema`, `ParameterUIHints<T>`, `extractParameterSchema<T>()`
- `src/TransformsV2/core/ParameterSchema.cpp` — `snakeCaseToDisplay`, `parseUnderlyingType`, `isOptionalType`, `hasValidator`, `extractConstraints`
- `src/WhiskerToolbox/TransformsV2_Widget/UI/AutoParamWidget.hpp/.cpp` — generic form generator
- `src/WhiskerToolbox/TransformsV2_Widget/UI/ParamWidgetRegistry.hpp` — custom widget override registry
- `tests/TransformsV2/test_parameter_schema.test.cpp` — 14 test cases / 115 assertions (all passing)

---

## Phase 1: Pipeline Builder Core UI ✅ COMPLETE

**Goal:** Replace the V1 "single transform" model with a multi-step pipeline builder.

### 1.1 — Input Selector with DataFocusAware ✅

Implemented in `TransformsV2Properties_Widget`. The widget inherits `DataFocusAware` and connects to `SelectionContext` via `create_editor_custom` registration pattern (same as `DataImport_Widget`). When data is focused in `DataManager_Widget`, `onDataFocusChanged()` updates the input display (key name + type label), resolves element/container types via `TypeIndexMapper`, and propagates to sub-widgets. Selected input key stored in `TransformsV2State::input_data_key`.

### 1.2 — Step List Widget ✅

Implemented as `PipelineStepListWidget` (`UI/PipelineStepListWidget.hpp/.cpp`).

- `QListWidget` showing numbered pipeline steps
- **[+] Add Step**: `QInputDialog` with filtered list of transforms compatible with the current output type (queries both `getTransformsForInputType()` and `getContainerTransformsForInputType()`)
- **[✕] Remove**: Removes step, re-validates type chain
- **[↑][↓] Reorder**: Move up/down buttons with type chain revalidation
- Step selection emits `stepSelected(int)` for the config panel
- `PipelineStepEntry` struct tracks step_id, transform_name, parameters_json, input/output types, validity, and container flag

### 1.3 — Step Configuration Panel ✅

Implemented as `StepConfigPanel` (`UI/StepConfigPanel.hpp/.cpp`).

When a step is selected:
1. Looks up `TransformMetadata` from `ElementRegistry::getMetadata()`
2. Checks `ParamWidgetRegistry` for custom widget override
3. Falls back to `AutoParamWidget` with schema from `ElementRegistry::getParameterSchema()`
4. Populates from existing step params JSON via `fromJson()`
5. `parametersChanged()` signal propagates back to `PipelineStepListWidget::updateStepParams()`

Header shows transform name (bold) and description (gray). Scrollable content area for parameter forms. "No configurable parameters" placeholder for `NoParams` transforms.

### 1.4 — Pre-Reduction Configuration ✅

Implemented as `PreReductionPanel` (`UI/PreReductionPanel.hpp/.cpp`).

- Collapsible `QGroupBox` ("Pre-Computations", collapsed by default)
- Lists configured pre-reduction entries with `reduction_name → output_key` format
- **[+] Add**: `QInputDialog` filtered by `RangeReductionRegistry::getReductionsForInputType()`; second dialog for output key name
- **[✕] Remove**: Removes entry
- `PreReductionEntry` struct tracks reduction_name, output_key, parameters_json
- `preReductionsChanged()` signal notifies the parent widget

### 1.5 — Type Chain Validation ✅

Implemented in `PipelineStepListWidget::validateTypeChain()`.

- Walks the step chain from input type forward
- Element transforms: validates `metadata.input_type` matches current element type; updates via `metadata.output_type`
- Container transforms: validates `ContainerTransformMetadata.input_container_type`; updates via `output_container_type`
- Cross-domain mapping via `TypeIndexMapper::elementToContainer()` / `containerToElement()`
- Invalid steps shown with light-red background and tooltip in the list
- `validationChanged(bool)` signal drives a status label ("Pipeline valid ✓" / "Pipeline has type errors ✗")

**Implemented files:**
- `src/WhiskerToolbox/TransformsV2_Widget/UI/PipelineStepListWidget.hpp/.cpp` — step list with add/remove/reorder/validation
- `src/WhiskerToolbox/TransformsV2_Widget/UI/StepConfigPanel.hpp/.cpp` — auto-generated or custom param editing
- `src/WhiskerToolbox/TransformsV2_Widget/UI/PreReductionPanel.hpp/.cpp` — pre-computation configuration
- `src/WhiskerToolbox/TransformsV2_Widget/UI/TransformsV2Properties_Widget.hpp/.cpp` — main widget with DataFocusAware, layout integration
- `src/WhiskerToolbox/TransformsV2_Widget/Core/TransformsV2State.hpp/.cpp` — extended with `input_data_key`
- `src/WhiskerToolbox/TransformsV2_Widget/TransformsV2WidgetRegistration.cpp` — switched to `create_editor_custom` for SelectionContext access

---

## Phase 2: Bidirectional JSON Synchronization ✅ COMPLETE

**Goal:** The UI and JSON representations stay in sync. Users can configure in UI and get JSON, or paste JSON and see it reflected in UI.

### 2.1 — Live JSON Panel ✅

A collapsible `QGroupBox` ("Pipeline JSON") containing a monospace `QTextEdit` that shows the current `PipelineDescriptor` as pretty-printed JSON. Updated in real-time via `syncJsonFromUI()` on every pipeline change.

`buildJsonFromUI()` constructs a `PipelineDescriptor` from the current step list and pre-reduction panel, then calls `savePipelineToJson()`. A `_syncing_json` bool guard prevents feedback loops when the JSON panel update would itself trigger `onPipelineChanged`.

### 2.2 — JSON → UI Loading ✅

`loadUIFromJson(json_str)` implements the reverse direction:
1. Parses with `rfl::json::read<PipelineDescriptor>`
2. Calls `_pre_reduction_panel->loadFromDescriptors()` to rebuild pre-reduction entries
3. Calls `_step_list->loadFromDescriptors()` to rebuild steps (added `loadFromDescriptors()` to both `PipelineStepListWidget` and `PreReductionPanel`)
4. Each step's `parameters` (`rfl::Generic`) is written back to a JSON string via `rfl::json::write()` and stored in `PipelineStepEntry::parameters_json`
5. Returns false and shows an error banner on invalid JSON

Triggered by the **Apply JSON** button or **Load JSON** file dialog.

### 2.3 — Copy/Paste and File I/O ✅

- **Copy JSON** (`onCopyJsonClicked`): Uses `QClipboard` to copy the JSON panel text
- **Load JSON** (`onLoadJsonClicked`): `QFileDialog::getOpenFileName` (`.json` filter); reads file, sets JSON panel text, calls `loadUIFromJson()`
- **Save JSON** (`onSaveJsonClicked`): `QFileDialog::getSaveFileName`; writes the current JSON panel text to file
- **Apply JSON** (`onApplyJsonClicked`): Calls `loadUIFromJson()` on the current panel text; shows success/failure via error banner label

The JSON panel itself is editable but does not auto-apply on every keystroke — the user clicks **Apply JSON** to commit manual edits, preventing partial-JSON parse errors while typing.

### 2.4 — `pipelineDescriptorChanged` Signal ✅

`pipelineDescriptorChanged(std::string const & pipeline_json)` is emitted from `syncJsonFromUI()` whenever the pipeline changes (add/remove/reorder steps, parameter edits, pre-reduction changes). This is the hook for external consumers (Phase 4).

### 2.5 — State Integration ✅

`TransformsV2StateData` extended with:
- `std::optional<std::string> pipeline_json` — current `PipelineDescriptor` as JSON (single source of truth)
- `bool json_panel_expanded` — UI preference for the JSON panel collapsed/expanded state

`TransformsV2State` gained `setPipelineJson()` / `pipelineJson()` and `setJsonPanelExpanded()` / `jsonPanelExpanded()` accessors, plus a `pipelineJsonChanged` signal.

**Implemented files:**
- `src/WhiskerToolbox/TransformsV2_Widget/UI/TransformsV2Properties_Widget.hpp/.cpp` — JSON panel, `buildJsonFromUI()`, `syncJsonFromUI()`, `loadUIFromJson()`, Copy/Load/Save/Apply slots, `pipelineDescriptorChanged` signal
- `src/WhiskerToolbox/TransformsV2_Widget/UI/PipelineStepListWidget.hpp/.cpp` — added `loadFromDescriptors(vector<PipelineStepDescriptor>)`
- `src/WhiskerToolbox/TransformsV2_Widget/UI/PreReductionPanel.hpp/.cpp` — added `loadFromDescriptors(vector<PreReductionStepDescriptor>)`
- `src/WhiskerToolbox/TransformsV2_Widget/Core/TransformsV2State.hpp/.cpp` — extended with `pipeline_json`, `json_panel_expanded`

---

## Phase 3: Pipeline Execution ✅ COMPLETE

**Goal:** Execute the configured pipeline against data in DataManager and store results.

### 3.1 — Output Configuration ✅

Added an **Output && Execution** `QGroupBox` below the JSON panel containing:
- **Output key name**: `QLineEdit`, auto-populated via `generateOutputName()` (input key + last transform name, stripping common prefixes like `calculate_`, `extract_`, `convert_`, `threshold_`). User edits are tracked via `_output_key_user_edited`; auto-generation only fires when the user hasn't manually typed a name.
- **Execution mode** `QComboBox`:
  - *"Save to DataManager"* — materializes result into a new data key
  - *"JSON Pipeline Only"* — confirms the JSON is ready; no DataManager write

Both values are persisted in `TransformsV2StateData` (`output_data_key`, `execution_mode`) and restored on widget creation.

### 3.2 — Execution via `loadPipelineFromJson` + `executePipeline` ✅

`onExecuteClicked()` implements the full execution path:
1. Resolves execution mode; JSON-only mode shows a success message and returns early
2. Validates DataManager is available, input key is selected, output key is non-empty
3. Calls `loadPipelineFromJson(json_str)` → `rfl::Result<TransformPipeline>` (from `PipelineLoader.hpp`)
4. Retrieves `input_variant` via `DataManager::getDataVariant(_input_data_key)`
5. Calls the free function `executePipeline(input_variant, pipeline)` → `DataTypeVariant`
6. Stores result via `DataManager::setData(output_key, result, dm->getTimeKey(_input_data_key))`
7. Updates `_state->setOutputDataKey()` and shows a success message with elapsed time (ms)

Note: Uses `loadPipelineFromJson` + `executePipeline` (free function) directly rather than `DataManagerPipelineExecutor`, because the widget's `PipelineDescriptor` (no input/output keys) is a different type from `DataManagerPipelineDescriptor`.

### 3.3 — Progress Reporting ✅

- `QProgressBar` below the execute button, range 0…N steps, hidden when idle
- `QLabel` progress label showing `"Step K/N: TransformName"` for each step, updated via `QApplication::processEvents()` before calling `executePipeline`
- Execute button disabled during execution, re-enabled via `updateExecuteButtonState()` on completion

### 3.4 — Error Display ✅

- Inline `_error_label` (`QLabel`) styled red for errors, green for success
- Shows descriptive messages for: no DataManager, no input selected, empty output key, empty JSON, pipeline build failure, and `std::exception` from execution
- Execute button enabled/disabled by `updateExecuteButtonState()`: requires valid input key + non-empty steps + non-empty output key (for DataManager mode)

### 3.5 — State Integration ✅

`TransformsV2StateData` extended with:
- `std::optional<std::string> output_data_key` — last used output key name
- `std::string execution_mode = "data_manager"` — persisted execution mode

`TransformsV2State` gained `setOutputDataKey()` / `outputDataKey()`, `setExecutionMode()` / `executionMode()`, and `outputDataKeyChanged` signal.

**Modified files:**
- `src/WhiskerToolbox/TransformsV2_Widget/Core/TransformsV2State.hpp/.cpp` — extended with `output_data_key`, `execution_mode`, accessors, `outputDataKeyChanged` signal
- `src/WhiskerToolbox/TransformsV2_Widget/UI/TransformsV2Properties_Widget.hpp` — added `QLineEdit`, `QComboBox`, `QProgressBar`, `QLabel` members; `onExecuteClicked`, `onOutputKeyEdited` slots; `generateOutputName`, `updateOutputKeyFromPipeline`, `updateExecuteButtonState` helpers; `DataManager` forward declaration; `_output_key_user_edited` guard
- `src/WhiskerToolbox/TransformsV2_Widget/UI/TransformsV2Properties_Widget.cpp` — output/execution section in `setupUI()`, all Phase 3 slot/helper implementations; added `DataManager/DataManager.hpp` and `TransformsV2/core/TransformPipeline.hpp` includes

---

## Phase 4: External Pipeline Consumers (Embeddable Mode)

**Goal:** Allow other widgets to use the TransformsV2 pipeline builder to configure pipelines without executing them against DataManager. This is the key extensibility point.

### 4.1 — `PipelineConfigResult` Abstraction

Define a result type that decouples pipeline *configuration* from pipeline *execution*:

```cpp
struct PipelineConfigResult {
    std::string pipeline_json;                    // Serialized PipelineDescriptor
    std::optional<TransformPipeline> pipeline;    // Ready-to-execute pipeline object
    std::string input_type_name;                  // E.g., "AnalogTimeSeries"
    std::string output_type_name;                 // E.g., "DigitalEventSeries"
};
```

### 4.2 — Embeddable Pipeline Builder Widget

Factor the pipeline builder into a reusable widget that can be embedded:

```cpp
class PipelineBuilderWidget : public QWidget {
    Q_OBJECT
public:
    // Set the input data type (no DataManager needed)
    void setInputType(std::type_index input_element_type);
    
    // Get the configured pipeline as JSON
    [[nodiscard]] std::string pipelineJson() const;
    
    // Load a pipeline from JSON
    void loadFromJson(std::string const& json);
    
signals:
    void pipelineChanged(PipelineConfigResult const& result);
};
```

### 4.3 — TensorData Lazy Column Integration

In the TensorInspector widget, when configuring a lazy-evaluated column:
1. Embed a `PipelineBuilderWidget` in a dialog or sub-panel
2. Set the input type to the tensor's element type
3. On `pipelineChanged`, store the JSON in the `TensorData` column definition
4. The lazy backend uses `loadPipelineFromJson()` to reconstruct the pipeline at evaluation time

### 4.4 — TableView Column Computer Integration

Similar pattern: `TableDesignerWidget` can embed a `PipelineBuilderWidget` to define computed columns:
1. User selects source column → sets input type
2. Builds a pipeline in the embedded builder
3. Pipeline JSON stored in the column computer definition
4. On table evaluation, pipeline is reconstructed and executed per cell/row

**Files to create:**
- `src/WhiskerToolbox/TransformsV2_Widget/UI/PipelineBuilderWidget.hpp/.cpp` — reusable embeddable builder
- Modify `TransformsV2Properties_Widget` to embed `PipelineBuilderWidget` + add execution/output controls

**Estimated effort:** Medium. The builder widget is a refactor of Phase 1 UI into a reusable component.

---

## Phase 5: State Serialization

**Goal:** Minimal EditorState that preserves the user's workspace without tracking internal sub-widget details.

### 5.1 — `TransformsV2StateData` Extension

```cpp
struct TransformsV2StateData {
    std::string instance_id;
    std::string display_name = "Transforms V2";
    
    // Pipeline state (the JSON is the source of truth)
    std::optional<std::string> pipeline_json;       // Current PipelineDescriptor as JSON
    std::optional<std::string> input_data_key;      // Currently selected input
    std::optional<std::string> output_data_key;     // Last used output key name
    std::optional<std::string> execution_mode;      // "data_manager" or "json_only"
    
    // UI preferences (not pipeline internals)
    bool json_panel_expanded = false;
    bool pre_reduction_panel_expanded = false;
};
```

This deliberately does **not** store the internal state of individual parameter widgets. The pipeline JSON captures all parameter values. On workspace restore:
1. `fromJson()` restores the state data
2. Pipeline JSON is loaded into the builder (Phase 2.2 path)
3. Builder reconstructs step widgets from the descriptor

### 5.2 — Dirty Tracking

Mark state dirty on any pipeline change. This integrates with the workspace save system.

**Files to modify:**
- `TransformsV2State.hpp/.cpp` — extend `TransformsV2StateData`

**Estimated effort:** Small.

---

## Phase 6: Transform Migration & Polish

**Goal:** Port V1 transforms to V2, add UX polish.

### 6.1 — Priority Transforms to Port

Focus on transforms that benefit most from pipelining:

| V1 Transform | V2 Status | Notes |
|---|---|---|
| Mask Area | ✅ Done | `CalculateMaskArea` |
| Line Angle | ✅ Done | `CalculateLineAngle` |
| Line Curvature | ✅ Done | `CalculateLineCurvature` |
| Z-Score Normalize | ✅ Done | `ZScoreNormalizeV2` with pre-reductions |
| Analog Event Threshold | ✅ Done | Container transform |
| Mask Centroid | ✅ Done | `CalculateMaskCentroid` |
| Analog Filter | 🔄 Port | Good pipeline candidate (filter → threshold → detect) |
| Line Resample | ✅ Done | `ResampleLine` |
| Others | 🔄 Port | Follow registration pattern |

### 6.2 — UI Polish

- Keyboard shortcuts (Ctrl+Enter to execute, Ctrl+S to save JSON)
- Undo/redo for pipeline edits (store descriptor stack)
- Transform search/filter in the add-step combo box
- Category grouping in the transform list (from `TransformMetadata.category`)
- Tooltips from `TransformMetadata.description`
- Dark/light theme support for JSON editor (syntax highlighting)

### 6.3 — Deprecation Path for V1 Widget

- V1 and V2 widgets coexist; V2 is registered under `"View/Tools"` menu
- V1 JSON pipelines can be loaded in V2 (V1 format → V2 descriptor conversion)
- Once all transforms are ported, V1 widget can be removed

---

## Implementation Order & Dependencies

```
Phase 0 (Auto-UI Infrastructure) ✅ COMPLETE
    │
    ├──→ Phase 1 (Pipeline Builder UI) ✅ COMPLETE ──→ Phase 2 (JSON Sync) ✅ COMPLETE
    │                                                       │
    │                                                       ├──→ Phase 3 (Execution) ✅ COMPLETE
    │                                                       │
    │                                                       └──→ Phase 4 (Embeddable Mode)
    │
    └──→ Phase 5 (State Serialization)  [partially done in Phases 2–3; pipeline_json, output_data_key, execution_mode added]
    
Phase 6 (Migration & Polish)  [ongoing, parallel with Phase 4]
```

**Recommended next:** Phase 4 (Embeddable Mode) — factor the pipeline builder into a reusable `PipelineBuilderWidget` for embedding in `TensorInspector` and `TableDesignerWidget`. Phase 5 (remaining state fields: `pre_reduction_panel_expanded`) is a small add-on.

---

## Key Design Decisions

### Why JSON as the Inter-Widget Protocol?

The pipeline JSON (`PipelineDescriptor`) is already a reflect-cpp struct with automatic serialization. Using it as the interchange format means:
- **No new IPC mechanism** — widgets pass strings
- **Human-readable** — users can inspect, edit, and version-control pipelines
- **Reproducible** — JSON pipelines are scripts that can be re-executed
- **Decoupled** — the consumer widget doesn't need to link against TransformsV2_Widget, only against TransformsV2 core

### Why Not Store Sub-Widget State in EditorState?

Each transform's parameter widget is ephemeral. The pipeline JSON already captures all parameter values with full fidelity (it's the same format used for execution). Storing sub-widget state separately would create a parallel source of truth. Instead:
- Pipeline JSON is the single source of truth for all parameter values
- UI state (which sections are expanded) is minimal and stored in `TransformsV2StateData`
- On restore, the JSON is loaded and widgets are reconstructed from it

### Why `rfl::fields<T>()` for Auto-UI?

reflect-cpp's `rfl::fields<T>()` returns `std::array<MetaField, N>` with field name and type as strings at compile time. Combined with `rfl::to_view()` for mutable access, this gives us:
- **Zero-boilerplate field enumeration** for any params struct
- **Type string parsing** to select appropriate Qt widgets
- **Validator constraint extraction** from `rfl::Validator<T, Constraint>` type strings to set min/max on spin boxes
- **Default values** from default-constructed params via the `getXxx()` helpers

The alternative (manually writing a widget per transform) doesn't scale — V1 has 30+ hand-written widgets each with 50-150 lines of boilerplate.

### Why Embeddable Builder Instead of Signals?

A signal-only approach (`pipelineExecuted(QString json)`) is one-directional. The embeddable `PipelineBuilderWidget` allows:
- Setting input type constraints (no DataManager needed)
- Receiving pipeline changes in real-time (not just on execution)
- Pre-populating with an existing pipeline (e.g., editing a TensorData lazy column)
- Being placed in dialogs, dock panels, or inline in other widgets

---

## File Structure

```
TransformsV2_Widget/
├── CMakeLists.txt
├── ROADMAP.md
├── TransformsV2WidgetRegistration.hpp       ✅ exists (create_editor_custom pattern)
├── TransformsV2WidgetRegistration.cpp       ✅ exists (create_editor_custom pattern)
├── Core/
    │   ├── TransformsV2State.hpp                ✅ Phase 3 complete (output_data_key, execution_mode added)
    │   └── TransformsV2State.cpp                ✅ Phase 3 complete
    └── UI/
        ├── TransformsV2Properties_Widget.hpp    ✅ Phase 3 complete (output config, execute button, progress bar, error label)
        ├── TransformsV2Properties_Widget.cpp    ✅ Phase 3 complete (onExecuteClicked, generateOutputName, updateExecuteButtonState)
        ├── TransformsV2Properties_Widget.ui     ✅ exists (base layout, sub-widgets added programmatically)
        ├── AutoParamWidget.hpp                  ✅ Phase 0 complete
        ├── AutoParamWidget.cpp                  ✅ Phase 0 complete
        ├── ParamWidgetRegistry.hpp              ✅ Phase 0 complete
        ├── PipelineStepListWidget.hpp           ✅ Phase 2 complete (loadFromDescriptors added)
        ├── PipelineStepListWidget.cpp           ✅ Phase 2 complete
        ├── StepConfigPanel.hpp                  ✅ Phase 1 complete
        ├── StepConfigPanel.cpp                  ✅ Phase 1 complete
        ├── PreReductionPanel.hpp                ✅ Phase 2 complete (loadFromDescriptors added)
        ├── PreReductionPanel.cpp                ✅ Phase 2 complete
# In TransformsV2 core library (non-Qt):
TransformsV2/core/
├── ParameterSchema.hpp                      ✅ Phase 0 complete
└── ParameterSchema.cpp                      ✅ Phase 0 complete

# Tests:
tests/TransformsV2/
└── test_parameter_schema.test.cpp           ✅ Phase 0 complete (14 cases, 115 assertions)
```
