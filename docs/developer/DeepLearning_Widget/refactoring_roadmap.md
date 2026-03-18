# DeepLearning_Widget Refactoring Roadmap

## Current State (after Phase 2.1)

`DeepLearningPropertiesWidget` has been reduced from **2716 lines to ~1603 lines** (Phase 1) and further by Phase 2.1.
All seven slot-type sub-widgets are in `UI/Helpers/`. Inference orchestration is now in `InferenceController`.

| Responsibility | Methods still in widget |
|---|---|
| UI construction (fixed) | `_buildUi`, `_rebuildSlotPanels`, `_clearDynamicContent` |
| Boolean mask panel | `_buildBooleanMaskGroup`, `_populateDataSourceCombo` |
| State sync (UI → State) | `_syncBindingsFromUi` |
| Model selection + weights | `_populateModelCombo`, `_onModelComboChanged`, `_loadModelIfReady`, `_updateWeightsStatus` |
| Run button handlers | `_onRunSingleFrame`, `_onRunBatch`, `_onRunRecurrentSequence`, `_onPredictCurrentFrame` — thin wrappers that show dialogs, call `_syncBindingsFromUi`, then delegate to `InferenceController` |
| Result merging | In `InferenceController` (merge timer + `_pending_feature_rows`; Phase 2.2 will extract to `ResultProcessor`) |
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
│   ├── DeepLearningParamSchemas.hpp/cpp   ← widget-level variant + slot param structs
│   ├── DeepLearningState.hpp/cpp
│   ├── SlotAssembler.hpp/cpp              ← PIMPL firewall; all libtorch usage here
│   └── WriteReservation.hpp
└── UI/
    ├── DeepLearningPropertiesWidget.hpp/cpp  ← coordinator; delegates inference to InferenceController
    ├── DeepLearningViewWidget.hpp/cpp
    ├── InferenceController.hpp/cpp        ← Phase 2.1: owns BatchInferenceWorker, merge timer, result merging
    └── Helpers/
        ├── DataSourceComboHelper.hpp/cpp   ← keysForTypes(), typesFromHint() utilities
        ├── DynamicInputSlotWidget.hpp/cpp
        ├── EncoderShapeWidget.hpp/cpp
        ├── OutputSlotWidget.hpp/cpp
        ├── PostEncoderWidget.hpp/cpp
        ├── RecurrentBindingWidget.hpp/cpp
        ├── SequenceSlotWidget.hpp/cpp
        ├── SlotBindingWidget.hpp/cpp       ← DEPRECATED, pending deletion
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

All inference orchestration has been extracted into `InferenceController` (`UI/InferenceController.hpp/cpp`), which owns the worker thread lifecycle, merge timer, and result merging (including `_pending_feature_rows`). Phase 2.2 will extract the latter into `ResultProcessor`.

**Extracted from `DeepLearningPropertiesWidget`:**
- `_onRunSingleFrame`, `_onRunBatch`, `_onRunRecurrentSequence`, `_onPredictCurrentFrame` logic
- `_setBatchRunning`, `_onCancelBatch`, `_onBatchFinished`
- `_batch_worker` (`QThread*`), `_write_reservation`, `_merge_timer`
- `_mergeResults()` and `_pending_feature_rows` (to move to `ResultProcessor` in Phase 2.2)

**API (as implemented):**
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

### 2.2 — `ResultProcessor`

Extract result merging and feature-vector accumulation into a dedicated class.

**Absorbs from `DeepLearningPropertiesWidget`:**
- `_mergeResults()`
- `_pending_feature_rows` accumulation (`std::map<std::string, std::vector<...>>`)
- `_merge_timer` (`QTimer*`) lifecycle

```cpp
class ResultProcessor : public QObject {
    Q_OBJECT
public:
    ResultProcessor(std::shared_ptr<DataManager> dm,
                    std::shared_ptr<DeepLearningState> state,
                    QObject* parent = nullptr);
    void acceptResult(BatchInferenceResult const& result);
    void flushFeatureVectors();

signals:
    void resultsWritten(int count);
};
```

`InferenceController` holds a `ResultProcessor*` and calls `acceptResult()` / `flushFeatureVectors()` — no widget dependency.

### Open question for Phase 2

`_syncBindingsFromUi()` is currently called at the start of every inference run to push the current UI state into `DeepLearningState` before the assembler reads it. After Phase 2, `InferenceController` will call the assembler directly. Two options:
1. **Keep the pre-run sync**: Properties widget still calls `_syncBindingsFromUi()` before delegating to the controller.
2. **Live state**: Phase 3.1 converts `_syncBindingsFromUi` into live updates on `bindingChanged`, eliminating the "flush before run" pattern entirely.

