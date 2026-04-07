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
