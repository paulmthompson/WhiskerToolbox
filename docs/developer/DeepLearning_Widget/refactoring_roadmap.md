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

**Additional composite structs (to be added as Phase 1 sub-widgets are implemented):**

The following types compose library-level encoder params into a widget-level variant, and define per-slot-type composite structs. They will be added to `DeepLearningParamSchemas.hpp` incrementally as each Phase 1 sub-widget is built:

- `EncoderVariant` — `rfl::TaggedUnion<"encoder", dl::ImageEncoderParams, dl::Point2DEncoderParams, dl::Mask2DEncoderParams, dl::Line2DEncoderParams>` — which encoder to use for a dynamic input.
- `DynamicInputSlotParams` — `{ source, EncoderVariant, time_offset }` — full config for one dynamic input slot.
- `StaticInputSlotParams` — `{ source, CaptureModeVariant }` — full config for one non-sequence static input slot.
- `RecurrentBindingSlotParams` — `{ output_slot_name, RecurrentInitVariant }` — full config for one recurrent binding.
- `PostEncoderSlotParams` — `{ PostEncoderVariant, point_key }` — post-encoder module + SpatialPoint data key.

Each struct's `source`, `point_key`, or `output_slot_name` field is a `std::string` annotated with `dynamic_combo = true` in `ParameterUIHints`, enabling runtime combo population via the revised Phase 0.2 AutoParamWidget extensions.

### 0.2 — Extend AutoParamWidget with dynamic field support ✅ COMPLETED

**Design revision:** The original Phase 0.2 created `SlotBindingWidget` (composite QComboBox + AutoParamWidget). This is **superseded by a unified struct approach** where data source keys are regular `std::string` fields in the param struct, and AutoParamWidget gains minimal extensions for dynamic combo population and cross-field constraint enforcement.

**Rationale for change:**
- The composite pattern keeps the data source outside the serializable struct, fragmenting serialization and requiring manual wiring between two independent widgets.
- The DataSynthesizer pattern proves that a single struct → single AutoParamWidget works cleanly with variants (combo + stacked widget auto-generated from `rfl::TaggedUnion`).
- With the data source inside the struct, the entire slot config round-trips through JSON transparently — no separate marshaling needed.
- Cross-field constraints (e.g., encoder type restricts valid data sources, or data source type restricts valid encoders) can be handled via `parametersChanged` signal + targeted update calls, in the same spirit as the existing `PostEditHook`.

**`SlotBindingWidget` is deprecated.** It remains in the codebase temporarily but Phase 1 sub-widgets will not use it.

#### Task 0.2.1 — Add `dynamic_combo` flag to `ParameterFieldDescriptor`

Add two fields to `ParameterFieldDescriptor`:
```cpp
bool dynamic_combo = false;       ///< If true, create QComboBox even with empty allowed_values
bool include_none_sentinel = false; ///< If true, prepend "(None)" sentinel to combo
```

In `ParameterUIHints`, annotators set these flags for fields that will be populated at runtime:
```cpp
template<> struct ParameterUIHints<DynamicInputSlotParams> {
    static void annotate(ParameterSchema& schema) {
        if (auto* f = schema.field("source")) {
            f->display_name = "Data Source";
            f->dynamic_combo = true;
            f->include_none_sentinel = true;
        }
    }
};
```

AutoParamWidget's `buildFieldRow` checks `dynamic_combo` and creates a QComboBox (initially empty or with just the sentinel) instead of QLineEdit.

#### Task 0.2.2 — Add `updateAllowedValues()` to AutoParamWidget

```cpp
/// @brief Dynamically update the combo items for a string/enum field.
///
/// Finds the FieldRow matching field_name. If the field has a combo_box,
/// repopulates it with values while preserving the current selection.
/// If include_none_sentinel was set, "(None)" is prepended.
/// If the field is a QLineEdit (non-dynamic), this is a no-op.
///
/// Does not emit parametersChanged unless the selection actually changes.
void updateAllowedValues(std::string const& field_name,
                         std::vector<std::string> const& values);
```

Typical usage by a slot widget:
```cpp
// DM observer fires → refresh the "source" combo
auto keys = getKeysForTypes(*_dm, allInputTypes());
_auto_param_widget->updateAllowedValues("source", keys);
```

