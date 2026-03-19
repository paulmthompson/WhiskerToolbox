# DeepLearning_Widget Refactoring Roadmap

## Current State (after Phase 3.1)

`DeepLearningPropertiesWidget` has been reduced from **2716 lines to ~1603 lines** (Phase 1) and further by Phase 2.1.
All seven slot-type sub-widgets are in `UI/Helpers/`. Inference orchestration is now in `InferenceController`.

| Responsibility | Methods still in widget |
|---|---|
| UI construction (fixed) | `_buildUi`, `_rebuildSlotPanels`, `_clearDynamicContent` |
| Boolean mask panel | `_buildBooleanMaskGroup`, `_populateDataSourceCombo` |
| State sync (UI → State) | `_syncBindingsFromUi` (now uses `dl::conversion`) |
| Model selection + weights | `_populateModelCombo`, `_onModelComboChanged`, `_loadModelIfReady`, `_updateWeightsStatus` |
| Run button handlers | `_onRunSingleFrame`, `_onRunBatch`, `_onRunRecurrentSequence`, `_onPredictCurrentFrame` — thin wrappers that show dialogs, call `_syncBindingsFromUi`, then delegate to `InferenceController` |
| Result merging | In `ResultProcessor` (merge timer + `_pending_feature_rows`) |
| Constraints | `_updateBatchSizeConstraint`, `_enforcePostEncoderDecoderConsistency` |
| Static capture coordination | `_onCaptureStaticInput`, `_onCaptureSequenceEntry` |

### Current directory structure

```
DeepLearning_Widget/
├── CMakeLists.txt
├── DeepLearningWidgetRegistration.hpp/cpp
├── Core/
│   ├── BatchInferenceResult.hpp
│   ├── DeepLearningBindingData.hpp        ← flat fields for state serialization
│   ├── BindingConversion.hpp/cpp          ← pure params<->binding mapping
│   ├── DeepLearningParamSchemas.hpp/cpp   ← widget-level variant + slot param structs
│   ├── DeepLearningState.hpp/cpp
│   ├── SlotAssembler.hpp/cpp              ← PIMPL firewall; all libtorch usage here
│   ├── ResultProcessor.hpp/cpp
│   └── WriteReservation.hpp
└── UI/
    ├── DeepLearningPropertiesWidget.hpp/cpp  ← coordinator; delegates inference to InferenceController
    ├── DeepLearningViewWidget.hpp/cpp
    ├── InferenceController.hpp/cpp        ← Phase 2.1: owns BatchInferenceWorker and delegates result processing
    └── Helpers/
        ├── DataSourceComboHelper.hpp/cpp   ← keysForTypes(), typesFromHint() utilities
        ├── DynamicInputSlotWidget.hpp/cpp
        ├── EncoderShapeWidget.hpp/cpp
        ├── OutputSlotWidget.hpp/cpp
        ├── PostEncoderWidget.hpp/cpp
        ├── RecurrentBindingWidget.hpp/cpp
        ├── SequenceSlotWidget.hpp/cpp
        └── StaticInputSlotWidget.hpp/cpp
```

---

## Design Goals

1. **Separation of concerns**: Each distinct responsibility lives in its own class/file.
2. **Leverage ParameterSchema + AutoParamWidget**: Replace hand-built form construction and manual `findChild`-based marshaling with schema-driven auto-generated forms where possible.
3. **Testability**: Core logic (binding assembly, constraint enforcement, result processing) should be unit-testable without Qt widgets.
4. **Incremental delivery**: Each phase is independently shippable and testable. No big-bang rewrite.
5. **Preserve PIMPL firewall**: All libtorch usage continues to be confined to `SlotAssembler.cpp`.

---

## Phase 0 — Preparatory Infrastructure ✅ COMPLETED

### Summary

All four sub-phases completed:

- **0.0** — The DeepLearning library was refactored to use per-component param structs (`MaskDecoderParams`, `Point2DEncoderParams`, `SpatialPointModuleParams`, etc.) with runtime context separated into `DecoderContext` / `EncoderContext`. Three companion `*ParamSchemas.hpp` headers provide `ParameterUIHints` specializations. `DeepLearningBindingData` retains flat fields for backward-compatible serialization; `SlotAssembler` maps internally.

