# DeepLearning_Widget Refactoring Roadmap

## Current State

`DeepLearningPropertiesWidget` is a **2716-line monolithic class** handling 15+ responsibilities:

| Responsibility | Lines (approx) | Methods |
|---|---|---|
| UI construction (fixed + dynamic) | ~500 | `_buildUi`, `_rebuildSlotPanels`, `_clearDynamicContent` |
| Dynamic input panel building | ~90 | `_buildDynamicInputGroup` |
| Static/sequence input panel building | ~500 | `_buildStaticInputGroup`, `_addSequenceEntryRow` |
| Boolean mask panel building | ~20 | `_buildBooleanMaskGroup` |
| Output panel building | ~55 | `_buildOutputGroup` |
| Recurrent binding panel building | ~100 | `_buildRecurrentInputGroup` |
| Post-encoder section | ~170 | `_buildPostEncoderSection`, `_enforcePostEncoderDecoderConsistency` |
| Encoder shape section | ~100 | `_buildEncoderShapeSection`, `_applyEncoderShape` |
| State sync (UI → State) | ~270 | `_syncBindingsFromUi` |
| Data source combo management | ~100 | `_populateDataSourceCombo`, `_refreshDataSourceCombos` |
| Model selection + weights loading | ~150 | `_populateModelCombo`, `_onModelComboChanged`, `_loadModelIfReady`, `_updateWeightsStatus` |
| Single-frame inference | ~40 | `_onRunSingleFrame` |
| Batch inference + worker thread | ~200 | `_onRunBatch`, `_setBatchRunning`, `_onCancelBatch`, `_onBatchFinished` |
| Recurrent inference | ~90 | `_onRunRecurrentSequence` |
| Result merging + feature accumulation | ~70 | `_mergeResults` |
| Static capture | ~130 | `_onCaptureStaticInput`, `_onCaptureSequenceEntry`, `_updateCaptureButtonState` |
| Batch-size constraint enforcement | ~90 | `_updateBatchSizeConstraint` |
| Timeline integration | ~50 | `onTimeChanged`, `_onPredictCurrentFrame` |

The widget directly constructs all Qt widgets programmatically, manually marshals widget values to/from `DeepLearningState` via `findChild` lookups, and orchestrates all inference modes inline.

---

## Design Goals

1. **Separation of concerns**: Each distinct responsibility lives in its own class/file.
2. **Leverage ParameterSchema + AutoParamWidget**: Replace hand-built form construction and manual `findChild`-based marshaling with schema-driven auto-generated forms where possible.
3. **Testability**: Core logic (binding assembly, constraint enforcement, result processing) should be unit-testable without Qt widgets.
4. **Incremental delivery**: Each phase is independently shippable and testable. No big-bang rewrite.
5. **Preserve PIMPL firewall**: All libtorch usage continues to be confined to `SlotAssembler.cpp`.

---

## Phase 0 — Preparatory Infrastructure

### 0.0 — Refactor DeepLearning library to use reflect-cpp param structs ✅ COMPLETED

Replaced all monolithic parameter structs with per-component param structs. Each struct contains only the user-configurable fields for its specific component. Runtime context fields (set by `SlotAssembler`, not user-facing) are separated into `DecoderContext` and `EncoderContext`.

#### Task 0.0.1 — Split `DecoderParams` into per-decoder param structs ✅ COMPLETED

- **`DecoderContext`** — Runtime fields: source_channel, batch_index, height, width, target_image_size
- **`MaskDecoderParams{threshold}`**, **`PointDecoderParams{subpixel, threshold}`**, **`LineDecoderParams{threshold}`**, **`FeatureVectorDecoderParams{}`**
- Each decoder's `decode()` now takes `(tensor, DecoderContext, XxxDecoderParams)`. Companion `DecoderParamSchemas.hpp` provides `ParameterUIHints` specializations.

#### Task 0.0.2 — Split `EncoderParams` into per-encoder param structs ✅ COMPLETED