#### Task 0.2.3 — Add `updateVariantAlternatives()` to AutoParamWidget

```cpp
/// @brief Show or hide variant alternatives by tag name.
///
/// Finds the variant FieldRow matching field_name. Hides alternatives
/// whose tags are not in allowed_tags. If the currently selected
/// alternative is hidden, switches to the first visible one.
///
/// Use case: restrict encoder options based on selected data source type,
/// or restrict decoder options based on post-encoder module.
void updateVariantAlternatives(std::string const& field_name,
                               std::vector<std::string> const& allowed_tags);
```

Typical usage by a slot widget:
```cpp
// User changes source → restrict encoder to compatible types
connect(_auto_param_widget, &AutoParamWidget::parametersChanged, this, [this]() {
    auto json = _auto_param_widget->toJson();
    auto params = rfl::json::read<DynamicInputSlotParams>(json);
    if (!params) return;
    auto source_type = _dm->getDataType(params->source);
    auto valid_encoders = validEncodersForType(source_type);
    _auto_param_widget->updateVariantAlternatives("encoder", valid_encoders);
});
```

**Design alternative — direction of constraint:**
Two valid approaches for encoder ↔ source coupling:
1. **Source → Encoder** (shown above): User picks data source first, valid encoders are restricted automatically. Simple, but user must pick source before seeing encoder options.
2. **Encoder → Source**: User picks encoder type first, source combo is filtered to only show compatible DM keys. Equally valid — use `updateAllowedValues("source", filteredKeys)` instead.

The choice between (1) and (2) is a UX decision to be made during Phase 1.1 implementation. Both are supported by the same `updateAllowedValues` / `updateVariantAlternatives` infrastructure.

**Outcome:** AutoParamWidget supports DataManager-driven combos and cross-field constraints with three minimal, non-breaking additions. No AutoParamWidget consumers outside DeepLearning_Widget are affected.

**Files created/modified:**
- `src/ParameterSchema/ParameterSchema.hpp` — Added `dynamic_combo` and `include_none_sentinel` fields to `ParameterFieldDescriptor`.
- `src/WhiskerToolbox/AutoParamWidget/AutoParamWidget.hpp` — Added `updateAllowedValues()` and `updateVariantAlternatives()` public methods; extended `FieldRow` with `include_none_sentinel` and `variant_all_tags`.
- `src/WhiskerToolbox/AutoParamWidget/AutoParamWidget.cpp` — Implementation of dynamic combo creation, sentinel mapping in JSON round-trips, and the two new update methods. Variant combo items now store original stack index as item data (`Qt::UserRole`) to support filtered alternatives.
- `tests/WhiskerToolbox/AutoParamWidget/AutoParamWidget.test.cpp` — 15 test cases (31 assertions) covering dynamic combos, sentinel mapping, `updateAllowedValues`, `updateVariantAlternatives`, nested variant sub-rows, JSON round-trips, and descriptor defaults.
- `tests/WhiskerToolbox/AutoParamWidget/CMakeLists.txt` — Test target definition.
- `tests/WhiskerToolbox/CMakeLists.txt` — Added `AutoParamWidget` test subdirectory.

### 0.3 — Create a `DataSourceComboHelper` utility ✅ COMPLETED (scope narrowed)

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

**Scope narrowing (Phase 0.2 revision):** With the unified struct approach (revised Phase 0.2), AutoParamWidget now owns the combo lifecycle via `updateAllowedValues()`. The combo tracking/population methods (`populateCombo`, `track`, `refreshAll`) become less central. The key remaining utilities are:
- `keysForTypes()` — fetching filtered key lists from DataManager (used by slot widgets to feed `updateAllowedValues`)
- `typesFromHint()` — converting type-hint strings to `DM_DataType` vectors

The class may be simplified to a set of free functions in a later cleanup pass. The `SlotBindingWidget` composite that depended on it is deprecated (see Phase 0.2 revision).

**Outcome:** Data source key-fetching and type-mapping logic is centralized and testable.

---

## Phase 1 — Extract Sub-Widgets (UI Decomposition)

Break the monolithic properties widget into focused sub-widgets, each handling one slot type. The properties widget becomes a **thin coordinator** that creates sub-widgets and wires signals.