- **0.1** — Widget-level variant types defined in `DeepLearningParamSchemas.hpp`: `EncoderVariant`, `DecoderVariant`, `PostEncoderVariant`, `RecurrentInitVariant`, `SequenceEntryVariant`, and composite slot param structs (`DynamicInputSlotParams`, `StaticInputSlotParams`, `OutputSlotParams`, `PostEncoderSlotParams`, `EncoderShapeParams`).

- **0.2** — `AutoParamWidget` extended with `updateAllowedValues()`, `updateVariantAlternatives()`, and `dynamic_combo` / `include_none_sentinel` descriptor flags. `SlotBindingWidget` superseded (deprecated). 15 test cases added in `tests/WhiskerToolbox/AutoParamWidget/`.

- **0.3** — `DataSourceComboHelper` created with `keysForTypes()` and `typesFromHint()` utilities for feeding `updateAllowedValues()`. Combo-tracking methods less central after the unified-struct approach.

---

## Phase 1 — Extract Sub-Widgets ✅ COMPLETED

All seven slot-type sub-widgets extracted into `UI/Helpers/`. Each owns its AutoParamWidget form, emits a `bindingChanged()` signal, and has a dedicated test binary.

| Sub-widget | Key API | Test file |
|---|---|---|
| `DynamicInputSlotWidget` | `params()`, `setParams()`, `toSlotBindingData()`, `refreshDataSources()` | `DynamicInputSlotWidget.test.cpp` |
| `StaticInputSlotWidget` | `params()`, `setParams()`, `toStaticInputData()`, `setCapturedStatus()`, `setModelReady()` | `StaticInputSlotWidget.test.cpp` |
| `SequenceSlotWidget` | `getStaticInputs()`, `getRecurrentBindings()`, `setEntriesFromState()`, `refreshOutputSlots()` | `SequenceSlotWidget.test.cpp` |
| `OutputSlotWidget` | `params()`, `setParams()`, `toOutputBindingData()`, `updateDecoderAlternatives()` | `OutputSlotWidget.test.cpp` |
| `RecurrentBindingWidget` | `params()`, `setParams()`, `toRecurrentBindingData()`, `refreshOutputSlots()` | `RecurrentBindingWidget.test.cpp` |
| `PostEncoderWidget` | `params()`, `setParams()`, `moduleTypeForState()`, `paramsFromState()` | `PostEncoderWidget.test.cpp` |
| `EncoderShapeWidget` | `params()`, `setParams()`, `applyFromState()`, `applyEncoderShapeToAssembler()` | `EncoderShapeWidget.test.cpp` |

`_rebuildSlotPanels()` now instantiates sub-widgets instead of calling inline `_build*()` methods. `_syncBindingsFromUi()` reads typed structs from sub-widgets rather than `findChild<>` lookups, though it still exists as a coordinator method. `_buildBooleanMaskGroup()` and `_populateDataSourceCombo()` were **not** extracted (boolean masks are a minor slot type, handled inline).

**UX constraint direction implemented:** Encoder → Source (selecting encoder type filters DataManager keys to compatible types).

---

## Phase 2 — Extract Inference Controllers

### 2.1 — `InferenceController` ✅ COMPLETED

All inference orchestration has been extracted into `InferenceController` (`UI/InferenceController.hpp/cpp`), which owns the worker thread lifecycle and delegates incremental/final result processing to `ResultProcessor`.

**Extracted from `DeepLearningPropertiesWidget`:**
- `_onRunSingleFrame`, `_onRunBatch`, `_onRunRecurrentSequence`, `_onPredictCurrentFrame` logic
- `_setBatchRunning`, `_onCancelBatch`, `_onBatchFinished`
- `_batch_worker` (`QThread*`) and cancellation
- Delegation wiring to `ResultProcessor` for progressive geometry writes and feature-vector accumulation

**API (as implemented):**
```cpp
class InferenceController : public QObject {
    Q_OBJECT
public:
    InferenceController(SlotAssembler* assembler,\
                        std::shared_ptr<DataManager> dm,
                        std::shared_ptr<DeepLearningState> state,
                        QObject* parent = nullptr);
    bool isRunning() const;

public slots:
    void runSingleFrame(int frame);
    void runBatch(int start, int end, int batch_size);
    void runRecurrentSequence(int start, int frame_count);
    void cancel();

signals:
    void batchProgressChanged(int current, int total);
    void recurrentProgressChanged(int current, int total);
    void batchFinished(bool success, QString const& error_message);
    void runningChanged(bool running);
};
```

The properties widget holds an `InferenceController*`, connects button clicks (after dialogs) to controller slots, and forwards progress signals to `DeepLearningViewWidget`. Pre-run sync: widget calls `_syncBindingsFromUi()` before delegating (Option 1).