- **`EncoderContext`** — Runtime fields: target_channel, batch_index, height, width
- **`ImageEncoderParams{normalize}`**, **`Point2DEncoderParams{mode, gaussian_sigma, normalize}`**, **`Mask2DEncoderParams{mode, normalize}`**, **`Line2DEncoderParams{mode, gaussian_sigma, normalize}`**
- `RasterMode` is a C++ `enum class`; `extractParameterSchema` auto-populates `allowed_values`. `ParameterUIHints` in `EncoderParamSchemas.hpp` restricts mode options per-encoder (e.g. Mask2DEncoder only exposes "Binary").

#### Task 0.0.3 — Make `PostEncoderModuleParams` reflect-cpp compatible ✅ COMPLETED

- **`GlobalAvgPoolModuleParams{}`** (no user fields), **`SpatialPointModuleParams{interpolation}`** where `InterpolationMode` is a C++ `enum class`
- `ImageSize` is runtime context, passed separately: `PostEncoderModuleFactory::create(module_name, ImageSize, SpatialPointModuleParams)`
- Companion `PostEncoderParamSchemas.hpp` provides `ParameterUIHints` for `SpatialPointModuleParams`.

#### Task 0.0.4 — Add `ParameterSchema` dependency to DeepLearning CMakeLists.txt ✅ COMPLETED

Added `target_link_libraries(DeepLearning PUBLIC WhiskerToolbox::ParameterSchema)` to the DeepLearning CMakeLists.txt.

Created the three companion ParameterUIHints header files:
- `channel_decoding/DecoderParamSchemas.hpp` — Hints for `MaskDecoderParams`, `PointDecoderParams`, `LineDecoderParams`
- `channel_encoding/EncoderParamSchemas.hpp` — Hints for `ImageEncoderParams`, `Point2DEncoderParams`, `Mask2DEncoderParams`, `Line2DEncoderParams`
- `post_encoder/PostEncoderParamSchemas.hpp` — Hints for `SpatialPointModuleParams`

All three files are registered in the DeepLearning CMakeLists.txt source list.

#### Task 0.0.5 — Update DeepLearningBindingData and SlotAssembler ✅ COMPLETED

SlotAssembler was updated during Tasks 0.0.1–0.0.3 to:
1. Construct `DecoderContext` + per-decoder params in `decodeOutputs` and `decodeOutputsToBuffer`.
2. Construct `EncoderContext` + per-encoder params in `encodeDynamicSlot`.
3. Pass `ImageSize` separately to `PostEncoderModuleFactory::create()` in `configurePostEncoderModule`.

`DeepLearningBindingData.hpp` retains its current flat fields (`threshold`, `subpixel`, `mode`, `gaussian_sigma`) for backward-compatible state serialization. `SlotAssembler` maps these to the new per-component param structs internally.

The `configurePostEncoderModule` public API retains its string-based `interpolation` parameter because `SlotAssembler.hpp` is the PIMPL firewall — exposing `SpatialPointModuleParams` would pull torch-adjacent forward declarations into Qt widget TUs. The internal conversion from string to `InterpolationMode` is the correct pattern.

#### Outcome of Phase 0.0

After this phase:
- `extractParameterSchema<MaskDecoderParams>()` works and produces a schema with one `threshold` field.
- `extractParameterSchema<Point2DEncoderParams>()` works and produces fields with `mode` as auto-detected enum.
- `extractParameterSchema<SpatialPointModuleParams>()` works with `interpolation` as auto-detected enum.
- The DeepLearning library owns its own parameter definitions — the widget layer consumes them rather than reinventing them.
- Each param struct is unit-testable for schema extraction and JSON round-trips.

---

### 0.1 — Define reflect-cpp parameter structs for widget-level binding data ✅ COMPLETED

With the DeepLearning library owning per-component param structs (Phase 0.0), this phase defines the **widget-level binding structs** that compose them. These struct are specific to how the UI organizes slot bindings and are NOT part of the DeepLearning library.