**Each sub-phase MUST:**
1. Create the new sub-widget class with its own param struct and AutoParamWidget form.
2. **Replace** the corresponding `_build*()` method call in `DeepLearningPropertiesWidget::_rebuildSlotPanels()` with instantiation of the new sub-widget.
3. **Update** `_syncBindingsFromUi()` to read binding data from the new sub-widget (via `params()` or `toSlotBindingData()`) instead of `findChild<>` searches.
4. **Update** `_refreshDataSourceCombos()` to delegate to the new sub-widget's `refreshDataSources()` instead of manually repopulating combos.
5. **Remove** the old `_build*()` method and any private helper methods that are now dead code.
6. Add tests for the new sub-widget.

### 1.1 — `DynamicInputSlotWidget` ✅ COMPLETED

Handles one dynamic input slot panel (replaces `_buildDynamicInputGroup`).

**UX decision (Phase 1.1):** Implemented the **Encoder → Source** constraint direction. When the user selects an encoder type, the source combo is filtered to only show DataManager keys matching the encoder's expected input type (e.g. selecting `Point2DEncoder` filters source to `PointData` keys). This was chosen because it gives immediate feedback about compatible data sources.

**Integration into DeepLearningPropertiesWidget:**
- `_rebuildSlotPanels()` now creates `DynamicInputSlotWidget` instances instead of calling `_buildDynamicInputGroup()`.
- `_syncBindingsFromUi()` calls `toSlotBindingData()` on each widget to produce `SlotBindingData` for state, replacing the old `findChild<QComboBox*>` code.
- `_refreshDataSourceCombos()` delegates to `widget->refreshDataSources()` for dynamic inputs.
- `_buildDynamicInputGroup()` and `_modesForEncoder()` were deleted as dead code.
- Widget pointers stored in `std::vector<DynamicInputSlotWidget*> _dynamic_input_widgets` (non-owning; owned by Qt widget tree).

**Param struct (unified):**
```cpp
using EncoderVariant = rfl::TaggedUnion<
    "encoder",
    dl::ImageEncoderParams,
    dl::Point2DEncoderParams,
    dl::Mask2DEncoderParams,
    dl::Line2DEncoderParams>;

struct DynamicInputSlotParams {
    std::string source;       ///< DataManager key — dynamic combo
    EncoderVariant encoder = dl::ImageEncoderParams{};
    int time_offset = 0;
};
```

**AutoParamWidget renders:**
- `source` → QComboBox (dynamic, populated via `updateAllowedValues` from DM observer)
- `encoder` → QComboBox (Image/Point2D/Mask2D/Line2D) + QStackedWidget with per-encoder sub-params (mode, gaussian_sigma, normalize — auto-generated from the library-level param structs)
- `time_offset` → QSpinBox

**Wiring:**
- DM observer callback → `widget->refreshDataSources()` → `_auto_param->updateAllowedValues("source", keys)`
- `parametersChanged` signal → encoder→source constraint enforcement + emit `bindingChanged()`

**Files created:**
- `DeepLearning_Widget/UI/Helpers/DynamicInputSlotWidget.hpp` — Public API header.
- `DeepLearning_Widget/UI/Helpers/DynamicInputSlotWidget.cpp` — Implementation: schema setup, encoder→source constraint, DM refresh.
- `tests/WhiskerToolbox/DeepLearning_Widget/DynamicInputSlotWidget.test.cpp` — Catch2 tests (construction, params round-trip, data source refresh, signal emission).

**Files modified:**
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp` — Added `EncoderVariant`, `DynamicInputSlotParams`, and `ParameterUIHints<DynamicInputSlotParams>`.
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp` — Added `ParameterUIHints<DynamicInputSlotParams>::annotate()` implementation.
- `DeepLearning_Widget/CMakeLists.txt` — Added the two new files to the source list.
- `tests/WhiskerToolbox/DeepLearning_Widget/CMakeLists.txt` — Added `test_dl_dynamic_input_slot` test target.
- `tests/DeepLearning/param_schemas/DeepLearningParamSchemas.test.cpp` — Added `EncoderVariant` and `DynamicInputSlotParams` schema extraction, JSON round-trip, and UIHints tests.

**Public API:**
- `params()` — Returns the current `DynamicInputSlotParams`.
- `setParams(DynamicInputSlotParams)` — Sets parameters and updates the UI.
- `refreshDataSources()` — Re-populates the source combo from DataManager.
- `slotName()` — Returns the bound slot name.
- `bindingChanged()` signal — Emitted on any parameter change.