### 2.2 — `ResultProcessor` ✅ COMPLETED

Extract result merging and feature-vector accumulation into a dedicated class.

**Absorbs from `DeepLearningPropertiesWidget`:**
- `_pending_feature_rows` accumulation (`std::map<std::string, std::vector<std::pair<int, std::vector<float>>>>` for `std::vector<float>` feature outputs)
- `_merge_timer` (`QTimer*`) lifecycle
- Drains `WriteReservation` results, writing geometry immediately (Mask/Point/Line) and deferring TensorData feature-vector flush

```cpp
class ResultProcessor : public QObject {
    Q_OBJECT
public:
    ResultProcessor(std::shared_ptr<DataManager> dm,
                    std::shared_ptr<DeepLearningState> state,
                    QObject* parent = nullptr);
    void acceptResults(std::vector<FrameResult> results);
    void flushFeatureVectors();
    void clear();

    void setReservation(std::shared_ptr<WriteReservation> reservation);
    void startMergeTimer();
    void stopMergeTimer();

signals:
    void resultsWritten(int count);
};
```

`InferenceController` owns a `std::unique_ptr<ResultProcessor>` and uses it to start/stop the merge timer; `ResultProcessor` remains Qt-free regarding libtorch usage and writes decoded outputs into DataManager.

### Open question for Phase 2

`_syncBindingsFromUi()` is currently called at the start of every inference run to push the current UI state into `DeepLearningState` before the assembler reads it. After Phase 2, `InferenceController` will call the assembler directly. Two options:
1. **Keep the pre-run sync**: Properties widget still calls `_syncBindingsFromUi()` before delegating to the controller.
2. **Live state**: Phase 3.1 extracts conversion logic and keeps state live via `bindingChanged` updates; removing the redundant pre-run call can be done later.

Option 2 is cleaner but depends on Phase 3.1. Implementing Phase 2 with option 1 first is still safe because the pre-run sync is now a short conversion loop.

---

## Phase 3 — Extract State Synchronization

### 3.1 — `BindingConversion` ✅ COMPLETED

`_syncBindingsFromUi()` now delegates all params<->binding mapping to a
new pure conversion module:

* Added `Core/BindingConversion.hpp/.cpp` (`dl::conversion`) with pure
  conversion functions.
* Refactored these sub-widgets to delegate their `to*BindingData()` and
  `paramsFromBinding()` logic to `dl::conversion`:
  * `DynamicInputSlotWidget`
  * `StaticInputSlotWidget`
  * `OutputSlotWidget`
  * `SequenceSlotWidget` (both `getStaticInputs()` and `getRecurrentBindings()`)
  * `RecurrentBindingWidget`
* `_syncBindingsFromUi()` is now a short loop over sub-widgets calling
  conversion functions.

Live state updates remain enabled because `_rebuildSlotPanels()` already
connects each sub-widget’s `bindingChanged()` to `_syncBindingsFromUi()`.
Run handlers still call `_syncBindingsFromUi()` before delegating; the next
phase can remove this redundancy once Phase 2/2.2/3.2 changes stabilize.

### 3.2 — `ConstraintEnforcer` (pure functions)

Extract the batch-size and decoder-consistency constraint logic as testable free functions:

```cpp
namespace dl::constraints {
    struct BatchSizeConstraint { int min; int max; bool forced_by_recurrent; };
    BatchSizeConstraint computeBatchSizeConstraint(ModelDisplayInfo const&, std::vector<RecurrentBindingData> const&);
    std::vector<std::string> validDecodersForModule(std::string const& module_type);
} // namespace dl::constraints
```

`_updateBatchSizeConstraint()` and `_enforcePostEncoderDecoderConsistency()` each shrink to a single call plus a UI update.

---

## Phase 4 — Test Coverage

Sub-widget tests are in place (8 test binaries, ~1,700 lines of tests total). Remaining gaps:

- **Phase 3.1 tests** — `BindingConversion` round-trips: `DynamicInputSlotParams ↔ SlotBindingData`, `OutputSlotParams ↔ OutputBindingData`, edge cases (empty keys, default values).
- **Phase 3.2 tests** — `ConstraintEnforcer`: batch size with/without recurrent bindings, decoder validity per module type.
- **Phase 2 tests** — `InferenceController` and `ResultProcessor` can be tested with a mock `SlotAssembler` or minimal DataManager.
- **DeepLearningParamSchemas** — schema extraction and JSON round-trips for all widget-level variant types (`EncoderVariant`, `DecoderVariant`, etc.).

