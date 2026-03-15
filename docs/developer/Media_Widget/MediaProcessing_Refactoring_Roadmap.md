# MediaProcessing → ParameterSchema + AutoParamWidget Refactoring Roadmap

## Progress Summary

| Phase | Status | Notes |
|-------|--------|-------|
| 1 | ✅ Complete | Option B implemented — `ProcessingOptionsSchema.hpp` in `MediaProcessingPipeline` |
| 2 | ✅ Complete | `active` removed from all option structs; `MagicEraserOptions` split |
| 3 | ✅ Complete | `MediaProcessingPipeline` library created and wired into build |
| 4 | ✅ Complete | 22 widget files deleted; `MediaProcessing_Widget` rewritten with generic registry loop |
| 5 | ✅ Complete | All special cases handled: contrast hook, median constraints, colormap gating, magic eraser composition, auto-enable |
| 6 | ⏳ Not started | Optional polish |

---

## Problem Summary

The `ProcessingOptions/` folder under `Media_Widget/UI/SubWidgets/MediaProcessing_Widget/` contains **8 hand-written Qt widgets** (24 files: `.hpp`, `.cpp`, `.ui` each) that follow an identical boilerplate pattern:

- Wrap a plain C++ options struct (e.g., `GammaOptions`) with a Qt form
- Manual signal-blocking `getOptions()`/`setOptions()` round-trips
- Hand-maintained `.ui` files defining spinboxes, checkboxes, combos
- Emit `optionsChanged(OptionsStruct)` signal

These widgets are **only consumed** by `MediaProcessing_Widget`, which itself contains ~800 lines of repetitive slot wiring and `_applyXxxFilter()` methods. The entire system could be replaced by reflect-cpp structs + `AutoParamWidget`.

### Goals

1. Use `ParameterSchema` / `AutoParamWidget` to auto-generate the processing option GUIs instead of maintaining hand-written widgets.
2. Move the processing wrappers out of `Media_Widget` into a standalone library that other widgets can reuse.

---

## Phase 1: Make ProcessingOptions structs reflect-cpp compatible ✅ COMPLETE

**Decision taken: Option B.**

**What was done:**
- `ProcessingOptions.hpp` kept as plain structs with no rfl dependency.
- `ParameterUIHints` specializations for all 8 structs created in `src/MediaProcessingPipeline/ProcessingOptionsSchema.hpp` (part of the Phase 3 library).
- `ColormapType` enum handled automatically by reflect-cpp's enumerator array support.
- Non-UI fields excluded via Phase 2 struct split (see below).

**Original tasks (for reference):**

1. **Add `rfl::Validator` constraints** to struct fields to encode UI ranges. For example:
   ```cpp
   struct GammaOptions {
       bool active{false};
       rfl::Validator<double, rfl::Minimum<0>, rfl::Maximum<10>> gamma{1.0};
   };
   ```
2. **Add `ParameterUIHints` specializations** for each struct to provide display names, tooltips, and field grouping. These live in a separate header so `ImageProcessing` doesn't need an `rfl` dependency — the hints can live in a new bridge header.
3. **Handle `ColormapType` enum** — this already works with reflect-cpp's `rfl::get_enumerator_array<T>()` since it's a plain `enum class`. AutoParamWidget will generate a `QComboBox` automatically.
4. **Exclude non-UI fields** — `MagicEraserOptions::mask`, `MagicEraserOptions::image_size`, and `MagicEraserOptions::drawing_mode` are runtime-only state, not user-editable parameters. These need to be split out (see Phase 2).

### Key Decision: Where does the `rfl` dependency live?

- The `ProcessingOptions.hpp` structs currently live in `ImageProcessing`, which has **no rfl dependency**.
- **Option A:** Add `rfl` dependency to `ImageProcessing` (simplest, but heavier).
- **Option B (recommended):** Keep the plain structs in `ImageProcessing` as-is. Create a companion header (e.g., `ProcessingOptionsSchema.hpp`) in the new library (Phase 3) that provides `extractParameterSchema` specializations and `ParameterUIHints` for each struct. This requires the structs to be simple aggregates, which they already are (except `ContrastOptions` with its methods — those can remain since reflect-cpp handles method-bearing structs fine as long as the fields are public).