**What belongs here (widget-level orchestration):**
- `CaptureModeVariant` — `rfl::TaggedUnion<"capture_mode", RelativeCaptureParams, AbsoluteCaptureParams>` — how a static input is acquired (UI workflow).
- `RecurrentInitVariant` — `rfl::TaggedUnion<"init_mode", ZerosInitParams, StaticCaptureInitParams, FirstOutputInitParams>` — how recurrent state is initialized at t=0.
- `SequenceEntryVariant` — `rfl::TaggedUnion<"source_type", StaticSequenceEntryParams, RecurrentSequenceEntryParams>` — whether a sequence entry is data-driven or recurrent.
- `PostEncoderVariant` — `rfl::TaggedUnion<"module", NoPostEncoderParams, GlobalAvgPoolModuleParams, SpatialPointModuleParams>` — which post-encoder module to use. Note: `GlobalAvgPoolModuleParams` and `SpatialPointModuleParams` are re-used from the DeepLearning library.
- `DecoderVariant` — `rfl::TaggedUnion<"decoder", MaskDecoderParams, PointDecoderParams, LineDecoderParams, FeatureVectorDecoderParams>` — which decoder to use. The alternative structs are re-used from the DeepLearning library.
- `EncoderShapeParams` — Custom encoder input/output dimensions (UI only, passed to `SlotAssembler::configureModelShape()`).
- `OutputSlotParams` — Composite struct: `data_key` + `DecoderVariant`.

**Files created:**
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp` — All widget-level variant types, composite structs, and `ParameterUIHints` declarations.
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp` — `ParameterUIHints` implementations (tooltips, display names, allowed values).

**Tests:** `tests/DeepLearning/param_schemas/DeepLearningParamSchemas.test.cpp` — schema extraction, JSON round-trips, and UIHints annotation for all types.

**Outcome:** `extractParameterSchema<OutputSlotParams>()` produces a schema with `data_key` string field + `decoder` variant field. The variant alternatives re-use the DeepLearning library's per-decoder param structs. All types round-trip through JSON correctly.

### 0.2 — Extend AutoParamWidget with data-source combo support ✅ COMPLETED

Chose **Option (C) — Composite widget pattern**. Created `SlotBindingWidget`, a reusable composite that pairs a DataManager-aware data-source combo with an `AutoParamWidget` for the remaining config fields.

**Files created:**
- `DeepLearning_Widget/UI/Helpers/SlotBindingWidget.hpp` — Header declaring the composite widget.
- `DeepLearning_Widget/UI/Helpers/SlotBindingWidget.cpp` — Implementation.

**Public API:**
- `setDataSourceLabel(text)` — Customise the label for the data source combo.
- `setDataSourceTypes(type_hint)` — Set the DM_DataType filter via a string hint (delegates to `DataSourceComboHelper::typesFromHint`).
- `selectedDataSource()` — Returns the currently selected DataManager key.
- `setParameterSchema(schema)` — Forwards a `ParameterSchema` to the embedded `AutoParamWidget`.
- `parametersJson()` — Returns the current parameter JSON from the embedded `AutoParamWidget`.
- `setPostEditHook(hook)` — Forwards a `PostEditHook` to the embedded `AutoParamWidget`.

**Signals:** `dataSourceChanged(key)`, `parametersChanged()`, `bindingChanged()` (unified).

**Outcome:** A reusable pattern for combining DataManager-aware combos with auto-generated forms. AutoParamWidget remains generic.

### 0.3 — Create a `DataSourceComboHelper` utility ✅ COMPLETED

Extracted the combo population and refresh logic from `_populateDataSourceCombo` / `_refreshDataSourceCombos` into a standalone utility class.

**Files created:**
- `DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp` — Public API header.
- `DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.cpp` — Implementation with all concrete data type includes.
- `tests/WhiskerToolbox/DeepLearning_Widget/DataSourceComboHelper.test.cpp` — Catch2 tests.
- `tests/WhiskerToolbox/DeepLearning_Widget/CMakeLists.txt` — Test target definition.

**Public API:**
- `populateCombo(combo, types)` — One-shot combo population with `(None)` sentinel and selection preservation.
- `track(combo, types)` / `untrack(combo)` — Combo registration for auto-refresh.
- `refreshAll()` — Re-populates all tracked combos.
- `trackedCount()` — Number of currently tracked combos.
- Static `typesFromHint(string)` — Converts SlotAssembler type-hint strings (e.g. `"PointData"`, `"MediaData"`) to `std::vector<DM_DataType>`.

Auto-untracking on `QObject::destroyed` prevents dangling pointers.

**Outcome:** Data source combo management is centralized and testable.

---

## Phase 1 — Extract Sub-Widgets (UI Decomposition)