---

## Phase 5 — Polish and Cleanup

- **`_buildBooleanMaskGroup` / `_populateDataSourceCombo`** — decide whether to extract into a `BooleanMaskSlotWidget` (matching the sub-widget pattern) or leave inline. Boolean mask slots appear to be a minor type; the case for extraction is consistency, not complexity.
- **State migration** — Verify `DeepLearningState::fromJson()` deserializes workspaces saved before the refactoring. The serialized format should not change (binding data flat fields are preserved).
- **Developer documentation** — Update `docs/developer/DeepLearning_Widget/` with the final architecture.
- **Rename `UI/Helpers/` to `UI/Slots/`** (optional) — The original roadmap envisioned `UI/Slots/` for slot sub-widgets and `UI/Helpers/` for utilities. Currently both live in `UI/Helpers/`. This is a cosmetic rename; decide whether the distinction is worth the churn.

---

## Phase 6 — Bugs & Validation Fixes

Issues discovered during post-refactoring validation testing.

### 6.1 — Move `point_key` into `SpatialPointModuleParams` (UI parameter scoping) ✅ COMPLETED

**Problem:** `PostEncoderSlotParams` had `point_key` as a top-level field alongside the `PostEncoderVariant module` discriminant, making the "Point Key" combo always visible regardless of which module variant is selected. It is now correctly scoped inside `SpatialPointModuleParams`, so it only appears when Spatial Point Extraction is selected.

**Changes made:**
- `SpatialPointModuleParams` now has `point_key` field.
- `ParameterUIHints<SpatialPointModuleParams>` annotates `point_key` with `dynamic_combo = true` and `include_none_sentinel = true`.
- `PostEncoderParamSchemas.hpp` is included in `PostEncoderWidget.cpp` so the hints are applied during schema extraction.
- `PostEncoderSlotParams` reduced to just `PostEncoderVariant module`.
- `PostEncoderWidget::paramsFromState()` sets `point_key` inside `SpatialPointModuleParams`.
- `PostEncoderWidget::_applyToStateAndAssembler()` extracts `point_key` from the variant visitor.
- `AutoParamWidget::updateAllowedValues("point_key", ...)` already searched variant sub-rows, so `refreshDataSources()` required no change.
- All tests updated and passing.

**Original problem description:** `PostEncoderSlotParams` currently has `point_key` as a top-level field alongside the `PostEncoderVariant module` discriminant. This means the "Point Key" combo is always visible in the Post-Encoder section regardless of which module variant is selected (None, GlobalAvgPool, or SpatialPoint). It should only appear when SpatialPoint is the active variant.

**Root cause:** The field is defined in `PostEncoderSlotParams` (widget-level struct) rather than inside `SpatialPointModuleParams` (library-level struct).

**Affected files:**
- `src/DeepLearning/post_encoder/PostEncoderModuleFactory.hpp` — `SpatialPointModuleParams` struct
- `src/DeepLearning/post_encoder/PostEncoderParamSchemas.hpp` — `ParameterUIHints<SpatialPointModuleParams>`
- `src/WhiskerToolbox/DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp` — `PostEncoderSlotParams`
- `src/WhiskerToolbox/DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp` — `ParameterUIHints<PostEncoderSlotParams>`
- `src/WhiskerToolbox/DeepLearning_Widget/UI/Helpers/PostEncoderWidget.cpp` — `refreshDataSources()`, `_applyToStateAndAssembler()`
- `src/WhiskerToolbox/DeepLearning_Widget/Core/BindingConversion.hpp/cpp` — any conversion referencing `point_key`

**Steps:**

1. **Add `point_key` to `SpatialPointModuleParams`:**
   ```cpp
   struct SpatialPointModuleParams {
       InterpolationMode interpolation = InterpolationMode::Nearest;
       std::string point_key;  // DataManager key for PointData (dynamic combo)
   };
   ```
2. **Update `ParameterUIHints<SpatialPointModuleParams>`** to annotate `point_key` with `dynamic_combo = true` and `include_none_sentinel = true` (moved from `PostEncoderSlotParams` hints).
3. **Remove `point_key` from `PostEncoderSlotParams`** — it becomes:
   ```cpp
   struct PostEncoderSlotParams {
       PostEncoderVariant module = NoPostEncoderParams{};
   };
   ```
