# TimeFrame Resampling & Upsampling Roadmap

## Progress Summary

*Last updated: 2026-04-07*

| Phase | Status | Notes |
|-------|--------|-------|
| **Phase 1:** Upsampled TimeFrame Creation | **Complete** | `createUpsampledTimeFrame()` utility + 6 test cases, 73 assertions |
| **Phase 2:** Pipeline Executor TimeKey Fix | **Complete** | `output_time_key` field + propagate input TimeKey (fixes hardcoded `"default"` bug) |
| **Phase 3:** Sinc Interpolation Transform | **Complete** | Windowed-sinc container transform with kernel normalization, 3 window types, 2 boundary modes, 11 test sections |
| **Phase 4:** DataManager Widget Integration | **Complete** | NewTimeFrameWidget with method combo, source/factor selection, live preview, auto-name suggestion |
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

### 5.1 Placement in DataManager Widget

The `DataManager_Widget` has the following vertical layout (top to bottom):

1. `Feature_Table_Widget` — data object table
2. `TimeFrame_Table_Widget` — TimeFrame table
3. `Section("Output Directory")` — collapsible, wraps `OutputDirectoryWidget`
4. `Section("Create New Data")` — collapsible, wraps `NewDataWidget`
5. `selected_feature_label` — "No Feature Selected"
6. `QStackedWidget` — per-type detail panels

A new collapsible section **"Create New TimeFrame"** is added between items 4 and 5
(after "Create New Data", before the feature detail area). This follows the existing
`Section` pattern used by "Output Directory" and "Create New Data".

### 5.2 NewTimeFrameWidget

A new widget `NewTimeFrameWidget` lives in
`src/WhiskerToolbox/DataManager_Widget/NewTimeFrameWidget/`. It follows the same
pattern as `NewDataWidget`: a self-contained widget with a `.ui` file, wrapped in a
`Section` collapsible container.

#### UI Layout

```
┌─ Create New TimeFrame ──────────────────────────┐
│                                                  │
│  Method:      [ Upsample          ▼]             │
│                                                  │
│  ┌─ Upsample Settings ───────────────────────┐   │
│  │  Source:     [ camera_time      ▼]         │   │
│  │  Factor:     [ 4            ↕]             │   │
│  │                                            │   │
│  │  Preview: 1000 entries → 3997 entries      │   │
│  └────────────────────────────────────────────┘   │
│                                                  │
│  Name:        [ camera_time_4x       ]           │
│                                                  │
│  [ Create TimeFrame ]                            │
│                                                  │
└──────────────────────────────────────────────────┘
```

#### Widget Components

| Component | Qt Type | Name | Purpose |
|-----------|---------|------|---------|
| Method combo | `QComboBox` | `method_combo` | Creation method. Initially only "Upsample". Extensible for future methods (e.g., "Downsample", "Subset", "Merge"). |
| Source combo | `QComboBox` | `source_timeframe_combo` | Lists available TimeKeys from `DataManager::getTimeFrameKeys()` |
| Factor spinbox | `QSpinBox` | `upsampling_factor_spin` | Integer, min: 2, max: 1000, default: 2 |
| Preview label | `QLabel` | `preview_label` | Shows "N entries → M entries" computed on-the-fly |
| Name field | `QLineEdit` | `new_timeframe_name` | User-specified TimeKey name |
| Create button | `QPushButton` | `create_timeframe_button` | Triggers creation |

The "Upsample Settings" group (source combo, factor spinbox, preview label) can be
placed inside a `QGroupBox` or a plain `QWidget` container. When additional methods
are added in the future, a `QStackedWidget` keyed by `method_combo` index can swap
between method-specific settings panels. For now, only the upsample panel exists.

#### Behavior

1. **Method selection** — `method_combo` currently has only "Upsample". Changing the
   method would swap the settings panel (future extensibility).

2. **Preview update** — Whenever `source_timeframe_combo` or `upsampling_factor_spin`
   changes, recompute and display:
   ```
   Source entries: N = source_timeframe->getTotalFrameCount()
   Output entries: M = (N - 1) * factor + 1
   Preview text: "N entries → M entries"
   ```
   If source has 0 or 1 entries, show "Cannot upsample (too few entries)".

3. **Auto-suggest name** — When the source or factor changes, auto-populate the name
   field with `"{source_name}_{factor}x"` (e.g., `"camera_time_4x"`). The user can
   override this. Only auto-suggest if the name field hasn't been manually edited or
   still matches the previous auto-suggestion.

4. **Create button** — On click:
   - Validate: name not empty, name not already a registered TimeKey
   - Call `createUpsampledTimeFrame()` from `DerivedTimeFrame.hpp`
   - Call `_data_manager->setTime(TimeKey(name), upsampled_tf)`
   - Show error (e.g., a `QLabel` or status text) if creation fails (nullptr return)
   - On success, refresh `source_timeframe_combo` (the new TimeFrame is now available
     as a source for further derivations) and clear the name field