Break the monolithic properties widget into focused sub-widgets, each handling one slot type. The properties widget becomes a **thin coordinator** that creates sub-widgets and wires signals.

### 1.1 — `DynamicInputSlotWidget`

Handles one dynamic input slot panel (currently `_buildDynamicInputGroup`).

**Contents:**
- Data source combo (via `DataSourceComboHelper`)
- Encoder selection combo
- Mode combo (populated based on selected encoder)
- Gaussian sigma spin box (visible only in Heatmap mode)
- Time offset spin box

**Integration with AutoParamWidget:**
- The per-encoder param struct (e.g. `dl::Point2DEncoderParams`) is defined in the DeepLearning library (Phase 0.0.2). The widget uses `extractParameterSchema<dl::Point2DEncoderParams>()` directly.
- `ParameterUIHints` live in `channel_encoding/EncoderParamSchemas.hpp` (DeepLearning library).
- The widget wraps a data source combo + `AutoParamWidget` for the encoder-specific fields.
- `AutoParamWidget::parametersChanged` → emit `bindingChanged(SlotBindingData)`.

**Challenge:** The encoder param struct type varies by selected encoder (e.g., `Point2DEncoderParams` vs `ImageEncoderParams`). When the user switches encoder, the AutoParamWidget must be rebuilt with a different schema. Options:
  - Rebuild the `AutoParamWidget` with the new encoder's schema on encoder change.
  - Or use a single `DynamicInputParams` wrapper at the widget level that selects the appropriate encoder schema dynamically.

### 1.2 — `StaticInputSlotWidget`

Handles one non-sequence static input slot (currently the non-sequence branch of `_buildStaticInputGroup`).

**Contents:**
- Data source combo
- Capture mode combo (Relative/Absolute)
- Time offset spin box (Relative mode)
- Capture button + status label (Absolute mode)

**Integration with AutoParamWidget:**
- `CaptureMode` as `rfl::TaggedUnion<"capture_mode", RelativeModeParams, AbsoluteModeParams>`:
  ```cpp
  struct RelativeModeParams {
      int time_offset = 0;
  };
  struct AbsoluteModeParams {
      // captured_frame shown as read-only label, not a spin box
  };
  ```
- AutoParamWidget handles the variant switching; the capture button is added outside the form.

### 1.3 — `SequenceSlotWidget`

Handles one sequence input slot (currently `_buildStaticInputGroup` sequence branch + `_addSequenceEntryRow`).

This is the most complex sub-widget (~500 lines of the current implementation).

**Contents:**
- "Add Entry" / "Remove Entry" buttons
- Per-entry rows, each with:
  - Source type combo: "Static" vs. "Recurrent"
  - **Static path:** data source, mode, offset, capture
  - **Recurrent path:** output slot combo, init mode combo

**Integration with AutoParamWidget:**
- Each entry row uses `rfl::TaggedUnion<"source_type", StaticEntryParams, RecurrentEntryParams>`:
  ```cpp
  struct StaticEntryParams {
      std::string data_key;
      std::string capture_mode_str = "Relative";
      int time_offset = 0;
  };
  struct RecurrentEntryParams {
      std::string output_slot_name;
      std::string init_mode_str = "Zeros";
  };
  ```
- Each entry row is a small composite widget with AutoParamWidget + data source combo.
- When the variant switches (Static ↔ Recurrent), AutoParamWidget rebuilds the stacked form automatically.

### 1.4 — `OutputSlotWidget`

Handles one output slot panel (currently `_buildOutputGroup`).

**Contents:**
- Target DataManager key combo (editable)
- Decoder selection combo
- Threshold spin box
- Subpixel checkbox

**Integration with AutoParamWidget:**
- `OutputSlotParams` (widget-level, Phase 0.1) contains `data_key` + `DecoderVariant`.
- `DecoderVariant` is `rfl::TaggedUnion<"decoder", dl::MaskDecoderParams, dl::PointDecoderParams, dl::LineDecoderParams, dl::FeatureVectorDecoderParams>` — the alternatives are DeepLearning library types (Phase 0.0.1).
- Switching decoder in the combo auto-rebuilds the relevant parameter fields via AutoParamWidget's variant support.

### 1.5 — `RecurrentBindingWidget`

Handles one non-sequence recurrent binding panel (currently `_buildRecurrentInputGroup`).