---

## Phase 2: Separate UI-editable parameters from runtime state ✅ COMPLETE

**What was done:**
- `active` field removed from `ContrastOptions`, `GammaOptions`, `SharpenOptions`, `ClaheOptions`, `BilateralOptions`, `MedianOptions`, `ColormapOptions`.
- `MagicEraserOptions` split into:
  - `MagicEraserParams` — UI-editable: `brush_size`, `median_filter_size`
  - `MagicEraserState` — runtime: `drawing_mode`, `mask`, `image_size`
  - `MagicEraserOptions` retained as a combined struct with `fromParamsAndState()` factory for backward-compatibility with `ImageProcessing` functions.
- `MediaDisplayOptions` updated with 8 `bool XXX_active` fields at the struct level.
- All 8 processing option widgets updated with `isActive()`/`setActive()`/`activeChanged()` signal.
- `MediaProcessing_Widget` updated: `_applyXxxFilter(options, active)` signatures, `_onXxxActiveChanged()` slots, `_setupProcessingWidgets()` signal connections, `_loadProcessingChainFromMedia()` using `XXX_active`.
- `Media_Window.cpp` updated: `colormap_options.active` → `colormap_active`.
- `MaskDilationOptions` left unchanged (out of scope).

**Original tasks (for reference):**

| Struct | UI-Editable Fields | Runtime State |
|--------|-------------------|---------------|
| `ContrastOptions` | `alpha`, `beta`, `display_min`, `display_max` | — |
| `GammaOptions` | `gamma` | — |
| `SharpenOptions` | `sigma` | — |
| `ClaheOptions` | `grid_size`, `clip_limit` | — |
| `BilateralOptions` | `diameter`, `sigma_color`, `sigma_spatial` | — |
| `MedianOptions` | `kernel_size` | — |
| `ColormapOptions` | `colormap`, `alpha`, `normalize` | — |
| `MagicEraserOptions` | `brush_size`, `median_filter_size` | `drawing_mode`, `mask`, `image_size` |

### Tasks