4. **Update `PostEncoderWidget::refreshDataSources()`** — call `updateAllowedValues()` targeting the nested `SpatialPointModuleParams.point_key` path within the variant. AutoParamWidget already supports nested field paths for variant children — verify this or extend if needed.
5. **Update `PostEncoderWidget::_applyToStateAndAssembler()`** — extract `point_key` from inside the variant visitor instead of from the top-level struct.
6. **Update `BindingConversion`** — adjust any `PostEncoderSlotParams ↔ binding` conversion that reads/writes `point_key`.
7. **Update tests** — `PostEncoderWidget.test.cpp` round-trip tests for the new nesting.

**Verification:** With SpatialPoint selected, the Point Key combo should appear. With None or GlobalAvgPool selected, it should be hidden.

---

### 6.2 — Gate weight loading on encoder shape being set ✅ COMPLETED

**Problem:** For `general_encoder` models, the user can browse and load weights before setting the encoder input/output shape. If the shape hasn't been configured, the loaded weights may be silently incompatible, or the model may initialize with wrong default dimensions (224×224 / 384×7×7). The weights file selector and load logic should be disabled until the shape has been explicitly applied.

**Root cause:** `_loadModelIfReady()` only gates on `!path.empty() && !_assembler->currentModelId().empty()`. It does not check whether shape has been configured for `general_encoder`.

**Affected files:**
- `src/WhiskerToolbox/DeepLearning_Widget/UI/DeepLearningPropertiesWidget.cpp` — `_loadModelIfReady()`, `_updateWeightsStatus()`
- `src/WhiskerToolbox/DeepLearning_Widget/Core/DeepLearningState.hpp/cpp` — may need a `shapeConfigured()` query
- `src/WhiskerToolbox/DeepLearning_Widget/UI/Helpers/EncoderShapeWidget.hpp/cpp` — `shapeApplied` signal already exists

**Steps:**

1. **Add shape-configured state tracking:** Add a `bool _shape_applied` flag (or derive it from `encoder_output_shape` being non-empty) in `DeepLearningState`. It becomes `true` after `EncoderShapeWidget::_onApplyClicked()` or when restoring from saved state with non-empty shape fields.
2. **Gate UI in `_updateWeightsStatus()`:**
   - If `model_id == "general_encoder"` and shape is not configured, disable the weights path edit and browse button. Show status: "Set encoder shape first".
   - For non-general-encoder models (which have fixed shapes), no gating needed.
3. **Gate `_loadModelIfReady()`:** Add an early return if `general_encoder` and shape not set.
4. **Connect `shapeApplied` signal** to re-evaluate `_loadModelIfReady()` so that once the user applies shape, weights auto-load if a path was previously entered.
5. **Update `_onModelComboChanged()`:** When switching to `general_encoder`, check whether saved state already has shape configured. If so, the gate is already satisfied (shape was restored from state).

**Verification:** Select `general_encoder` → weights browse button should be disabled → set shape and click Apply → weights browse becomes enabled → load weights succeeds.

---

### 6.3 — Validate loaded weights against configured shape ✅ COMPLETED

**Problem:** After loading weights, there is no proactive validation that the weights are compatible with the specified encoder input/output shape. Mismatches are only caught at inference time (via libtorch exceptions), which gives poor user feedback. A dummy forward pass at load time would catch mismatches early.

**Changes made:**
- `SlotAssembler::validateWeights()` added to `Core/SlotAssembler.hpp/.cpp`. Creates zero tensors for all input slots (batch_size=1), calls `forward()` with `torch::NoGradGuard`, checks all expected output slots are present and non-empty. Returns empty string on success or a diagnostic message on failure.
- `DeepLearningPropertiesWidget` gains a `_weights_validation_error` member (cleared on model change and at the start of each `_loadModelIfReady()` call).
- `_loadModelIfReady()` calls `validateWeights()` after a successful `loadWeights()` and stores the result.
- `_updateWeightsStatus()` now shows "✓ Loaded & validated" (green) on success or "⚠ Shape mismatch: <message>" (orange) on validation failure.

**Original problem description:** After loading weights, there is no proactive validation that the weights are compatible with the specified encoder input/output shape. Mismatches are only caught at inference time (via libtorch exceptions), which gives poor user feedback.

**Verification:** Load weights with correct shape → "✓ Loaded & validated". Load weights with wrong shape → "⚠ Shape mismatch: …". All existing inference paths are unchanged (validation is a read-only forward pass on zero tensors).