**Contents:**
- Output slot combo (which output feeds back)
- Init mode combo
- Init mode-dependent fields (StaticCapture shows data_key + frame)

**Integration with AutoParamWidget:**
- `RecurrentInitMode` as variant:
  ```cpp
  struct ZerosInit {};
  struct StaticCaptureInit { std::string data_key; int frame = 0; };
  struct FirstOutputInit {};
  
  using InitVariant = rfl::TaggedUnion<"init_mode", ZerosInit, StaticCaptureInit, FirstOutputInit>;
  
  struct RecurrentBindingParams {
      std::string output_slot_name;
      InitVariant init = ZerosInit{};
  };
  ```

### 1.6 — `PostEncoderWidget`

Handles the post-encoder module section (currently `_buildPostEncoderSection` + `_enforcePostEncoderDecoderConsistency`).

**Contents:**
- Module type combo (None / GlobalAvgPool / SpatialPoint)
- Interpolation combo (SpatialPoint only)
- Point key combo (SpatialPoint only)

**Integration with AutoParamWidget:**
- `PostEncoderVariant` (widget-level, Phase 0.1) is `rfl::TaggedUnion<"module", NoPostEncoderParams, dl::GlobalAvgPoolModuleParams, dl::SpatialPointModuleParams>`.
- `dl::SpatialPointModuleParams` contains `InterpolationMode` enum (auto-detected by ParameterSchema). The `point_key` field (DataManager key) is widget-level and stays outside the auto-param form (composite widget pattern).
- Decoder consistency enforcement moves into a pure function called on module change.

### 1.7 — `EncoderShapeWidget`

Handles the encoder shape configuration section (currently `_buildEncoderShapeSection` + `_applyEncoderShape`).

**Contents:**
- Input height/width spin boxes
- Output shape text field
- Apply button

**Integration with AutoParamWidget:**
- Straightforward struct:
  ```cpp
  struct EncoderShapeParams {
      rfl::Validator<int, rfl::Minimum<1>> input_height = 224;
      rfl::Validator<int, rfl::Minimum<1>> input_width = 224;
      std::string output_shape;
  };
  ```
- AutoParamWidget generates the form; an external "Apply" button triggers `_applyEncoderShape`.

### Phase 1 Summary

After Phase 1, `DeepLearningPropertiesWidget` shrinks from ~2700 lines to a **thin coordinator** (~300-400 lines) that:
1. Creates the fixed UI (model combo, weights, frame/batch controls, run buttons).
2. In `_rebuildSlotPanels`, instantiates appropriate sub-widgets per slot.
3. Connects sub-widget `bindingChanged` signals to state updates.
4. Delegates run actions to Phase 2 controllers.

