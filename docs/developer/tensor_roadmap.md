# Tensor Refactor — Remaining Roadmap

**Date:** 2026-03-21
**Source:** Consolidated from `tensor_data_refactor_proposal.md` (TensorData CRTP storage refactor) and `tensor_column_refactor_proposal.md` (Tensor Column API / pipeline-first builders).

---

## Completed Work Summary

Everything below is implemented, tested, and merged. Detailed design rationale lives in the two source proposals.

| Area | What Was Done |
|------|---------------|
| **DimensionDescriptor** | Named axes, shape/strides, column naming. Pure value type. |
| **RowDescriptor** | `TimeFrameIndex`, `Interval`, `Ordinal` row types with factories and per-row labeling. |
| **CRTP Storage Base** | `TensorStorageBase<Derived>` with row-major semantics, cache, contiguity checks. |
| **ArmadilloTensorStorage** | Default ≤3D backend. Column↔row-major translation. Zero-copy `arma::fmat`/`arma::fcube` access. |
| **DenseTensorStorage** | Flat `vector<float>` fallback for >3D. Row-major with precomputed strides. |
| **LibTorchTensorStorage** | `torch::Tensor` wrapper behind `TENSOR_BACKEND_LIBTORCH`. Device management, dtype conversion. |
| **LazyColumnTensorStorage** | Type-erased `ColumnProviderFn` per column. Per-column caching, invalidation, `materializeAll()`. |
| **TensorStorageWrapper** | Sean Parent type-erasure. `tryGetAs<T>()` for zero-copy backend access. `sharedStorage()` for future views. |
| **TensorData** | Full public API: factories (`createTimeSeries2D`, `createFromIntervals`, `createND`, `createFromArmadillo`, `createOrdinal2D`, `createFromLazyColumns`, `createFromTorch`), element access, backend conversion (`toArmadillo`, `toLibTorch`), `DataTraits`, observer integration. |
| **Compile flags** | Armadillo always linked. LibTorch opt-in per instance. Single build holds both. |
| **extractTimeIndices()** | Extracts `TimeIndexResult{indices, time_frame}` from any keyed data type via `DataTypeVariant`. |
| **ColumnRecipe** | `gather_type` removed — derived from `DataManager::getType()` at build time. `pipeline_json` is primary. |
| **ColumnConfigDialog** | Pipeline-first UX: always-visible JSON editor, source key combo, interval property group. |
| **Pattern A builders** | `buildPipelineColumnProvider()` — generic timestamp-row builder via `executePipeline()`. Works with any source type that has a pipeline producing float. |
| **Pattern B builders** | `buildIntervalPipelineProvider()` — generic interval-row builder via `gatherAndExecutePipeline()`. |
| **GatherPipelineExecutor** | `gatherAndExecutePipeline()`, `extractSingleFloat()`, `applyRangeReductionToOutput()` in `TransformsV2/core/`. |
| **buildProviderFromRecipe()** | Simplified dispatch: no per-type branching. Routes to Pattern A or Pattern B based on row type. |
| **Old builders deleted** | `buildDirectColumnProvider`, `buildTimeSeriesColumnProvider`, `buildIntervalReductionForAnalog/Events/Intervals`, `validateStepChainForFloats`, `buildElementChain`, `applyChainToFloat` — all removed. |
| **Validation helpers** | `dmTypeToContainerTypeIndex()`, `getStepNames()`, `pipelineProducesFloat()` — all in anonymous namespace of `TensorColumnBuilders.cpp`. |
| **IntervalReduction transforms** | `AnalogIntervalReduction`, `EventIntervalReduction`, `IntervalOverlapReduction` — binary container transforms producing `TensorData`. |
| **Phase 3.8 tests (Pattern A)** | Timestamp-row coverage for MaskData, LineData, PointData columns (valid data, missing timestamps, empty pipeline rejection). |
| **buildInvalidationWiringFn()** | Observer registration helper for lazy column invalidation. |

---

## Remaining Work

### 1. Interval-Row Support for Non-Analog Types (from Column Refactor Phase 3.8)

**Priority:** High — this is the last gap in the generic pipeline column system.

**Problem:** `buildIntervalPipelineProvider()` (Pattern B) works for `AnalogTimeSeries`, `DigitalEventSeries`, and `DigitalIntervalSeries`, but fails at runtime for `MaskData`, `LineData`, and `PointData`. Tests in `test_nonanalog_column_builders.test.cpp` document this as `[IntervalGap]`.

**Root causes:**
1. `GatherPipelineExecutor.cpp` does not include MaskData/LineData/PointData headers.
2. `CopyableTimeRangeDataType` concept is not satisfied for the gather path.
3. `element_type_of<T>` has no specialization for these types in the gather context.

**Work:**
- Add header includes and template instantiations for MaskData, LineData, PointData in `GatherPipelineExecutor.cpp`.
- Ensure `GatherResult<MaskData>`, `GatherResult<LineData>`, `GatherResult<PointData>` satisfy the required concepts.
- Convert the `[IntervalGap]` tests from "expects runtime error" to verifying correct float output.

**Files:** `src/TransformsV2/core/GatherPipelineExecutor.cpp`, `tests/TransformsV2/test_nonanalog_column_builders.test.cpp`

---

### 2. ViewTensorStorage — Zero-Copy Slicing (from TensorData Refactor Step 6)

**Priority:** Medium — enables efficient sub-tensor access without materialization.

**Problem:** No way to create a lightweight view into an existing tensor. All sub-tensor access currently requires copying.

**Design (documented in tensor_data_refactor_proposal.md §6.5):**