**Key advantage over old design:** The encoder variant's sub-params (mode, gaussian_sigma, etc.) are auto-generated from the DeepLearning library's per-encoder param structs. Switching encoder in the combo auto-rebuilds the stacked form — no manual `findChild` or schema-swapping needed.

### 1.2 — `StaticInputSlotWidget` ✅ COMPLETED

Handles one non-sequence static input slot (currently the non-sequence branch of `_buildStaticInputGroup`).

**Integration into DeepLearningPropertiesWidget:** Replace the non-sequence branch of `_buildStaticInputGroup()` in `_rebuildSlotPanels()`. Update `_syncBindingsFromUi()` static input section and `_refreshDataSourceCombos()` static source combo refresh to delegate to the new widget. Phase 1.3 completed the extraction and deleted `_buildStaticInputGroup()`.

**Param struct (unified):**
```cpp
struct StaticInputSlotParams {
    std::string source;          ///< DataManager key — dynamic combo
    CaptureModeVariant capture_mode = RelativeCaptureParams{};
};
```

**AutoParamWidget renders:**
- `source` → QComboBox (dynamic, populated via `updateAllowedValues`)
- `capture_mode` → QComboBox (Relative/Absolute) + QStackedWidget:
  - **Relative** page: `time_offset` QSpinBox
  - **Absolute** page: `captured_frame` display (read-only or hidden)

**Additional UI (outside AutoParamWidget):**
- Capture button + status label for Absolute mode (imperative action, not a parameter)
- Button visibility toggled based on active variant via `parametersChanged` signal

**Wiring:**
- DM observer callback → `updateAllowedValues("source", keys)`
- `parametersChanged` → show/hide capture button based on active variant

**Integration into DeepLearningPropertiesWidget:**
- `_rebuildSlotPanels()` creates `StaticInputSlotWidget` instances for non-sequence static slots instead of calling the old branch of `_buildStaticInputGroup()`.
- `_syncBindingsFromUi()` calls `toStaticInputData()` on each widget, with a state fallback for `captured_frame` on freshly-rebuilt panels.
- `_refreshDataSourceCombos()` delegates to `widget->refreshDataSources()` for non-sequence static slots.
- `_loadModelIfReady()` calls `widget->setModelReady(ready)` on all static input widgets to enable/disable the capture button.
- `_onCaptureStaticInput()` now calls `widget->setCapturedStatus(frame, range)` / `widget->clearCapturedStatus()` to update the status display, replacing the old `_updateCaptureButtonState()` method which was deleted.
- The non-sequence branch of `_buildStaticInputGroup()` was removed; only the sequence branch remained until Phase 1.3 removed the whole method.
- Widget pointers stored in `std::vector<StaticInputSlotWidget*> _static_input_widgets` (non-owning; owned by Qt widget tree).

**Files created:**
- `DeepLearning_Widget/UI/Helpers/StaticInputSlotWidget.hpp` — Public API header.
- `DeepLearning_Widget/UI/Helpers/StaticInputSlotWidget.cpp` — Implementation: schema setup, capture row show/hide, capture status management, source invalidation.
- `tests/WhiskerToolbox/DeepLearning_Widget/StaticInputSlotWidget.test.cpp` — Catch2 tests (construction, params round-trip, data source refresh, capture status, signal validity).

**Files modified:**
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp` — Added `StaticInputSlotParams` struct and `ParameterUIHints<StaticInputSlotParams>` declaration.
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp` — Added `ParameterUIHints<StaticInputSlotParams>::annotate()` implementation.
- `DeepLearning_Widget/UI/DeepLearningPropertiesWidget.hpp` — Added `StaticInputSlotWidget` forward declaration, `_static_input_widgets` member; removed `_updateCaptureButtonState` declaration.
- `DeepLearning_Widget/UI/DeepLearningPropertiesWidget.cpp` — All integration changes above; deleted `_updateCaptureButtonState` method body.
- `DeepLearning_Widget/CMakeLists.txt` — Added the two new source files.
- `tests/WhiskerToolbox/DeepLearning_Widget/CMakeLists.txt` — Added `test_dl_static_input_slot` test target.