**New directory structure:**
```
DeepLearning/                              ← Library (non-Qt)
├── channel_decoding/
│   ├── ChannelDecoder.hpp                 ← DecoderContext + per-decoder param structs
│   ├── DecoderParamSchemas.hpp            ← NEW: ParameterUIHints for decoder params
│   ├── TensorToMask2D.hpp/cpp             ← Updated decode() signature
│   ├── TensorToPoint2D.hpp/cpp
│   ├── TensorToLine2D.hpp/cpp
│   └── TensorToFeatureVector.hpp/cpp
├── channel_encoding/
│   ├── ChannelEncoder.hpp                 ← EncoderContext + per-encoder param structs
│   ├── EncoderParamSchemas.hpp            ← NEW: ParameterUIHints for encoder params
│   ├── ImageEncoder.hpp/cpp
│   ├── Point2DEncoder.hpp/cpp
│   ├── Mask2DEncoder.hpp/cpp
│   └── Line2DEncoder.hpp/cpp
├── post_encoder/
│   ├── PostEncoderModuleFactory.hpp/cpp   ← Updated factory signature
│   ├── PostEncoderParamSchemas.hpp        ← NEW: ParameterUIHints for post-encoder params
│   ├── SpatialPointExtractModule.hpp/cpp
│   └── GlobalAvgPoolModule.hpp/cpp
└── CMakeLists.txt                         ← + ParameterSchema dependency

DeepLearning_Widget/                       ← Widget (Qt)
├── Core/
│   ├── BatchInferenceResult.hpp
│   ├── DeepLearningBindingData.hpp
│   ├── DeepLearningParamSchemas.hpp       ← Widget-level variants (compose library types)
│   ├── DeepLearningParamSchemas.cpp
│   ├── DeepLearningState.hpp
│   ├── DeepLearningState.cpp
│   ├── SlotAssembler.hpp
│   ├── SlotAssembler.cpp
│   └── WriteReservation.hpp
├── UI/
│   ├── DeepLearningPropertiesWidget.hpp   ← SLIMMED (coordinator)
│   ├── DeepLearningPropertiesWidget.cpp   ← SLIMMED (~300 lines)
│   ├── DeepLearningViewWidget.hpp
│   ├── DeepLearningViewWidget.cpp
│   ├── Slots/                             ← NEW subdirectory
│   │   ├── DynamicInputSlotWidget.hpp/cpp
│   │   ├── StaticInputSlotWidget.hpp/cpp
│   │   ├── SequenceSlotWidget.hpp/cpp
│   │   ├── OutputSlotWidget.hpp/cpp
│   │   ├── RecurrentBindingWidget.hpp/cpp
│   │   ├── PostEncoderWidget.hpp/cpp
│   │   └── EncoderShapeWidget.hpp/cpp
│   └── Helpers/                           ← NEW subdirectory
│       ├── DataSourceComboHelper.hpp/cpp
│       └── BindingConversion.hpp/cpp      (param struct ↔ binding data conversion)
├── DeepLearningWidgetRegistration.hpp
├── DeepLearningWidgetRegistration.cpp
└── CMakeLists.txt
```

---

## Phase 2 — Extract Inference Controllers (Logic Decomposition)

### 2.1 — `InferenceController`

Extract all inference orchestration out of the properties widget into a dedicated non-widget class.

```cpp
class InferenceController : public QObject {
    Q_OBJECT
public:
    InferenceController(SlotAssembler* assembler,
                        std::shared_ptr<DataManager> dm,
                        std::shared_ptr<DeepLearningState> state,
                        QObject* parent = nullptr);
    
    bool isRunning() const;
    
public slots:
    void runSingleFrame(int frame);
    void runBatch(int start_frame, int end_frame, int batch_size);
    void runRecurrentSequence(int start_frame, int end_frame);
    void cancel();

signals:
    void batchProgressChanged(int current, int total);
    void recurrentProgressChanged(int current, int total);
    void inferenceFinished(bool success, QString error);
    void runningChanged(bool running);
};
```

**Absorbs:**
- `_onRunSingleFrame`, `_onRunBatch`, `_onRunRecurrentSequence`
- `_setBatchRunning`, `_onCancelBatch`, `_onBatchFinished`
- Worker thread lifecycle management
- `_onPredictCurrentFrame` logic (called with resolved frame index)

### 2.2 — `ResultProcessor`

Extract result merging and feature-vector accumulation into a dedicated class.

```cpp
class ResultProcessor : public QObject {
    Q_OBJECT
public:
    ResultProcessor(std::shared_ptr<DataManager> dm,
                    std::shared_ptr<WriteReservation> reservation,
                    QObject* parent = nullptr);
    
    /// Start periodic merge timer.
    void startMerging();
    
    /// Stop timer and flush remaining results.
    void stopAndFlush();
    
    /// Flush accumulated feature vectors to TensorData.
    void flushFeatureVectors();

signals:
    void resultsWritten(int count);
};
```

**Absorbs:**
- `_mergeResults`
- `_pending_feature_rows` accumulation
- `QTimer` lifecycle

### Phase 2 Summary

After Phase 2, the properties widget contains **zero inference logic**. It holds an `InferenceController*` and connects button clicks to controller slots. Progress signals flow from controller → view widget.

---

## Phase 3 — Extract State Synchronization

### 3.1 — `BindingConversion` (pure functions)

Move the bidirectional conversion between UI param structs and binding data structs into pure, testable functions:

```cpp
namespace dl::conversion {

SlotBindingData fromDynamicInputParams(
    std::string const& slot_name,
    std::string const& data_key,
    DynamicInputParams const& params);

DynamicInputParams toDynamicInputParams(SlotBindingData const& binding);

OutputBindingData fromOutputParams(
    std::string const& slot_name,
    OutputSlotParams const& params);

OutputSlotParams toOutputParams(OutputBindingData const& binding);

// ... similar for static, recurrent, post-encoder, encoder shape

} // namespace dl::conversion
```