```
ViewTensorStorage : TensorStorageBase<ViewTensorStorage>
  - Contiguous slice: (source, axis, start, end)
  - Index-based selection: (source, axis, indices)
  - isContiguous: true only for outermost-axis contiguous slices
  - flatData: available when contiguous, throws otherwise
  - Owns shared_ptr<TensorStorageWrapper const> to keep source alive
```

**Work:**
- Implement `ViewTensorStorage` CRTP backend (`.hpp`/`.cpp`).
- Wire `TensorData` methods: `sliceRows(start, end)`, `selectRows(indices)`, `selectColumns(names)`, `sliceAxis(axis, start, end)`.
- Tests for contiguous and non-contiguous views, lifetime safety, materialization round-trip.

**Infrastructure already in place:** `TensorStorageWrapper::sharedStorage()` exists for ownership sharing.

**Files:** New `src/DataManager/Tensors/storage/ViewTensorStorage.hpp/.cpp`, modifications to `TensorData.hpp/.cpp`.

---

### 3. Multi-Output Transforms Producing TensorData (from TensorData Refactor Step 9)

**Priority:** Medium — enables Hilbert, spectrogram, and similar transforms to output named-column tensors natively.

**Problem:** Transforms like Hilbert (magnitude + phase) and spectrogram (time × frequency) currently either output separate `AnalogTimeSeries` objects or are not implemented as transforms at all.

**Work:**
- Register multi-output pipeline steps that produce `TensorData` with named columns (e.g., `{"magnitude", "phase"}` for Hilbert).
- Spectrogram transform producing a 2D `TensorData` (time × frequency) with frequency bin labels.
- Ensure `DataTypeVariant` and the transforms v2 type chain handle `TensorData` as a pipeline output.

**Note:** `AnalogHilbertPhase` exists but outputs `AnalogTimeSeries`, not `TensorData`.

---

### 4. Lazy Row Resolution (from Column Refactor Phase 5)

**Priority:** Low — deferred until the column pipeline system is stable.

**Problem:** `ColumnProviderFn` closures capture a snapshot of `row_times` or `intervals` at build time. If the row source grows after tensor creation, the snapshot does not update.

**Work:**
1. Change `ColumnProviderFn` closures to query `extractTimeIndices()` at materialization time rather than capturing a snapshot.
2. Add row-source invalidation that triggers tensor shape recalculation when the row count changes.
3. Ensure all columns are materialized against the same row snapshot in a single locked pass (already protected by `LazyColumnTensorStorage`'s mutex).

**Trade-off:** Changes the lazy evaluation contract. Requires careful handling to prevent inconsistent column lengths if rows change mid-materialization.

---

### 5. Observer Lifetime Management for Lazy Columns (from TensorData Refactor §18)

**Priority:** Low — currently managed at the DataManager level (data objects outlive their observers in practice).

**Problem:** When a source `ObserverData` is destroyed before the `TensorData`, the registered observer callback becomes a dangling call.

**Options:**
- (a) `weak_ptr` guards in callbacks
- (b) Explicit cleanup via stored `CallbackID`s
- (c) Accept current convention (DataManager lifetime guarantees)

**Decision needed.** No implementation required if option (c) is chosen.

---

### 6. TensorData Integration with ContainerTraits for Transforms V2 (from TensorData Refactor §18)

**Priority:** Low — `DataTraits` already provides `element_type = float`, `is_ragged = false`, `is_temporal = true`.

**Problem:** Tensor transforms may need shape-awareness (e.g., "apply PCA across columns"). The current `ContainerTraits` system does not express dimensionality or column structure.

**Work:** Investigate whether a `TensorContainerTraits` extension or a new transform category is needed. May be deferred indefinitely if shape-aware transforms are implemented as standalone functions rather than generic pipeline steps.

**See also:** `docs/developer/tensor_transforms_roadmap.md` — comprehensive roadmap for TensorData → TensorData transforms (PCA, t-SNE, etc.) via the MLCore + TransformsV2 hybrid architecture. Phases 1-2 of that roadmap deliver tensor container transforms and scatter plot interaction without requiring ContainerTraits changes.

---

## Future Considerations (Not Planned)

These ideas are documented in the source proposals but have no implementation plan:

| Idea | Source | Notes |
|------|--------|-------|
| **Row Transforms** | Column Refactor §4 | Pipeline on the row axis (e.g., filter timestamps, resample to 100 Hz). Would need `RowDefinition.pipeline_json`. |
| **EntityID Rows** | Column Refactor §4 | `RowDescriptor::fromEntityIds()` factory. One value per entity. New pipeline execution path. |
| **Dynamic Quick Pipeline Menu** | Column Refactor §4 | Populate `ColumnConfigDialog` quick-pipeline options from `RangeReductionRegistry` instead of hardcoding. |
| **IntervalReductionRegistry** | TensorData Refactor §17 Pattern B | Standalone registry of named gather+reduce compositions. Deferred — Pattern A is sufficient. |
| **MemoryMappedTensorStorage** | TensorData Refactor §3 | `mmap`'d binary file backend. Listed in architecture diagram, no design or plan. |

---

## Priority Summary

| # | Item | Priority | Complexity | Blocked By |
|---|------|----------|------------|------------|
| 1 | Interval-row support for MaskData/LineData/PointData | High | Medium | Nothing |
| 2 | ViewTensorStorage (zero-copy slicing) | Medium | Medium | Nothing |
| 3 | Multi-output transforms → TensorData | Medium | Medium | Nothing |
| 4 | Lazy row resolution | Low | High | Items 1-3 stable |
| 5 | Observer lifetime for lazy columns | Low | Low | Decision needed |
| 6 | TensorData ContainerTraits extension | Low | Low | Design needed |
