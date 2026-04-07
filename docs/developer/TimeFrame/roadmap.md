# TimeFrame Resampling & Upsampling Roadmap

## Progress Summary

*Last updated: 2026-04-07*

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 1:** Upsampled TimeFrame Creation | **Complete** | `createUpsampledTimeFrame()` utility + 6 test cases, 73 assertions |
| **Phase 2:** Pipeline Executor TimeKey Fix | **Complete** | `output_time_key` field + propagate input TimeKey (fixes hardcoded `"default"` bug) |
| **Phase 3:** Sinc Interpolation Transform | Not Started | Pure data transform: N samples → M samples. No TimeFrame awareness. |
| **Phase 4:** DataManager Widget Integration | Not Started | UI for creating upsampled TimeFrames interactively |
| **Phase 5:** Float TimeFrame | Not Started | Future: `std::vector<double>` for sub-clock-cycle precision |

---

## 1. Problem Statement

Neural data is often recorded at different sampling rates on a shared master clock.
A camera might sample at 500 Hz while electrophysiology runs at 30 kHz.
To precisely align these signals, the lower-rate data must be upsampled using
band-limited interpolation (sinc/Nyquist reconstruction).

Today, the system has no support for:

- **Creating upsampled TimeFrames** from existing ones
- **Assigning transform output to a different TimeKey** than the input
- **Sinc interpolation** as a data transform

Additionally, the pipeline executor has a latent bug: `storeOutputData()` hardcodes
`TimeKey("default")` instead of propagating the input's TimeKey.

### Design Principle: Separation of Concerns

**Transforms are pure data operations. TimeFrame management is the user's responsibility.**

A sinc interpolation transform takes N input samples and an upsampling factor, and
produces `(N-1) * factor + 1` output samples. It does not create, register, or
reference any TimeFrame. The user creates the target TimeFrame beforehand (via the
DataManager widget), and the pipeline executor validates that the output size matches
the target TimeFrame's entry count.

This separation has concrete benefits:

- **Multi-channel efficiency**: 4 tetrode channels sharing a TimeFrame are each
  upsampled independently, but all point at the same pre-created target TimeFrame.
  No redundant TimeFrame creation.
- **No DataManager coupling in transforms**: transforms remain testable as pure
  functions with no DataManager dependency.
- **Clear contract**: if the output sample count does not match the target TimeFrame
  size, the pipeline step fails with an explicit error.

### Example Workflow (4-Channel Tetrode)

```
1. User action (DataManager widget):
   "Create Upsampled TimeFrame"
   → Source: camera_time (500 Hz, 1000 entries)
   → Factor: 4
   → New key: camera_time_4x (3997 entries)

2. Pipeline step 1:  SincInterpolation on ch1, output_time_key: camera_time_4x
3. Pipeline step 2:  SincInterpolation on ch2, output_time_key: camera_time_4x
4. Pipeline step 3:  SincInterpolation on ch3, output_time_key: camera_time_4x
5. Pipeline step 4:  SincInterpolation on ch4, output_time_key: camera_time_4x

Contract: each output must have exactly 3997 samples, matching camera_time_4x.
```

---

## 2. Phase 1: Upsampled TimeFrame Creation Utility

**Goal:** Add a `createUpsampledTimeFrame()` function to the existing DerivedTimeFrame utilities.

### 2.1 New Options Struct

Add to `src/DataManager/utils/DerivedTimeFrame.hpp`:

```cpp
struct DerivedTimeFrameFromUpsamplingOptions {
    std::shared_ptr<TimeFrame> source_timeframe;  ///< TimeFrame to upsample
    int upsampling_factor;                        ///< Integer factor (must be > 0)
};
```

### 2.2 Function Specification

```cpp
std::shared_ptr<TimeFrame> createUpsampledTimeFrame(
    DerivedTimeFrameFromUpsamplingOptions const & options);
```

**Behavior:**

Given a source TimeFrame with N entries and clock values `[t_0, t_1, ..., t_{N-1}]`,
produce a new TimeFrame with `(N-1) * factor + 1` entries by linearly interpolating
between consecutive pairs:

```
Source: [0, 60, 120], factor = 4
Output: [0, 15, 30, 45, 60, 75, 90, 105, 120]
         ↑              ↑                   ↑
       original       original            original
```

- Between `t_i` and `t_{i+1}`, insert `factor - 1` evenly spaced values.
- Values are `int` — non-integer interpolation results are rounded to the nearest tick.
- Factor = 1 returns a copy of the source TimeFrame.
- Empty or single-entry source returns a copy (no interpolation possible).

### 2.3 Files to Modify

| File | Change |
|------|--------|
| `src/DataManager/utils/DerivedTimeFrame.hpp` | Add struct + function declaration |
| `src/DataManager/utils/DerivedTimeFrame.cpp` | Add implementation |
| `tests/DataManager/` | Add unit tests |