**Benefit:** The `_syncBindingsFromUi` function (currently 270 lines of fragile `findChild` lookups) is eliminated entirely. Each sub-widget emits its typed params struct; the coordinator converts it to binding data and sets state.

### 3.2 — `ConstraintEnforcer` (pure functions)

Extract constraint logic that currently lives scattered across the widget:

```cpp
namespace dl::constraints {

/// Given the current bindings and model info, compute valid batch size range.
struct BatchSizeConstraint {
    int min = 1;
    int max = 9999;
    bool forced_by_recurrent = false;
};
BatchSizeConstraint computeBatchSizeConstraint(
    ModelDisplayInfo const& info,
    std::vector<RecurrentBindingData> const& recurrent);

/// Given post-encoder module type, return valid decoder IDs for each output slot.
std::vector<std::string> validDecodersForModule(
    std::string const& module_type,
    std::vector<std::string> const& all_decoders);

/// Enforce that output slots have compatible decoders for the current post-encoder.
std::vector<OutputBindingData> enforceDecoderConsistency(
    std::vector<OutputBindingData> const& bindings,
    std::string const& module_type);

} // namespace dl::constraints
```

**Benefit:** Constraint logic is unit-testable without any UI dependencies.

---

## Phase 4 — Test Coverage

### 4.1 — Unit tests for ParameterSchema extraction

**Library-level (Phase 0.0):**
- Verify `extractParameterSchema<dl::MaskDecoderParams>()` produces expected fields.
- Verify `extractParameterSchema<dl::Point2DEncoderParams>()` produces `mode` as `"enum"` with correct `allowed_values`.
- Verify `extractParameterSchema<dl::SpatialPointModuleParams>()` produces `interpolation` as `"enum"`.
- JSON round-trip: struct → `rfl::json::write` → `rfl::json::read` → struct for each per-component param.

**Widget-level (Phase 0.1):**
- Verify variant schemas for `DecoderVariant`, `PostEncoderVariant`, `RecurrentInitVariant`.
- Verify `extractParameterSchema<OutputSlotParams>()` produces `data_key` + `decoder` variant.
- Verify round-trip: struct → schema → AutoParamWidget → `toJson()` → struct.

### 4.2 — Unit tests for BindingConversion

- `DynamicInputParams` ↔ `SlotBindingData` round-trip.
- `OutputSlotParams` ↔ `OutputBindingData` round-trip, including variant switching.
- Edge cases: empty data keys, default values, missing fields.

### 4.3 — Unit tests for ConstraintEnforcer

- Batch size constraints with/without recurrent bindings.
- Decoder consistency with each post-encoder module type.
- Model-level batch mode interaction (`FixedBatch`, `DynamicBatch`, `RecurrentOnlyBatch`).

### 4.4 — Integration tests (optional, lower priority)

- Load a schema, set it on AutoParamWidget, modify values, verify `toJson()` output.
- Verify sub-widget signal wiring (slot widget emits → coordinator updates state).

---

## Phase 5 — Polish and Documentation

### 5.1 — Developer documentation

- Document the new directory structure and data flow in `docs/developer/DeepLearning_Widget/`.
- Update the architecture description in `.github/copilot-instructions.md` if the widget boundary changes.

### 5.2 — State migration

- Ensure `DeepLearningState::fromJson()` can still deserialize workspaces saved before the refactoring. The serialized format should not change (binding data structs are unchanged).

### 5.3 — Remove dead code

- Delete any helper methods in the properties widget that are fully subsumed by sub-widgets.
- Remove `findChild`-based lookup patterns entirely.

---

## Execution Order and Dependencies