Option 2 is cleaner but depends on Phase 3.1. Implementing Phase 2 with option 1 first is safe.

---

## Phase 3 — Extract State Synchronization

### 3.1 — `BindingConversion` (pure functions)

`_syncBindingsFromUi()` is ~270 lines that reads typed structs from each sub-widget and converts them into `DeepLearningState` binding data. Although the `findChild<>` lookups are gone, the conversion logic is still inlined in the widget and untested.

Extract to a pure function namespace:

```cpp
namespace dl::conversion {
    SlotBindingData fromDynamicInputParams(std::string const& slot_name, DynamicInputSlotParams const&);
    StaticInputData fromStaticInputParams(std::string const& slot_name, StaticInputSlotParams const&);
    OutputBindingData fromOutputParams(std::string const& slot_name, OutputSlotParams const&);
    RecurrentBindingData fromRecurrentParams(std::string const& slot_name, RecurrentBindingSlotParams const&);
    // ...and inverses for state → params restore
} // namespace dl::conversion
```

Once these exist, `_syncBindingsFromUi` becomes a short loop:
```cpp
for (auto* w : _dynamic_input_widgets)
    _state->setDynamicInput(dl::conversion::fromDynamicInputParams(w->slotName(), w->params()));
```

This also unlocks **live state updates**: connect sub-widget `bindingChanged` signals directly to conversion+state-set, removing the "flush before run" pattern.

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

- **Phase 3.1 tests** — `BindingConversion` round-trips: `DynamicInputParams ↔ SlotBindingData`, `OutputSlotParams ↔ OutputBindingData`, edge cases (empty keys, default values).
- **Phase 3.2 tests** — `ConstraintEnforcer`: batch size with/without recurrent bindings, decoder validity per module type.
- **Phase 2 tests** — `InferenceController` and `ResultProcessor` can be tested with a mock `SlotAssembler` or minimal DataManager.
- **DeepLearningParamSchemas** — schema extraction and JSON round-trips for all widget-level variant types (`EncoderVariant`, `DecoderVariant`, etc.).

---

## Phase 5 — Polish and Cleanup

- **Delete `SlotBindingWidget`** (`UI/Helpers/SlotBindingWidget.hpp/cpp`) — deprecated since Phase 0.2, never used by Phase 1 sub-widgets.
- **`_buildBooleanMaskGroup` / `_populateDataSourceCombo`** — decide whether to extract into a `BooleanMaskSlotWidget` (matching the sub-widget pattern) or leave inline. Boolean mask slots appear to be a minor type; the case for extraction is consistency, not complexity.
- **State migration** — Verify `DeepLearningState::fromJson()` deserializes workspaces saved before the refactoring. The serialized format should not change (binding data flat fields are preserved).
- **Developer documentation** — Update `docs/developer/DeepLearning_Widget/` with the final architecture.
- **Rename `UI/Helpers/` to `UI/Slots/`** (optional) — The original roadmap envisioned `UI/Slots/` for slot sub-widgets and `UI/Helpers/` for utilities. Currently both live in `UI/Helpers/`. This is a cosmetic rename; decide whether the distinction is worth the churn.

---

## Execution Order (Remaining)

```
Phase 2.1  InferenceController      ✅ DONE
Phase 2.2  ResultProcessor          — extract from InferenceController
Phase 3.1  BindingConversion        — eliminates _syncBindingsFromUi, enables live state
Phase 3.2  ConstraintEnforcer       — small, immediately testable
Phase 4    Fill test gaps           — after each phase above
Phase 5    Cleanup / docs           — final pass
```

---

## Expected Outcomes

| Metric | Before Phase 1 | After Phase 1 | After Phase 2.1 | After All Phases |
|---|---|---|---|---|
| `DeepLearningPropertiesWidget.cpp` lines | 2716 | ~1603 | ~1200 | ~300-400 |
| Number of classes | 1 monolithic | 9 focused | 10 (+InferenceController) | ~13 focused |
| Lines covered by unit tests | 0 | ~1,700 | ~1,700 | ~2,500+ |
| `findChild` lookup count | ~40 | ~0 | ~0 | 0 |
| Adding a new slot type | Edit monolith in 5+ places | Create one sub-widget | Create one sub-widget | Create one sub-widget |
| Pre-run state sync required | Yes (findChild flush) | Yes (`_syncBindingsFromUi`) | Yes (`_syncBindingsFromUi`) | No (live updates) |
