# RunTransformsV2PipelineAtTime — Step-by-Step Implementation Guide

**Last updated:** 2026-05-26

This document is the implementation checklist for integrating TransformsV2 pipelines into the Command Architecture. Follow phases in order; each phase should build and pass `test_commands` (or targeted `-R` filters) before proceeding.

**Related docs:** [run_transforms_v2_pipeline_at_time.qmd](run_transforms_v2_pipeline_at_time.qmd), [pipeline_library.qmd](../../transforms_v2/pipeline_library.qmd)

---

## Prerequisites

- Pipeline library complete (`PipelineLibrary.hpp`, widget save/load).
- `CommandContext::config_dir` and `TriageSession::commit(..., config_dir)` wired (done).
- Understand schema split: `CommandSequenceDescriptor` (Triage) vs `PipelineDescriptor` (TransformsV2).

---

## Phase 1 — Core command (Qt-free)

**Goal:** `RunTransformsV2PipelineAtTime` runs via `pipeline_path` or `pipeline_id` + `config_dir`; no Triage UI changes yet.

### Step 1.1 — `PipelinePathResolver`

| File | Action |
|------|--------|
| `src/Commands/detail/PipelinePathResolver.hpp` | Declare `resolvePipelineFilePath(params, ctx)` |
| `src/Commands/detail/PipelinePathResolver.cpp` | Implement resolution rules |

**Resolution order:**

1. `pipeline_path` set → use it (relative + `ctx.config_dir` → resolve under `defaultUserPipelineDirectory`).
2. Else `pipeline_id` set → `{library}/{id}.json` (requires `ctx.config_dir`).
3. Else error.

**Tests:** missing both; `pipeline_id` without `config_dir`; relative/absolute `pipeline_path`.

### Step 1.2 — `FramePipelineBridge`

| File | Action |
|------|--------|
| `src/Commands/detail/FramePipelineBridge.hpp` | `extractFrameVariant`, `mergeFrameIntoDataManager` |
| `src/Commands/detail/FramePipelineBridge.cpp` | Implement |

**Extract (zero-copy for ragged):**

- `LineData` / `MaskData` / `PointData`: collect `(time, entity_id, cref(data))` at frame via `getElementsInRange({t,t})`, then `createFromView`.
- `AnalogTimeSeries` / `DigitalEventSeries`: `createView(source, t, t)`.
- Hold source `shared_ptr` alive through `executePipeline`.

**Merge (v1):**

- Ragged types: `clearAtTime` if `replace_existing_at_time`, then `addEntryAtTime` for each result entry at target frame.
- `create_output_if_missing`: empty series + `setData`, then merge.
- Analog/Digital merge: defer or return clear error until API is confirmed.

**Reject:** `PipelineDescriptor` with `range_reduction` set.

### Step 1.3 — Command class

| File | Action |
|------|--------|
| `src/Commands/RunTransformsV2PipelineAtTime.hpp` | Params struct + `ICommand` |
| `src/Commands/RunTransformsV2PipelineAtTime.cpp` | `execute()` |

**Flow:**

1. Validate `ctx.data_manager`.
2. Resolve pipeline path → `loadPipelineDescriptorFromFile` → reject `range_reduction`.
3. `loadPipelineFromJson` → `TransformPipeline`.
4. `getDataVariant(input_key)` → `extractFrameVariant` → `executePipeline`.
5. `mergeFrameIntoDataManager` → return `CommandResult::ok({output_key})`.

### Step 1.4 — CMake & registration

- `src/Commands/CMakeLists.txt`: list new sources; `target_link_libraries(Commands PRIVATE TransformsV2)`.
- `register_core_commands.cpp`: register command (already stubbed if present).
- `CommandUIHints.hpp`: tooltips + `dynamic_combo` on keys / `pipeline_id`.
- `tests/Commands/CMakeLists.txt`: add test file + **whole-archive** `TransformsV2`.
- `src/DataManagerPipelineRunner/CMakeLists.txt`: whole-archive `TransformsV2` (already stubbed if present).

### Step 1.5 — Tests

| File | Cases |
|------|--------|
| `src/Commands/RunTransformsV2PipelineAtTime.test.cpp` | `${current_frame}` substitution; `pipeline_id` + temp config dir; LineData frame merge; no extra DM keys; error paths |

**Pipeline fixture:** `ResampleLine` on one `LineData` frame (see test file).

### Step 1.6 — Developer doc stub

- `docs/developer/DataManager/Commands/run_transforms_v2_pipeline_at_time.qmd` — params, examples, constraints.

### Phase 1 verification

```bash
cmake --build --preset linux-clang-release > build_log.txt 2>&1
ctest --preset linux-clang-release --output-on-failure -R "RunTransformsV2" > test_log.txt 2>&1
run-clang-tidy -fix -p out/build/Clang/Release src/Commands/RunTransformsV2PipelineAtTime.cpp ... > tidy_log.txt 2>&1
clang-format -i src/Commands/RunTransformsV2PipelineAtTime.cpp ...
```

---

## Phase 2 — Triage guided UI

**Goal:** Pick `pipeline_id` from library in `CommandRowWidget` / `GuidedPipelineEditor`.

### Step 2.1 — Pass `configDir`

- `TriageSessionProperties_Widget`: store `QString _config_dir`; pass to `commit(..., config_dir)`.
- `TriageSessionWidgetRegistration` / `mainwindow.cpp`: pass `StateManager::configDir()` at widget construction.

### Step 2.2 — Populate `pipeline_id` combo

- `GuidedPipelineEditor`: accept `config_dir`.
- On row create for `RunTransformsV2PipelineAtTime`: clone schema, set `pipeline_id` `allowed_values` from `listPipelineFiles`.
- Optional: **Library…** → `PipelineLibraryDialog` pick-only.

### Step 2.3 — User guide stub

- `docs/user_guide/triage/run_v2_pipeline_on_frame.qmd`

---

## Phase 3 — Keymap & headless runner

### Step 3.1 — `KeymapCommandBridge`

- Add `std::optional<std::filesystem::path> config_dir` parameter.
- Wire from `KeymapManager` / `MainWindow`.

### Step 3.2 — `JsonPipelineRunner`

- Optional `--config-dir` or env var for `pipeline_id` resolution in CLI sequences.

---

## Phase 4 — Optional optimizations

- `RaggedTimeSeries::createTimeRangeView` using `ViewRaggedStorage` + aliasing `shared_ptr`.
- Full-series `RunTransformPipeline` command.
- Undo support for frame merge.
- `TensorData` / `MediaData` frame paths.

---

## Commit message template (Phase 1)

```
Add RunTransformsV2PipelineAtTime command for single-frame V2 pipelines

Bridge Command Architecture to PipelineLibrary JSON without registering
intermediate DataManager keys; extract one frame via lazy view storage,
execute executePipeline in memory, and merge only the final output.
```