**Affected files:**
- `src/WhiskerToolbox/DeepLearning_Widget/Core/SlotAssembler.hpp/cpp` — new `validateWeights()` method
- `src/WhiskerToolbox/DeepLearning_Widget/UI/DeepLearningPropertiesWidget.cpp` — call validation after load

**Steps:**

1. **Add `SlotAssembler::validateWeights()` method** behind the PIMPL:
   ```cpp
   /// @brief Run a dummy forward pass to validate weight compatibility.
   /// @return Empty string on success, or error message on mismatch.
   /// @pre Model must be loaded and weights must be loaded.
   std::string validateWeights();
   ```
2. **Implementation** (in `SlotAssembler.cpp`):
   - Create a dummy input tensor matching each input slot's shape (zeros, batch_size=1).
   - Call `_impl->model->forward(dummy_inputs)` inside a try/catch.
   - On success, verify output tensor shapes match `model.outputSlots()`.
   - On failure, return the exception message formatted for display.
   - Use `torch::NoGradGuard` to avoid unnecessary gradient computation.
3. **Call from `_loadModelIfReady()`** after `_assembler->loadWeights(path)`:
   ```cpp
   auto validation_error = _assembler->validateWeights();
   if (!validation_error.empty()) {
       _weights_status_label->setText(
           tr("⚠ Shape mismatch: %1").arg(QString::fromStdString(validation_error)));
       _weights_status_label->setStyleSheet("color: orange;");
       // Weights are loaded but user is warned — don't block, but flag the issue
   }
   ```
4. **Update `_updateWeightsStatus()`** to show a validated-OK state (e.g., "✓ Loaded & validated") vs just "✓ Loaded".

**Verification:** Load weights with correct shape → "✓ Loaded & validated". Load weights with wrong output_shape → "⚠ Shape mismatch: expected [768,16,16], got [384,7,7]".

**Note:** This depends on 6.2 (shape is set before weights load). Implement 6.2 first.

---

### 6.4 — BUG: Single-frame inference fails with GlobalAvgPool post-encoder ✅ COMPLETED

**Problem:** When using `GlobalAvgPoolModule` as the post-encoder, single-frame inference ("Run Frame", "Predict Current") with `TensorToFeatureVector` decoder fails silently, but batch inference works. Additionally, `runSingleFrame()` never calls `updateSpatialPoint()`, breaking spatial point extraction in single-frame mode.

**Root cause analysis (revised):**

Three distinct bugs were identified:

1. **`decodeOutputs()` TensorToFeatureVector branch silently drops results after first call.** The code only creates a `TensorData` if one doesn't already exist. On subsequent single-frame calls (or after a previous batch run created the key), the decoded feature vector is silently discarded. The batch path doesn't have this issue because `decodeOutputsToBuffer` always returns `FrameResult` objects, and `ResultProcessor::flushFeatureVectors()` always replaces the `TensorData` via `setData`.

2. **`runSingleFrame()` does not call `updateSpatialPoint()`.** Both `runBatchRange()` and `runBatchRangeOffline()` call `updateSpatialPoint()` before each forward pass when a spatial point post-encoder is configured. `runSingleFrame()` was missing this call, meaning spatial point extraction produced wrong results (default point) in single-frame mode.

3. **No rank guard for spatial decoders with rank-reduced tensors.** If a misconfiguration passes a 2D `[B, C]` tensor (after GlobalAvgPool) to a spatial decoder expecting 4D `[B, C, H, W]`, the decoder would crash. The UI constraint (`_enforcePostEncoderDecoderConsistency`) already prevents this in the UI, but no runtime guard existed.

**Changes made:**

- **`decodeOutputs()` TensorToFeatureVector branch**: Now always creates/replaces a 1-row `TensorData` via `setData`, instead of only writing when the key doesn't exist. This ensures every single-frame inference produces a visible result.
- **`runSingleFrame()`**: Added `updateSpatialPoint()` call before `assembleInputs()`, matching the pattern used in `runBatchRange()` and `runBatchRangeOffline()`.
- **`runRecurrentSequence()`**: Added `updateSpatialPoint()` call before each frame's `assembleInputs()`, for consistency with the batch paths.
- **Rank guard in `decodeOutputs()` and `decodeOutputsToBuffer()`**: Both functions now check `tensor.dim() < 4` before dispatching to spatial decoders (`TensorToMask2D`, `TensorToPoint2D`, `TensorToLine2D`). If the tensor rank is insufficient, the decoder is skipped with a diagnostic message to `stderr`.
- **Pre-existing test fix**: Updated `DeepLearningParamSchemas.test.cpp` to expect 2 fields in `SpatialPointModuleParams` (was checking for 1 after Phase 6.1 added `point_key`).