**Public API:**
- `params()` — Returns the current `StaticInputSlotParams`.
- `setParams(StaticInputSlotParams)` — Sets parameters and updates the UI.
- `refreshDataSources()` — Re-populates the source combo from DataManager.
- `slotName()` — Returns the bound slot name.
- `toStaticInputData()` — Converts current params to `StaticInputData` for state sync.
- `setCapturedStatus(frame, range)` — Updates the status label after successful capture.
- `clearCapturedStatus()` — Resets status label to "Not captured".
- `setModelReady(bool)` — Enables/disables the capture button.
- `bindingChanged()` signal — Emitted on any parameter change.
- `captureRequested(slot_name)` signal — Emitted when user clicks the capture button.
- `captureInvalidated(slot_name)` signal — Emitted when source changes (parent should clear assembler cache).

### 1.3 — `SequenceSlotWidget` ✅ COMPLETED

Handles one sequence input slot (replaces `_buildStaticInputGroup` sequence branch + `_addSequenceEntryRow`).

**Integration into DeepLearningPropertiesWidget:**
- `_rebuildSlotPanels()` creates `SequenceSlotWidget` instances for sequence slots instead of calling `_buildStaticInputGroup()`.
- `_syncBindingsFromUi()` calls `getStaticInputs()` and `getRecurrentBindings()` on each widget.
- `_refreshDataSourceCombos()` and output slot refresh delegate to `widget->refreshDataSources()` and `widget->refreshOutputSlots()`.
- `_buildStaticInputGroup()` and `_addSequenceEntryRow()` were deleted.
- `_loadModelIfReady()` calls `widget->setModelReady(ready)` on sequence widgets.
- `_onCaptureSequenceEntry()` calls `widget->setCapturedStatus()` / `widget->clearCapturedStatus()`.
- Widget pointers stored in `std::vector<SequenceSlotWidget*> _sequence_slot_widgets` (non-owning; owned by Qt widget tree).

**Param struct (unified, per-entry):**
```cpp
struct StaticSequenceEntryParams {
    std::string data_key;                      ///< DataManager key — dynamic combo
    std::string capture_mode_str = "Relative"; ///< "Relative" or "Absolute"
    int time_offset = 0;
};
struct RecurrentSequenceEntryParams {
    std::string output_slot_name;        ///< dynamic combo (model output slots)
    std::string init_mode_str = "Zeros";
};
using SequenceEntryVariant = rfl::TaggedUnion<
    "source_type", StaticSequenceEntryParams, RecurrentSequenceEntryParams>;

/// Wrapper for AutoParamWidget: variant must be a struct field (like encoder in
/// DynamicInputSlotParams), not the root type, so fromJson/toJson nesting works.
struct SequenceEntryParams {
    SequenceEntryVariant entry = StaticSequenceEntryParams{};
};
```

**AutoParamWidget renders (per entry row):**
- `entry` variant → QComboBox (Static/Recurrent) + QStackedWidget:
  - **Static** page: `data_key` (dynamic combo via `updateAllowedValues`), `capture_mode_str` (combo), `time_offset` (spin)
  - **Recurrent** page: `output_slot_name` (dynamic combo), `init_mode_str` (combo)

**Dynamic field population:**
- `data_key` populated from DM observer via `updateAllowedValues`; combos refreshed before `fromJson` in `setEntriesFromState` so values can be set.
- `output_slot_name` populated from model output slot names via `refreshOutputSlots`.

**Widget structure:** Each entry row is a separate AutoParamWidget with `SequenceEntryParams` schema. The SequenceSlotWidget manages a list of these, with add/remove buttons controlling the list size.

**Files created:**
- `DeepLearning_Widget/UI/Helpers/SequenceSlotWidget.hpp` — Public API header.
- `DeepLearning_Widget/UI/Helpers/SequenceSlotWidget.cpp` — Implementation: schema setup, add/remove rows, capture row show/hide, state sync.
- `tests/WhiskerToolbox/DeepLearning_Widget/SequenceSlotWidget.test.cpp` — Catch2 tests (construction, getStaticInputs/getRecurrentBindings, setEntriesFromState round-trip, capture signals).