1. Split `MagicEraserOptions` into `MagicEraserParams` (UI-editable: `brush_size`, `median_filter_size`) and `MagicEraserState` (runtime: `active`, `drawing_mode`, `mask`, `image_size`).
2. Consider whether the `active` field belongs in the schema or is managed externally (it's more of a "processing chain toggle" than a parameter). **Recommendation:** Remove `active` from all option structs and manage it at the processing chain level via a wrapper. AutoParamWidget doesn't generate "active" toggles — that's the collapsible section's job.

---

## Phase 3: Create a new `MediaProcessingPipeline` library ✅ COMPLETE

**What was done:**
- Created `src/MediaProcessingPipeline/` with:
  - `ProcessingOptionsSchema.hpp` — `ParameterUIHints` specializations for all 8 option structs.
  - `ProcessingStep.hpp` — descriptor struct (`display_name`, `chain_key`, `schema`, `apply`).
  - `ProcessingStepRegistry.hpp/.cpp` — singleton registry with `RegisterStep<T>` RAII helper.
  - `CMakeLists.txt` — static library aliased as `WhiskerToolbox::MediaProcessingPipeline`.
- 6 steps statically registered: Linear Transform, Gamma Correction, Image Sharpening, CLAHE, Bilateral Filter, Median Filter. Magic Eraser and Colormap are handled separately (state/rendering requirements).
- Wired into `src/CMakeLists.txt` after `DataSynthesizer`.
- Developer docs created at `docs/developer/MediaProcessingPipeline/index.qmd`.

**Original design (for reference):**

- `ProcessingStep.hpp` — Descriptor for a named processing step: display name, execution order, function pointer `void(cv::Mat&, nlohmann::json const&)`, and its `ParameterSchema`.
- `ProcessingStepRegistry.hpp` — Static registry (like `DataSynthesizer`/`TransformsV2` pattern) mapping step names → `ProcessingStep` descriptors.
- `ProcessingOptionsSchema.hpp` — The `ParameterUIHints` specializations and `extractParameterSchema` wrappers for each `*Options` struct.
- No Qt dependency; depends on `ImageProcessing`, `ParameterSchema`, and `rfl`.

### Registration Pattern

```cpp
// In ProcessingStepRegistry.cpp
static RegisterStep<ContrastOptions> reg_contrast{
    "Linear Transform",           // display name
    "1__lineartransform",         // chain key
    [](cv::Mat& m, ContrastOptions const& o) { ImageProcessing::linear_transform(m, o); }
};
```

This eliminates the per-filter `_applyXxxFilter()` methods in `MediaProcessing_Widget` — the widget can iterate the registry and apply steps generically.

### Dependencies

```
MediaProcessingPipeline → ImageProcessing, ParameterSchema, reflectcpp
```

---

## Phase 4: Replace ProcessingOptions widgets with AutoParamWidget ✅ COMPLETE

**What was done:**
- Deleted 22 files from `ProcessingOptions/` (7 widgets × 3 files each + `MagicEraserWidget.ui`). Only `MagicEraserWidget.hpp/.cpp` retained (slimmed to drawing controls only).
- Rewrote `MediaProcessing_Widget.hpp/.cpp` from ~902 lines of per-filter boilerplate to ~650 lines of generic registry-driven code.
- New `ProcessingSection` struct vector replaces 8 per-filter widget pointers, 8 Section pointers, and 8 per-filter slots.
- `_setupRegistrySections()` iterates `ProcessingStepRegistry::instance().steps()` and creates `AutoParamWidget` + `QCheckBox` + `Section` for each step.
- `_applyRegistryStep()` uses `ProcessingStepRegistry::findByKey()` to get the step's apply function.
- Updated `CMakeLists.txt`: removed 21 file references, added `AutoParamWidget` and `WhiskerToolbox::MediaProcessingPipeline` dependencies.

**Original goal:** Delete all 24 files in `ProcessingOptions/` and replace them with `AutoParamWidget` instances driven by the registry.

### New `MediaProcessing_Widget` Flow

```
For each step in ProcessingStepRegistry:
    1. Create a Section (collapsible) with the step's display name
    2. Add a QCheckBox for "active" toggle (replaces the per-struct `active` field)
    3. Create an AutoParamWidget, call setSchema(step.schema)
    4. Connect AutoParamWidget::parametersChanged → generic handler:
       - Serialize toJson() → rfl::json::read<OptionsType>() → apply to processing chain
    5. Connect the active checkbox → add/remove step from MediaData
```

### Estimated Code Reduction

- **Delete:** 8 × `.hpp` + 8 × `.cpp` + 8 × `.ui` = **24 files** (~1500 lines)
- **Delete:** ~400 lines of per-filter slot handlers and apply methods in `MediaProcessing_Widget.cpp`
- **Add:** ~100 lines of generic loop in `MediaProcessing_Widget.cpp`
- **Add:** ~150 lines for `MediaProcessingPipeline` library (registry + schemas)
- **Net reduction: ~1650 lines, consolidated into ~250 lines of generic code.**

---

## Phase 5: Handle special cases ✅ COMPLETE

**What was done:**
- **Contrast bidirectional**: Installed `PostEditHook` on the Linear Transform `AutoParamWidget` that calls `ContrastOptions::calculateAlphaBetaFromMinMax()` after any field change.
- **Median kernel constraints**: `_updateMedianKernelConstraints()` rebuilds the schema with adjusted `max_value` (21 for 8-bit grayscale, 5 otherwise) when media changes.
- **Colormap availability**: Section enabled/disabled based on `MediaData::DisplayFormat::Gray`; auto-unchecks active when non-grayscale.
- **Magic Eraser composition**: Separate section with `AutoParamWidget` for `MagicEraserParams` (brush_size, filter_size) + slimmed `MagicEraserWidget` for drawing mode toggle and mask clearing.
- **Auto-enable checkbox**: All sections auto-check their active checkbox when `parametersChanged` fires.

**Original description:** Some widgets have behavior that `AutoParamWidget` doesn't natively support. These need targeted solutions:

| Widget | Special Behavior | Solution |
|--------|-----------------|----------|
| `ContrastWidget` | Bidirectional alpha/beta ↔ min/max calculation | Use the `postEditHook` on `AutoParamWidget`. When `parametersChanged` fires, the hook recomputes derived fields and calls `fromJson()` to update the UI. Alternatively, keep `ContrastOptions::calculateAlphaBetaFromMinMax()` and apply it as a post-deserialization hook. |
| `MedianWidget` | `setKernelConstraints(bool is_8bit_grayscale)` — changes max range at runtime | Use `ParameterUIHints` to set default range; provide a `reconfigure` hook on the step that accepts media metadata and rebuilds the schema with updated constraints. `AutoParamWidget::setSchema()` already supports being called multiple times. |
| `ColormapWidget` | `setColormapEnabled(bool)` — disables entire section for non-grayscale | Handle at the Section level: hide/disable the section based on media format. This is orchestration logic, not widget logic. |
| `MagicEraserWidget` | Drawing mode toggle, mask accumulation, clear button | This widget has significant canvas interaction that `AutoParamWidget` cannot replace. **Keep a thin custom widget** for the drawing/mask management, but use `AutoParamWidget` for the parameter portion (brush size, filter size). Compose: `MagicEraserSection = AutoParamWidget(params) + DrawingControls(custom)`. |
| All widgets | Auto-enable checkbox when value changes | Implement at the Section level: connect `AutoParamWidget::parametersChanged` → auto-check active checkbox. |

---

## Phase 6: Update MediaDisplayOptions serialization

**Current:** `MediaDisplayOptions` stores each `*Options` struct with its `active` and `bool` fields.

**After refactoring:**
- Each options struct no longer contains `active` — that's managed by the processing chain presence.
- `MediaDisplayOptions` stores a `std::map<std::string, nlohmann::json>` (or equivalent) mapping step keys to their JSON-serialized parameters, plus a `std::set<std::string>` of active step keys.
- This makes the options future-proof: adding a new processing step to the registry automatically makes it available without modifying `MediaDisplayOptions`.

**Alternative (simpler, less invasive):** Keep the current struct-per-filter layout in `MediaDisplayOptions` for now. The structs still work — we're just generating UI from them differently. Defer the generic map approach to a future phase.

---

## Execution Order

| Phase | Dependency | Risk | Effort | Status |
|-------|------------|------|--------|--------|
| 1 | None | Low — additive changes to existing structs | Small | ✅ Complete |
| 2 | Phase 1 | Low — struct split, find-and-replace | Small | ✅ Complete |
| 3 | Phase 1 | Medium — new library, CMake wiring | Medium | ✅ Complete |
| 4 | Phase 3 | Medium — main widget rewrite, large diff | Large | ✅ Complete |
| 5 | Phase 4 | Medium — special-case handling | Medium | ✅ Complete |
| 6 | Phase 4 | Low — optional, can defer | Small | ⏳ Not started |

**Current status:** Phases 1–5 are complete. Phase 6 (generic `MediaDisplayOptions` serialization) is optional polish and deferred.

---

## What Stays, What Goes

| Component | Fate |
|-----------|------|
| `BilateralWidget.*` (3 files) | **Deleted** ✅ |
| `ClaheWidget.*` (3 files) | **Deleted** ✅ |
| `ContrastWidget.*` (3 files) | **Deleted** ✅ (replaced by AutoParamWidget + post-edit hook) |
| `GammaWidget.*` (3 files) | **Deleted** ✅ |
| `MedianWidget.*` (3 files) | **Deleted** ✅ |
| `SharpenWidget.*` (3 files) | **Deleted** ✅ |
| `ColormapWidget.*` (3 files) | **Deleted** ✅ |
| `MagicEraserWidget.ui` | **Deleted** ✅ |
| `MagicEraserWidget.hpp/.cpp` | **Slimmed** ✅ — drawing controls only; params via AutoParamWidget |
| `MediaProcessing_Widget.*` | **Rewritten** ✅ — generic loop replaces per-filter boilerplate |
| `ProcessingOptions.hpp` | **Kept** in `ImageProcessing` (no rfl dependency added) |
| `ProcessingOptionsSchema.hpp` | **Created** in `MediaProcessingPipeline` lib |
| `ProcessingStepRegistry.*` | **Created** in `MediaProcessingPipeline` lib |