```
✅ Phase 0.0 (DeepLearning library param refactor — COMPLETE)
        │
        └── ✅ Phase 0.1 (widget-level binding param structs — COMPLETE)
                │
                ├── Phase 0.2 (AutoParamWidget extension)  
                │       │
                │       └── Phase 0.3 (DataSourceComboHelper)
                │               │
                │               ├── Phase 1.1 (DynamicInputSlotWidget)
                │               ├── Phase 1.2 (StaticInputSlotWidget)
                │               ├── Phase 1.3 (SequenceSlotWidget)
                │               ├── Phase 1.4 (OutputSlotWidget)
                │               ├── Phase 1.5 (RecurrentBindingWidget)
                │               ├── Phase 1.6 (PostEncoderWidget)
                │               └── Phase 1.7 (EncoderShapeWidget)
                │
                ├── Phase 2.1 (InferenceController)     ← can start after Phase 0.0
                ├── Phase 2.2 (ResultProcessor)
                │
                ├── Phase 3.1 (BindingConversion)       ← after Phase 1 sub-widgets exist
                └── Phase 3.2 (ConstraintEnforcer)      ← can start after Phase 0.0

Phase 4 (Tests)      ← after each phase, incrementally
Phase 5 (Polish)     ← after all phases complete
```

### Recommended implementation order (next steps):

1. **Phase 0.1** — Widget-level binding param structs (compose library types into variants)
2. **Phase 0.3** — `DataSourceComboHelper` (quick win, no AutoParamWidget dependency)
3. **Phase 3.2** — `ConstraintEnforcer` pure functions (quick win, immediately testable)
4. **Phase 2.1** — `InferenceController` (large isolated chunk, biggest maintainability gain)
5. **Phase 2.2** — `ResultProcessor` (pairs with InferenceController)
6. **Phase 0.2** — Evaluate AutoParamWidget extension needs
7. **Phase 1.7** — `EncoderShapeWidget` (simplest sub-widget, good pilot)
8. **Phase 1.4** — `OutputSlotWidget` (medium complexity, exercises variant support)
9. **Phase 1.6** — `PostEncoderWidget` (medium complexity, exercises variant support)
10. **Phase 1.1** — `DynamicInputSlotWidget` (exercises dependent-field pattern)
11. **Phase 1.5** — `RecurrentBindingWidget`
12. **Phase 1.2** — `StaticInputSlotWidget`
13. **Phase 1.3** — `SequenceSlotWidget` (most complex, benefits from patterns established above)
14. **Phase 3.1** — `BindingConversion` pure functions
15. **Phase 4** — Fill in test gaps
16. **Phase 5** — Documentation and cleanup

---

## Risk Assessment

| Risk | Mitigation |
|---|---|
| **Dependent combo fields** (encoder → mode) | Each encoder has its own param struct with correct mode subset. Switching encoder rebuilds AutoParamWidget with the new schema. Pilot in Phase 1.1. |
| **Dynamic `allowed_values`** from registries | Populate at schema construction time. Registry contents are fixed after startup. |
| **DataManager key combos** need runtime refresh | `DataSourceComboHelper` stays outside AutoParamWidget. Composite widget pattern (Phase 0.2, Option C). |
| **State serialization breakage** | `DeepLearningBindingData` flat fields are preserved. Per-component param structs are transient. State format is unchanged. |
| **Variant JSON format mismatch** | `BindingConversion` functions handle the mapping. Tests in Phase 4.2 verify round-trips. |
| **Large diff size** | Each phase ships independently. Feature branches per phase. |

---

## Expected Outcomes

| Metric | Before | After |
|---|---|---|
| `DeepLearningPropertiesWidget.cpp` lines | 2716 | ~300-400 |
| Number of classes | 1 monolithic | ~12 focused |
| Lines covered by unit tests | 0 (UI only) | ~500+ (pure logic) |
| `findChild` lookup count | ~40 | 0 |
| Adding a new slot type | Edit monolith in 5+ places | Create one new sub-widget |
| Adding a new decoder | Edit monolithic `DecoderParams`, widget `_buildOutputGroup`, `_syncBindingsFromUi`, `_enforcePostEncoderDecoderConsistency` | Add per-decoder param struct in DeepLearning library + variant alternative in `DecoderVariant` |
| Adding a new encoder | Edit monolithic `EncoderParams`, widget `_modesForEncoder`, `_buildDynamicInputGroup` | Add per-encoder param struct in DeepLearning library; widget auto-generates form from schema |
| Decoder/encoder param ownership | Widget re-derives from monolithic struct | DeepLearning library owns; widget consumes via `extractParameterSchema<T>()` |