**Affected files:**
- `src/WhiskerToolbox/DeepLearning_Widget/Core/SlotAssembler.cpp` — `decodeOutputs()`, `decodeOutputsToBuffer()`, `runSingleFrame()`, `runRecurrentSequence()`
- `tests/DeepLearning/param_schemas/DeepLearningParamSchemas.test.cpp` — updated `SpatialPointModuleParams` field count

**Verification:** Single-frame inference with GlobalAvgPool + TensorToFeatureVector now produces correct results on every invocation. Spatial decoders with rank-reduced tensors are gracefully skipped with a diagnostic message.

---

## Phase 7 — Command Architecture Integration

Integrate deep learning inference into the Command architecture so that inference can be invoked from TriageSession pipelines, JSON command sequences, and programmatic workflows without the widget.

### Design Considerations

**Current state:** Inference is widget-driven — `DeepLearningPropertiesWidget` orchestrates `InferenceController` which calls `SlotAssembler`. All configuration (model selection, shape, weights, slot bindings, decoders) lives in `DeepLearningState`, which is a widget-level concern.

**Goal:** A `RunInference` command that can be fully specified in JSON and executed headlessly via `CommandContext`, independent of any widget.

**Challenges:**

| Challenge | Approach |
|---|---|
| **SlotAssembler lifetime** | Create inside `execute()`, destroy on return. Stateless per-execution. |
| **libtorch dependency** | The `Commands` library currently has no torch dependency. Add an optional DL command module gated behind `ENABLE_DL_COMMANDS` CMake flag, similar to `ENABLE_BENCHMARK`. |
| **Model shape + weights** | Serialized as command params. `execute()` calls `loadModel → configureModelShape → loadWeights` in sequence. |
| **Slot bindings** | The existing `SlotBindingData`, `OutputBindingData`, `StaticInputData`, `RecurrentBindingData` structs are already reflect-cpp serializable — reuse directly as command params. |
| **Progress / cancellation** | Use `CommandContext::on_progress` for reporting. Cancellation is not yet in the command architecture; add an `std::atomic<bool>*` to `CommandContext` in the future. |
| **Post-encoder Module** | Serialized as a string (`"none"`, `"global_avg_pool"`, `"spatial_point"`) + params. Applied after model load. |
| **Output handling** | Direct writes to DataManager via decoders (same as `runBatchRange`). No `ResultProcessor` or `WriteReservation` needed (synchronous execution). |

### 7.1 — `RunInference` Command (Batch Mode)

**Params struct:**
```cpp
struct RunInferenceParams {
    std::string model_id;                              // e.g. "general_encoder"
    std::string weights_path;                          // filesystem path to .pt/.pt2
    std::optional<int> input_height;                   // for general_encoder
    std::optional<int> input_width;                    // for general_encoder
    std::optional<std::string> output_shape;           // e.g. "768,16,16"
    std::string post_encoder_module = "none";          // "none"|"global_avg_pool"|"spatial_point"
    std::optional<std::string> post_encoder_point_key; // for spatial_point
    std::optional<std::string> interpolation;          // "nearest"|"bilinear"
    int start_frame = 0;
    int end_frame = 0;
    std::vector<SlotBindingData> input_bindings;
    std::vector<StaticInputData> static_inputs;
    std::vector<OutputBindingData> output_bindings;
};
```

**Steps:**

1. **Create `src/Commands/DL/RunInference.hpp/cpp`** with the params struct and `ICommand` implementation.
2. **Gated CMake integration:** Add `ENABLE_DL_COMMANDS` option. When ON, add `src/Commands/DL/` to the build. Link `Commands` against `SlotAssembler` (which links libtorch via PIMPL).
3. **`execute()` implementation:**
   ```
   a. Create SlotAssembler
   b. loadModel(model_id)
   c. If general_encoder: configureModelShape(height, width, output_shape)
   d. configurePostEncoderModule(post_encoder_module, source_size, interpolation)
   e. loadWeights(weights_path)
   f. validateWeights() — fail early if mismatch (depends on 6.3)
   g. Resolve source_image_size from MediaData in input_bindings
   h. runBatchRange(dm, input_bindings, ..., start_frame, end_frame, source_size, on_progress)
   i. Return CommandResult::ok(affected_keys)
   ```