**Files modified:**
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp` — Added `SequenceEntryParams` wrapper, `ParameterUIHints<SequenceEntryParams>` declaration; `data_key` and `output_slot_name` marked `dynamic_combo` and `include_none_sentinel`.
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp` — Added `ParameterUIHints<SequenceEntryParams>::annotate()` implementation.
- `DeepLearning_Widget/UI/DeepLearningPropertiesWidget.hpp` — Added `SequenceSlotWidget` forward declaration, `_sequence_slot_widgets` member.
- `DeepLearning_Widget/UI/DeepLearningPropertiesWidget.cpp` — All integration changes; deleted `_buildStaticInputGroup()`, `_addSequenceEntryRow()`.
- `DeepLearning_Widget/CMakeLists.txt` — Added SequenceSlotWidget source files.
- `tests/WhiskerToolbox/DeepLearning_Widget/CMakeLists.txt` — Added `test_dl_sequence_slot` test target.

**Public API:**
- `slotName()` — Returns the bound slot name.
- `getStaticInputs()` — Returns `StaticInputData` for each static entry.
- `getRecurrentBindings()` — Returns `RecurrentBindingData` for each recurrent entry.
- `setEntriesFromState(static_inputs, recurrent_bindings)` — Restores UI from saved state.
- `refreshDataSources()` — Re-populates data_key combos from DataManager.
- `refreshOutputSlots(names)` — Re-populates output_slot_name combos.
- `setModelReady(bool)` — Enables/disables capture buttons.
- `setCapturedStatus(memory_index, frame, range)` — Updates status after capture.
- `clearCapturedStatus(memory_index)` — Resets status.
- `bindingChanged()` signal — Emitted on any parameter change.
- `captureRequested(slot_name, memory_index)` signal — Emitted when user clicks capture.
- `captureInvalidated(slot_name, memory_index)` signal — Emitted when source changes.

### 1.4 — `OutputSlotWidget` ✅ COMPLETED

Handles one output slot panel (replaces `_buildOutputGroup`).

**Integration into DeepLearningPropertiesWidget:**
- `_rebuildSlotPanels()` creates `OutputSlotWidget` instances for each output slot instead of calling `_buildOutputGroup()`.
- `_syncBindingsFromUi()` calls `toOutputBindingData()` on each widget.
- `_refreshDataSourceCombos()` delegates to `widget->refreshDataSources()`.
- `_enforcePostEncoderDecoderConsistency()` calls `updateDecoderAlternatives()` and `refreshDataSources()` on each output widget.
- `_buildOutputGroup()` was deleted.
- State restored from `_state->outputBindings()` when rebuilding panels via `paramsFromBinding()`.
- Widget pointers stored in `std::vector<OutputSlotWidget*> _output_slot_widgets` (non-owning; owned by Qt widget tree).

**Param struct (unified):**
```cpp
struct OutputSlotParams {
    std::string data_key;                         ///< DataManager key for results (dynamic combo)
    DecoderVariant decoder = MaskDecoderParams{}; ///< Decoder configuration
};
```

**AutoParamWidget renders:**
- `data_key` → QComboBox (dynamic, populated via `updateAllowedValues` from DM; filtered by decoder output type)
- `decoder` variant → QComboBox (Mask/Point/Line/FeatureVector) + QStackedWidget with per-decoder sub-params (threshold, subpixel, etc.)

**Cross-field constraint:**
- Post-encoder module type restricts valid decoders — coordinator calls `updateDecoderAlternatives()` when post-encoder changes (spatial_dims_removed → FeatureVector only).

**Files created:**
- `DeepLearning_Widget/UI/Helpers/OutputSlotWidget.hpp` — Public API header.
- `DeepLearning_Widget/UI/Helpers/OutputSlotWidget.cpp` — Implementation: schema setup, decoder→target constraint, state restore.
- `tests/WhiskerToolbox/DeepLearning_Widget/OutputSlotWidget.test.cpp` — Catch2 tests (construction, params round-trip, toOutputBindingData, paramsFromBinding).

**Files modified:**
- `DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp` — Added `dynamic_combo` and `include_none_sentinel` for `data_key` in OutputSlotParams.
- `DeepLearning_Widget/UI/DeepLearningPropertiesWidget.hpp` — Added `OutputSlotWidget` forward declaration, `_output_slot_widgets` member; removed `_buildOutputGroup` declaration.
- `DeepLearning_Widget/UI/DeepLearningPropertiesWidget.cpp` — All integration changes; deleted `_buildOutputGroup()`.
- `DeepLearning_Widget/CMakeLists.txt` — Added OutputSlotWidget source files.
- `tests/WhiskerToolbox/DeepLearning_Widget/CMakeLists.txt` — Added `test_dl_output_slot` test target.