### 2.4 Tests

| Test | Expected |
|------|----------|
| `[0, 60, 120]` × 4 | `[0, 15, 30, 45, 60, 75, 90, 105, 120]`, size = 9 |
| `[0, 60, 120]` × 1 | `[0, 60, 120]`, size = 3 (identity) |
| `[0, 100]` × 3 | `[0, 33, 67, 100]`, size = 4 (rounding) |
| Empty source | Empty output |
| Single entry `[42]` | `[42]`, size = 1 |
| Factor ≤ 0 | Returns nullptr (invalid input) |

---

## 3. Phase 2: Pipeline Executor TimeKey Fix

**Goal:** Allow pipeline steps to specify which TimeKey the output should be associated
with, and fix the existing bug where all outputs are stored under `TimeKey("default")`.

### 3.1 Add `output_time_key` to `DataManagerStepDescriptor`

In `src/TransformsV2/core/DataManagerIntegration.hpp`:

```cpp
struct DataManagerStepDescriptor {
    // ... existing fields ...
    std::optional<std::string> output_time_key;  ///< TimeKey for output (must pre-exist)
};
```

### 3.2 Rework `storeOutputData()`

Current code (`DataManagerIntegration.cpp:333–349`):

```cpp
void DataManagerPipelineExecutor::storeOutputData(...) {
    TimeKey time_key("default");  // BUG: hardcoded
    std::visit([...](auto const & data_ptr) {
        data_manager_->setData<T>(output_key, data_ptr, time_key);
    }, data);
}
```

Reworked logic:

```
if output_time_key is specified:
    validate TimeKey exists in DataManager → error if not
    use TimeKey(output_time_key)
else:
    look up input's TimeKey via dm->getTimeKey(input_key)
    use that TimeKey (propagation, not hardcoded "default")
```

### 3.3 Output Size Validation

When `output_time_key` is specified, after the transform produces output, validate:

```
output_size == target_timeframe->getTotalFrameCount()
```

If mismatch, return an error:
`"Output has M samples but target TimeFrame 'X' has K entries."`

This enforces the contract between transforms and the user-provided target TimeFrame.

### 3.4 Thread Context Through `executeStep()`

`executeStep()` must pass `input_key` and `output_time_key` (from the step descriptor)
through to `storeOutputData()` so it can look up the input's TimeKey and validate
against the target TimeFrame.

### 3.5 Pipeline JSON Schema Update

Add `output_time_key` as an optional field in the step schema:

```json
{
    "step_id": "upsample_ch1",
    "transform_name": "SincInterpolation",
    "input_key": "ch1_voltage",
    "output_key": "ch1_voltage_upsampled",
    "output_time_key": "camera_time_4x",
    "parameters": {
        "upsampling_factor": 4,
        "kernel_half_width": 8
    }
}
```

### 3.6 Files to Modify

| File | Change |
|------|--------|
| `src/TransformsV2/core/DataManagerIntegration.hpp` | Add `output_time_key` to descriptor |
| `src/TransformsV2/core/DataManagerIntegration.cpp` | Rework `storeOutputData()`, add validation |
| `docs/pipeline_schema.md` | Document new optional field |
| `tests/TransformsV2/` | Executor tests |

### 3.7 Tests

| Test | Expected |
|------|----------|
| Step with `output_time_key` set | Output stored under that TimeKey |
| Step without `output_time_key` | Output inherits input's TimeKey |
| `output_time_key` that doesn't exist | Step fails with clear error |
| Output size ≠ target TimeFrame size | Step fails with size mismatch error |
| Existing non-resampling pipeline | Still works (regression) — now uses input's TimeKey instead of `"default"` |

---

## 4. Phase 3: Sinc Interpolation Transform

**Goal:** Implement band-limited (Nyquist) interpolation as a pure data container transform.

### 4.1 Algorithm

For each output sample position `m` in `[0, M-1]` where `M = (N-1) * factor + 1`:

1. Compute the fractional input position: `x = m / factor`
2. For each input sample `n` in the kernel window `[floor(x) - K, floor(x) + K]`:
   - Compute `sinc(x - n)` weighted by the window function
3. Output sample = weighted sum of input samples

Where `K` = `kernel_half_width` and `sinc(t) = sin(πt) / (πt)` (with `sinc(0) = 1`).

At signal boundaries, symmetric extension reflects the input signal to avoid edge artifacts.

### 4.2 Parameters

```cpp
enum class SincWindowType { Lanczos, Hann, Blackman };
enum class BoundaryMode { SymmetricExtension, ZeroPad };

struct SincInterpolationParams {
    int upsampling_factor;                                        ///< Required, > 1
    std::optional<int> kernel_half_width;                         ///< Default: 8
    std::optional<SincWindowType> window_type;                    ///< Default: Lanczos
    std::optional<BoundaryMode> boundary_mode;                    ///< Default: SymmetricExtension
};
```