5. **Refresh on DataManager changes** — `populateTimeframes()` is called when the
   DataManager adds/removes TimeFrames. This keeps the source combo up to date.
   Use the same observer callback pattern as `NewDataWidget`.

#### Signal/Slot Chain

```
User clicks "Create TimeFrame" button
    ↓
NewTimeFrameWidget::_createTimeFrame() (private slot)
    ↓
    Validates inputs (name non-empty, name not duplicate)
    ↓
Emits: createTimeFrame(std::string name,
                        std::string source_timeframe_key,
                        int upsampling_factor)
    ↓ (connected in DataManager_Widget constructor)
DataManager_Widget::_createNewTimeFrame()
    ↓
    Calls createUpsampledTimeFrame(options)
    Calls _data_manager->setTime(TimeKey(name), result)
    Refreshes TimeFrame_Table_Widget
```

### 5.3 Integration in DataManager_Widget

#### DataManager_Widget.ui Changes

Add a new `Section` widget named `new_timeframe_section` in the vertical layout after
`new_data_section` and before `selected_feature_label`. The Section wraps a
`NewTimeFrameWidget` named `new_timeframe_widget`, following the identical pattern
used by `new_data_section` / `new_data_widget`.

```xml
<item>
  <widget class="Section" name="new_timeframe_section" native="true">
    <layout class="QVBoxLayout" name="new_timeframe_section_layout">
      <item>
        <widget class="NewTimeFrameWidget" name="new_timeframe_widget" native="true"/>
      </item>
    </layout>
  </widget>
</item>
```

Register the custom widget in the `<customwidgets>` block:

```xml
<customwidget>
  <class>NewTimeFrameWidget</class>
  <extends>QWidget</extends>
  <header>DataManager_Widget/NewTimeFrameWidget/NewTimeFrameWidget.hpp</header>
  <container>1</container>
</customwidget>
```

#### DataManager_Widget.cpp Changes

In the constructor, after the `new_data_section` setup:

```cpp
ui->new_timeframe_section->autoSetContentLayout();
ui->new_timeframe_section->setTitle("Create New TimeFrame");
ui->new_timeframe_widget->setDataManager(_data_manager);
ui->new_timeframe_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

connect(ui->new_timeframe_widget, &NewTimeFrameWidget::createTimeFrame,
        this, &DataManager_Widget::_createNewTimeFrame);
```

Add the slot:

```cpp
void DataManager_Widget::_createNewTimeFrame(
        std::string const & name,
        std::string const & source_key,
        int upsampling_factor) {

    auto source_tf = _data_manager->getTime(TimeKey(source_key));
    if (!source_tf) return;

    DerivedTimeFrameFromUpsamplingOptions opts;
    opts.source_timeframe = source_tf;
    opts.upsampling_factor = upsampling_factor;

    auto result = createUpsampledTimeFrame(opts);
    if (!result) return;

    _data_manager->setTime(TimeKey(name), result);
    ui->timeframe_table_widget->populateTable();
    ui->new_data_widget->populateTimeframes();
    ui->new_timeframe_widget->populateTimeframes();
}
```

### 5.4 New Files

| File | Purpose |
|------|---------|
| `src/WhiskerToolbox/DataManager_Widget/NewTimeFrameWidget/NewTimeFrameWidget.hpp` | Widget class declaration |
| `src/WhiskerToolbox/DataManager_Widget/NewTimeFrameWidget/NewTimeFrameWidget.cpp` | Widget implementation |
| `src/WhiskerToolbox/DataManager_Widget/NewTimeFrameWidget/NewTimeFrameWidget.ui` | Qt Designer form |

### 5.5 Files to Modify

| File | Change |
|------|--------|
| `DataManager_Widget.ui` | Add `new_timeframe_section` Section + `NewTimeFrameWidget` |
| `DataManager_Widget.hpp` | Add `_createNewTimeFrame()` slot declaration |
| `DataManager_Widget.cpp` | Section init, signal connection, slot implementation |
| `src/WhiskerToolbox/CMakeLists.txt` | Add NewTimeFrameWidget sources |

### 5.6 Tests

UI widget tests are not in scope for Phase 4 (no existing widget test infrastructure).
The underlying `createUpsampledTimeFrame()` function is already tested in Phase 1.
Manual verification:

| Scenario | Expected |
|----------|----------|
| Select source TimeFrame, set factor, click create | New TimeFrame appears in TimeFrame_Table_Widget |
| Preview label updates on source/factor change | Shows correct "N → M entries" |
| Duplicate name | Create button shows error / is disabled |
| Empty name | Create button disabled or shows error |
| Source with 0 or 1 entries | Preview shows warning; create still works (returns copy) |
| New TimeFrame appears in NewDataWidget combo | Can immediately use it when creating new data |

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