**Public API:**
- `params()` — Returns the current `OutputSlotParams`.
- `setParams(OutputSlotParams)` — Sets parameters and updates the UI.
- `refreshDataSources()` — Re-populates target combo from DataManager.
- `updateDecoderAlternatives(allowed_tags)` — Restricts visible decoder options (e.g. when post-encoder changes).
- `slotName()` — Returns the bound slot name.
- `toOutputBindingData()` — Converts current params to `OutputBindingData` for SlotAssembler.
- `paramsFromBinding(binding)` — Static helper to build `OutputSlotParams` from saved state.
- `bindingChanged()` signal — Emitted on any parameter change.

### 1.5 — `RecurrentBindingWidget`

Handles one non-sequence recurrent binding panel (currently `_buildRecurrentInputGroup`).

**Integration into DeepLearningPropertiesWidget:** Replace `_buildRecurrentInputGroup()` calls in `_rebuildSlotPanels()`. Update `_syncBindingsFromUi()` recurrent binding section. Delete `_buildRecurrentInputGroup()`.

**Param struct (unified):**
```cpp
struct RecurrentBindingSlotParams {
    std::string output_slot_name;                   ///< dynamic combo (model output slots)
    RecurrentInitVariant init = ZerosInitParams{};  ///< Init mode variant
};
```

**AutoParamWidget renders:**
- `output_slot_name` → QComboBox (dynamic, populated from model output slot names via `updateAllowedValues`)
- `init` variant → QComboBox (Zeros/StaticCapture/FirstOutput) + QStackedWidget:
  - **StaticCapture** page: `data_key` (dynamic combo via `updateAllowedValues`) + `frame` (spin box)
  - Other pages: empty (no sub-params)

**Dynamic field population:**
- `output_slot_name` from model info via `updateAllowedValues`
- `data_key` inside StaticCapture variant from DM observer

### 1.6 — `PostEncoderWidget`

Handles the post-encoder module section (currently `_buildPostEncoderSection` + `_enforcePostEncoderDecoderConsistency`).

**Integration into DeepLearningPropertiesWidget:** Replace `_buildPostEncoderSection()` call in `_rebuildSlotPanels()`. Move `_enforcePostEncoderDecoderConsistency()` logic into the widget or a shared constraint enforcer. Delete `_buildPostEncoderSection()` and the post-encoder combo refresh from `_refreshDataSourceCombos()`.

**Param struct (unified):**
```cpp
struct PostEncoderSlotParams {
    PostEncoderVariant module = NoPostEncoderParams{};
    std::string point_key;  ///< DataManager key for SpatialPoint — dynamic combo
};
```

**AutoParamWidget renders:**
- `module` variant → QComboBox (None/GlobalAvgPool/SpatialPoint) + QStackedWidget:
  - **SpatialPoint** page: `interpolation` enum combo (auto-generated from `dl::SpatialPointModuleParams`)
  - Other pages: empty or minimal
- `point_key` → QComboBox (dynamic, populated with Point-type DM keys via `updateAllowedValues`)

**Cross-field constraints:**
- `point_key` visibility/relevance depends on active variant (only needed for SpatialPoint) — handled via `PostEditHook` or `parametersChanged` signal
- Decoder consistency enforcement (restrict output decoders based on module type) moves into a pure function in `ConstraintEnforcer` (Phase 3.2), called by the coordinator when `PostEncoderSlotParams` changes

### 1.7 — `EncoderShapeWidget`

Handles the encoder shape configuration section (currently `_buildEncoderShapeSection` + `_applyEncoderShape`).

**Integration into DeepLearningPropertiesWidget:** Replace `_buildEncoderShapeSection()` call in `_rebuildSlotPanels()`. Move `_applyEncoderShape()` logic into the widget. Delete both methods.

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
│   ├── DeepLearningParamSchemas.hpp       ← Widget-level variants + composite slot structs
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
│       ├── DataSourceComboHelper.hpp/cpp  ← Key-fetching utility (scope narrowed)
│       ├── SlotBindingWidget.hpp/cpp      ← DEPRECATED (kept temporarily)
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
    DynamicInputSlotParams const& params);