### 4.3 Transform Function

```cpp
std::shared_ptr<AnalogTimeSeries> sincInterpolation(
    AnalogTimeSeries const & input,
    SincInterpolationParams const & params,
    ComputeContext const & ctx);
```

- Input: N samples
- Output: `(N-1) * factor + 1` samples with Dense TimeIndexStorage starting from 0
- No TimeFrame access — purely operates on data values

### 4.4 Registration

In `RegisteredTransforms.cpp`:

```cpp
auto const register_sinc = RegisterContainerTransform<
    AnalogTimeSeries, AnalogTimeSeries, SincInterpolationParams>(
    "SincInterpolation",
    sincInterpolation,
    ContainerTransformMetadata{
        .name = "SincInterpolation",
        .description = "Band-limited sinc interpolation (Nyquist upsampling)",
        .category = "Signal Processing"
    });
```

### 4.5 New Files

| File | Purpose |
|------|---------|
| `src/TransformsV2/algorithms/SincInterpolation/SincInterpolation.hpp` | Params struct + function declaration |
| `src/TransformsV2/algorithms/SincInterpolation/SincInterpolation.cpp` | Algorithm implementation |
| `src/TransformsV2/algorithms/SincInterpolation/CMakeLists.txt` | Build integration |
| `tests/TransformsV2/SincInterpolation/` | Unit tests |

### 4.6 Tests

| Test | Expected |
|------|----------|
| Factor 1 | Output identical to input |
| Pure 50 Hz sine at 500 Hz, upsampled 4× | Matches analytical sine within 1% error |
| DC signal (all same value) | Output is same constant value |
| Impulse (single non-zero sample) | Output is a windowed sinc function |
| Output size | Exactly `(N-1) * factor + 1` for all tested inputs |
| Large kernel vs small kernel | Larger kernel → closer to ideal reconstruction |
| Boundary mode: symmetric vs zero-pad | Symmetric has smaller edge artifacts |
| Empty input | Empty output |

---

## 5. Phase 4: DataManager Widget Integration

**Goal:** Let users create upsampled TimeFrames interactively through the UI.

### 5.1 UI Elements

Add to the DataManager widget (or a TimeFrame management sub-widget):

- **Source TimeFrame dropdown**: lists all registered TimeKeys
- **Upsampling factor**: integer spinbox (min: 2)
- **New TimeKey name**: text field with auto-suggestion (e.g., `source_name_Nx`)
- **Preview label**: "Source has N entries → output will have M entries"
- **Create button**: calls `createUpsampledTimeFrame()` + `dm->setTime()`

### 5.2 Files to Create/Modify

| File | Change |
|------|--------|
| DataManager widget (TBD) | Add "Create Upsampled TimeFrame" section |

---

## 6. Phase 5 (Future): Float TimeFrame

**Goal:** Support sub-clock-cycle precision for non-integer upsampling and continuous-time applications.

### 6.1 Motivation

With integer clock values, upsampling by non-integer factors (e.g., 3.7×) requires
rounding, which introduces temporal jitter. A `double`-valued TimeFrame would allow
exact representation of any upsampled position.

### 6.2 Scope

- Change `TimeFrame::_times` from `std::vector<int>` to `std::vector<double>`
- Update `getTimeAtIndex()` return type from `int` to `double`
- Update `checkFrameInbounds()` and all callers
- Evaluate whether `TimeFrameIndex` should remain `int64_t` (discrete frame index into
  the TimeFrame vector) or become `double` (continuous-time coordinate)
- Comprehensive regression test pass across all data types and widgets

### 6.3 Risk

This is a wide-reaching change touching many callers. Should be preceded by a
`grep`-based audit of all `getTimeAtIndex()` call sites and a compatibility plan.

---

## 7. Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| No new transform category | Existing `RegisterContainerTransform` suffices. Transforms get `InContainer const&` which exposes `getTimeFrame()`. Avoids adding complexity to an already complex library. |
| TimeFrame creation outside transforms | Transforms are pure data operations. Multi-channel workflows share one TimeFrame. No DataManager coupling in transform code. |
| Contract enforcement in executor | Output sample count must match target TimeFrame entry count. Fail-fast with clear error. |
| Integer TimeFrame for now | 500 Hz → 30 kHz upsampling produces integer clock positions. Float deferred to Phase 5. |
| Upsampling factor, not target rate | Simpler parameterization. Avoids needing to infer the input's sampling rate from its TimeFrame. |
| Scope: AnalogTimeSeries only | RaggedTimeSeries, LineData, etc. are out of scope for this roadmap. |