4. **Register in `CommandFactory`** (gated by `ENABLE_DL_COMMANDS`).
5. **Add `CommandInfo`** with schema from `extractParameterSchema<RunInferenceParams>()`.
6. **Variable substitution support:** `start_frame` and `end_frame` can use `${mark_frame}` and `${current_frame}` from TriageSession.

### 7.2 — `RunRecurrentInference` Command (Recurrent Mode)

Handled as a separate command because the params differ significantly (recurrent bindings, no batch range).

**Params struct:**
```cpp
struct RunRecurrentInferenceParams {
    std::string model_id;
    std::string weights_path;
    std::optional<int> input_height;
    std::optional<int> input_width;
    std::optional<std::string> output_shape;
    std::string post_encoder_module = "none";
    int start_frame = 0;
    int frame_count = 1;
    std::vector<SlotBindingData> input_bindings;
    std::vector<StaticInputData> static_inputs;
    std::vector<OutputBindingData> output_bindings;
    std::vector<RecurrentBindingData> recurrent_bindings;
};
```

**Steps:**
1. Create `src/Commands/DL/RunRecurrentInference.hpp/cpp`.
2. `execute()` calls `runRecurrentSequence()` synchronously.
3. Register in factory alongside `RunInference`.

### 7.3 — Pipeline Integration Examples

With the commands registered, TriageSession pipelines can use inference:

```json
{
  "commands": [
    {
      "name": "RunInference",
      "params": {
        "model_id": "general_encoder",
        "weights_path": "/models/convnext_encoder.pt2",
        "input_height": 256,
        "input_width": 256,
        "output_shape": "768,16,16",
        "post_encoder_module": "global_avg_pool",
        "start_frame": "${mark_frame}",
        "end_frame": "${current_frame}",
        "input_bindings": [
          {"slot_name": "image", "data_key": "video", "encoder_id": "ImageEncoder"}
        ],
        "output_bindings": [
          {"slot_name": "features", "data_key": "embeddings", "decoder_id": "TensorToFeatureVector"}
        ]
      }
    },
    {
      "name": "SaveData",
      "params": {
        "data_key": "embeddings",
        "format": "numpy",
        "output_path": "/results/embeddings.npy"
      }
    }
  ]
}
```

### 7.4 — Future: Async Command Execution

The current command architecture is synchronous. For long-running inference pipelines, future work could add:

- `std::atomic<bool> * cancel_flag` to `CommandContext`
- An `AsyncCommandRunner` that executes a command sequence on a background thread
- Progress aggregation across multiple commands in a sequence

This is out of scope for the initial integration but should be designed for when adding `RunInference`.

---

## Execution Order (Remaining)

```
Phase 3.2  ConstraintEnforcer       — small, immediately testable
Phase 4    Fill test gaps           — after each phase above
Phase 5    Cleanup / docs           — final pass
Phase 6.1  Move point_key into SpatialPointModuleParams ✅ COMPLETED
Phase 6.2  Gate weight loading on shape set             ✅ COMPLETED
Phase 6.3  Validate weights via dummy forward pass      ✅ COMPLETED
Phase 6.4  Fix single-frame + GlobalAvgPool decoder compat ✅ COMPLETED
Phase 7.1  RunInference command     — batch mode
Phase 7.2  RunRecurrentInference    — recurrent mode
Phase 7.3  Pipeline examples + docs
```

---

## Expected Outcomes

| Metric | Before Phase 1 | After Phase 1 | After Phase 2.1 | After All Phases |
|---|---|---|---|---|
| `DeepLearningPropertiesWidget.cpp` lines | 2716 | ~1603 | ~1200 | ~300-400 |
| Number of classes | 1 monolithic | 9 focused | 10 (+InferenceController) | ~15 focused |
| Lines covered by unit tests | 0 | ~1,700 | ~1,700 | ~3,000+ |
| `findChild` lookup count | ~40 | ~0 | ~0 | 0 |
| Adding a new slot type | Edit monolith in 5+ places | Create one sub-widget | Create one sub-widget | Create one sub-widget |
| Pre-run state sync required | Yes (findChild flush) | Yes (`_syncBindingsFromUi`) | Yes (`_syncBindingsFromUi`) | No (live updates) |
| Headless inference via commands | No | No | No | Yes (JSON pipelines) |