DynamicInputSlotParams toDynamicInputParams(SlotBindingData const& binding);

OutputBindingData fromOutputParams(
    std::string const& slot_name,
    OutputSlotParams const& params);

OutputSlotParams toOutputParams(OutputBindingData const& binding);

// ... similar for static, recurrent, post-encoder, encoder shape

} // namespace dl::conversion
```

**Benefit:** The `_syncBindingsFromUi` function (currently 270 lines of fragile `findChild` lookups) is eliminated entirely. Each sub-widget emits its typed params struct (which includes `source` as a regular field); the coordinator converts it to binding data and sets state. No separate data-source marshaling needed.

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
                ├── ✅ Phase 0.2 (AutoParamWidget dynamic field extensions — COMPLETE)
                │       │
                │                       ├── Phase 1.1 (DynamicInputSlotWidget)
                │       ├── Phase 1.2 (StaticInputSlotWidget)
                │       ├── ✅ Phase 1.3 (SequenceSlotWidget)
                │       ├── ✅ Phase 1.4 (OutputSlotWidget)
                │       ├── Phase 1.5 (RecurrentBindingWidget)
                │       ├── Phase 1.6 (PostEncoderWidget)
                │       └── Phase 1.7 (EncoderShapeWidget)
                │
                ├── ✅ Phase 0.3 (DataSourceComboHelper — COMPLETE, scope narrowed)
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

1. ~~**Phase 0.2** — AutoParamWidget dynamic field extensions~~ ✅ **DONE**
2. **Phase 3.2** — `ConstraintEnforcer` pure functions (quick win, immediately testable, no UI dependency)
3. **Phase 2.1** — `InferenceController` (large isolated chunk, biggest maintainability gain)
4. **Phase 2.2** — `ResultProcessor` (pairs with InferenceController)
5. **Phase 1.7** — `EncoderShapeWidget` (simplest sub-widget, good pilot for the unified struct pattern)
6. ~~**Phase 1.4** — `OutputSlotWidget`~~ ✅ **DONE**
7. **Phase 1.6** — `PostEncoderWidget` (medium complexity, exercises variant + dynamic `point_key` combo)
8. **Phase 1.1** — `DynamicInputSlotWidget` (exercises full pattern: dynamic source combo + encoder variant + cross-field constraints)
9. **Phase 1.5** — `RecurrentBindingWidget`
10. **Phase 1.2** — `StaticInputSlotWidget`
11. ~~**Phase 1.3** — `SequenceSlotWidget`~~ ✅ **DONE**
12. **Phase 3.1** — `BindingConversion` pure functions
13. **Phase 4** — Fill in test gaps
14. **Phase 5** — Documentation, cleanup, delete deprecated `SlotBindingWidget`

---

## Risk Assessment

| Risk | Mitigation |
|---|---|
| **Dependent combo fields** (encoder → mode) | Each encoder has its own param struct with correct mode subset. Variant auto-generates stacked form — no manual schema swap needed. |
| **Dynamic `allowed_values`** from registries | Populate at schema construction time. Registry contents are fixed after startup. |
| **DataManager key combos** need runtime refresh | `updateAllowedValues()` on AutoParamWidget, driven by DM observer callbacks. `DataSourceComboHelper::keysForTypes()` provides the key lists. |
| **Cross-field constraints** (source type ↔ encoder, post-encoder ↔ decoder) | `updateVariantAlternatives()` restricts visible options. Direction of constraint (source→encoder vs encoder→source) is a UX decision per widget. |
| **Nested dynamic combos** (e.g., `data_key` inside a variant alternative like `StaticCaptureInitParams`) | `updateAllowedValues` works by field name; for nested variant fields, the slot widget may need to call update after variant switch. Pilot in Phase 1.5. |
| **State serialization breakage** | `DeepLearningBindingData` flat fields are preserved. Per-component param structs are transient. State format is unchanged. |
| **Variant JSON format mismatch** | `BindingConversion` functions handle the mapping. Tests in Phase 4.2 verify round-trips. |
| **`SlotBindingWidget` deprecation** | Already committed but unused by Phase 1 sub-widgets. Removed in Phase 5 cleanup. No breaking changes. |
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
