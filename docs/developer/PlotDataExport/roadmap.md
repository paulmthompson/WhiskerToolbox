# PlotDataExport — Implementation Roadmap

Step-by-step guide for building the PlotDataExport library and integrating it into
the plot widgets. Steps within a phase can be done in parallel unless noted.

---

## Phase 1: Core Library (`src/PlotDataExport/`)

### Step 1.1 — Create library skeleton ✅

1. Create `src/PlotDataExport/CMakeLists.txt`:
   - Define as an `INTERFACE` library (header-only, no `.cpp` files).
   - Link dependencies: `GatherResult`, `CorePlotting`, `TimeFrame`.
   - No Qt dependency.

2. Add `add_subdirectory(PlotDataExport)` to `src/CMakeLists.txt`.

3. Verify the build still succeeds (empty library, no consumers yet).

### Step 1.2 — Raster CSV export ✅

Create `src/PlotDataExport/RasterCSVExport.hpp`.

**Structs:**
```cpp
struct RasterSeriesInput {
    std::string event_key;
    GatherResult<DigitalEventSeries> const& gathered;
    TimeFrame const* time_frame;
};

struct RasterExportMetadata {
    std::string alignment_key;
    double window_size = 0.0;
};
```

**Functions:**
```cpp
[[nodiscard]] std::string exportRasterToCSV(
    std::vector<RasterSeriesInput> const& series,
    RasterExportMetadata const& metadata = {},
    std::string_view delimiter = ",");
```

**Algorithm:**
1. Write metadata comment header (`# export_type: raster`, `# alignment_key: ...`, etc.).
2. Write CSV header: `trial_index<delim>event_key<delim>relative_time`.
3. For each series → for each trial → for each event in trial view:
   - `int64_t alignment = gathered.alignmentTimeAt(trial_idx)`
   - `int64_t abs_time = time_frame->getTimeAtIndex(event.time())`
   - `double relative = static_cast<double>(abs_time - alignment)`
   - Append row: `trial_idx<delim>event_key<delim>relative`.

**Reference code:** `EventPlotOpenGLWidget.cpp` lines 544–590 (the scene building
loop that iterates trials/events and computes relative time from alignment).

### Step 1.3 — Histogram CSV export ✅

Create `src/PlotDataExport/HistogramCSVExport.hpp`.

**Structs:**
```cpp
struct HistogramSeriesInput {
    std::string event_key;
    CorePlotting::HistogramData const& data;
};

struct HistogramExportMetadata {
    std::string alignment_key;
    double window_size = 0.0;
    std::string scaling_mode;       // "FiringRateHz", "ZScore", etc.
    std::string estimation_method;  // "Binning", "GaussianKernel", etc.
};
```

**Functions:**
```cpp
[[nodiscard]] std::string exportHistogramToCSV(
    std::vector<HistogramSeriesInput> const& series,
    HistogramExportMetadata const& metadata = {},
    std::string_view delimiter = ",");
```

**Algorithm:**
1. Write metadata comment header.
2. Write CSV header: `bin_center<delim>event_key<delim>value`.
3. For each series → for each bin `i` in `data.counts`:
   - `double center = data.binCenter(i)`
   - Append row: `center<delim>event_key<delim>data.counts[i]`.

### Step 1.4 — Heatmap CSV export ✅

Create `src/PlotDataExport/HeatmapCSVExport.hpp`.

**Structs:**
```cpp
struct HeatmapExportMetadata {
    std::string alignment_key;
    double window_size = 0.0;
    std::string scaling_mode;
    std::string estimation_method;
};
```

**Functions:**
```cpp
// Aggregate — wide matrix
[[nodiscard]] std::string exportHeatmapToCSV(
    std::vector<std::string> const& unit_keys,
    std::vector<CorePlotting::HeatmapRowData> const& rows,
    HeatmapExportMetadata const& metadata = {},
    std::string_view delimiter = ",");

// Per-trial — long-form tidy
[[nodiscard]] std::string exportHeatmapPerTrialToCSV(
    std::vector<std::string> const& unit_keys,
    std::vector<RateEstimateWithTrials> const& rate_data,
    HeatmapExportMetadata const& metadata = {},
    std::string_view delimiter = ",");
```

**Algorithm (aggregate):**
1. Write metadata comment header.
2. Compute bin centers from the first row: `bin_start + (i + 0.5) * bin_width`.
3. Write header: `unit_key<delim><center_0><delim><center_1><delim>...`.
4. For each unit `i`: write `unit_keys[i]<delim>rows[i].values[0]<delim>...`.

**Algorithm (per-trial):**
1. Write metadata comment header.
2. Write header: `unit_key<delim>trial_index<delim>bin_center<delim>value`.
3. For each unit → for each trial → for each time bin:
   - Append row from `rate_data[unit].trials.per_trial_values[trial][bin]`.

> **Implementation note:** The per-trial function takes `vector<PerTrialHeatmapInput>`
> rather than `vector<RateEstimateWithTrials>` directly. `EventRateEstimation` lives
> under the UI layer (gated by `ENABLE_UI`) so adding it as a dependency of
> `PlotDataExport` would break non-UI builds and tests. Widgets convert
> `RateEstimateWithTrials → PerTrialHeatmapInput` before calling the export API.

### Step 1.5 — Build verification ✅

```bash
cmake --build --preset linux-clang-release > build_log.txt 2>&1
```

The library is header-only and has no consumers yet, so this just verifies the
CMake integration is correct.

---

## Phase 2: Unit Tests (`tests/PlotDataExport/`)

### Step 2.1 — Test scaffolding ✅

1. [x] Create `tests/PlotDataExport/CMakeLists.txt`.
   - Define test executable linking `PlotDataExport` and Catch2.
   - Note: since `PlotDataExport` depends on `GatherResult` which depends on
     `TransformsV2`, `--whole-archive` linker flags are needed (same pattern as
     `tests/TransformsV2/CMakeLists.txt`).
2. [x] Add `add_subdirectory(PlotDataExport)` to `tests/CMakeLists.txt`.
3. [x] Add stub `TEST_CASE` to each `.cpp` (scaffold tests pass with ctest).

### Step 2.2 — Raster export tests ✅

Create `tests/PlotDataExport/test_RasterCSVExport.cpp`.

Test cases:
- [x] **Empty series vector** → CSV has only header + metadata comments.
- [x] **Empty GatherResult** → CSV has only header (no data rows, no crash).
- [x] **Single series, single trial, single event** → verify exact CSV row values.
- [x] **Single series, single trial, multiple events** → all relative times correct.
- [x] **Multiple trials** → trial_index column increments correctly.
- [x] **Trial with no events** → that trial produces no data row.
- [x] **Multiple series** → verify interleaving and correct event_key per row.
- [x] **Custom delimiter** → verify tab-separated output.
- [x] **Metadata comment header** → verify `#` lines present with correct values.
- [x] **Metadata not written when empty** → alignment_key/window_size lines absent.

### Step 2.3 — Histogram export tests ✅

Create `tests/PlotDataExport/test_HistogramCSVExport.cpp`.

Test cases:
- [x] **Empty series vector** → CSV has only header + metadata comment.
- [x] **Empty HistogramData** → 0 data rows, no crash.
- [x] **Single series, single bin** → exact `bin_center` and `value` in row.
- [x] **Multiple bins** → all 3 bin centers correct (including negative bin_start).
- [x] **Zero-count bins appear** → middle bin with 0 count still emits a row.
- [x] **Multiple series** → each row carries the correct `event_key`.
- [x] **Series with different bin counts** → independent bin counts per series.
- [x] **Custom delimiter** → tab-separated header and data.
- [x] **Full metadata comment header** → all four optional fields written.
- [x] **Empty metadata suppressed** → no extra `#` lines for default-constructed metadata.

### Step 2.4 — Heatmap export tests ✅

Create `tests/PlotDataExport/test_HeatmapCSVExport.cpp`.

**Aggregate (`exportHeatmapToCSV`) test cases:**
- [x] Empty rows → comment + `unit_key` header only.
- [x] Single unit, single bin → header contains bin center, data row matches.
- [x] Single unit, multiple bins → all bin centers in header, all values in row.
- [x] Multiple units → each unit gets its own row, shared header.
- [x] Custom delimiter → tab-separated header and data.
- [x] Full metadata header → all four optional `#` fields written.
- [x] Empty metadata suppressed → no extra `#` lines.

**Per-trial (`exportHeatmapPerTrialToCSV`) test cases:**
- [x] Empty data → comment + column header only.
- [x] Single unit, single trial, single bin → exact row content.
- [x] Single unit, multiple trials → correct `trial_index` column.
- [x] Multiple units → both units present with correct `unit_key`.
- [x] Custom delimiter → tab-separated output.
- [x] Metadata comment header → `alignment_key` and `window_size` written.
- [x] Unit with no trials → zero data rows emitted.
- [x] Row count check → 2 units × 3 trials × 2 bins = 14 total newlines.

### Step 2.5 — Run tests ✅

```bash
ctest --preset linux-clang-release -R "PlotDataExport" --output-on-failure > test_log.txt 2>&1
```

Result: **100% tests passed** (1 test — `test_PlotDataExport_all` — bundles all 3 test files).

---

## Phase 3: Widget Integration

### Step 3.1 — EventPlotWidget CSV export ✅

**Files to modify:**
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.hpp` — add `exportCSVRequested()` signal
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.cpp` — add "Export CSV" button, connect click → emit signal
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.ui` — add button in export section (next to SVG button)
- `src/WhiskerToolbox/Plots/EventPlotWidget/Rendering/EventPlotOpenGLWidget.hpp` — add `collectRasterExportData()` method returning `vector<RasterSeriesInput>`
- `src/WhiskerToolbox/Plots/EventPlotWidget/Rendering/EventPlotOpenGLWidget.cpp` — implement `collectRasterExportData()` (extract gather logic from `rebuildScene()` Step 1)
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotWidget.hpp` — add `handleExportCSV()` slot
- `src/WhiskerToolbox/Plots/EventPlotWidget/UI/EventPlotWidget.cpp` — implement: file dialog → collect data → `exportRasterToCSV()` → write file
- `src/WhiskerToolbox/Plots/EventPlotWidget/EventPlotWidgetRegistration.cpp` — connect `exportCSVRequested` → `handleExportCSV`

**Follow the exact SVG export pattern** already in place:
1. `EventPlotPropertiesWidget` emits signal
2. Registration file connects to `EventPlotWidget::handleExportCSV()`
3. Handler opens `AppFileDialog::getSaveFileName("export_event_plot_csv", ...)`
4. Calls `_opengl_widget->collectRasterExportData()`
5. Calls `exportRasterToCSV(data)`
6. Writes string to file

### Step 3.2 — PSTHWidget CSV export (parallel with 3.1) ✅

**Files to modify:**
- `src/WhiskerToolbox/Plots/PSTHWidget/UI/PSTHPropertiesWidget.hpp` — add `exportCSVRequested()` signal
- `src/WhiskerToolbox/Plots/PSTHWidget/UI/PSTHPropertiesWidget.cpp` — add button, connect
- `src/WhiskerToolbox/Plots/PSTHWidget/Rendering/PSTHPlotOpenGLWidget.hpp` — add getter for cached `HistogramData` per series (e.g. `collectHistogramExportData()`)
- `src/WhiskerToolbox/Plots/PSTHWidget/Rendering/PSTHPlotOpenGLWidget.cpp` — implement getter
- `src/WhiskerToolbox/Plots/PSTHWidget/UI/PSTHWidget.hpp` — add `handleExportCSV()`
- `src/WhiskerToolbox/Plots/PSTHWidget/UI/PSTHWidget.cpp` — implement handler
- `src/WhiskerToolbox/Plots/PSTHWidget/PSTHWidgetRegistration.cpp` — connect signal

### Step 3.3 — HeatmapWidget CSV export (parallel with 3.1) ✅

**Files to modify:**
- `src/WhiskerToolbox/Plots/HeatmapWidget/UI/HeatmapPropertiesWidget.hpp` — add `exportCSVRequested(bool include_per_trial)` signal, add per-trial checkbox
- `src/WhiskerToolbox/Plots/HeatmapWidget/UI/HeatmapPropertiesWidget.cpp` — add button + checkbox, connect
- `src/WhiskerToolbox/Plots/HeatmapWidget/UI/HeatmapWidget.hpp` — add `handleExportCSV(bool include_per_trial)`
- `src/WhiskerToolbox/Plots/HeatmapWidget/UI/HeatmapWidget.cpp` — implement handler
- `src/WhiskerToolbox/Plots/HeatmapWidget/HeatmapWidgetRegistration.cpp` — connect signal

**Note:** For per-trial export, the heatmap pipeline currently calls `estimateRates()`
which returns `vector<RateEstimate>` (aggregate only). To get per-trial data, the
pipeline needs to call `estimateRateWithTrials()` instead. Options:
  - (a) Always compute `RateEstimateWithTrials` and cache it — slightly more memory.
  - (b) Re-run the pipeline with `estimateRateWithTrials()` on export — slightly slower.
  
  Recommendation: option (b) to avoid changing the hot rendering path.

### Step 3.4 — CMake link updates ✅

Add `PlotDataExport` to the `target_link_libraries` for the WhiskerToolbox
executable target (or to each widget's CMakeLists if they have separate ones).

### Step 3.5 — Build and smoke test ✅

```bash
cmake --build --preset linux-clang-release > build_log.txt 2>&1
ctest --preset linux-clang-release --output-on-failure > test_log.txt 2>&1
```

---

## Phase 4: Code Quality

### Step 4.1 — clang-format

```bash
clang-format -i src/PlotDataExport/*.hpp
clang-format -i tests/PlotDataExport/*.cpp
# + all modified widget .hpp/.cpp files
```

### Step 4.2 — clang-tidy

```bash
run-clang-tidy -fix -p out/build/Clang/Release \
    src/PlotDataExport/RasterCSVExport.hpp \
    src/PlotDataExport/HistogramCSVExport.hpp \
    src/PlotDataExport/HeatmapCSVExport.hpp \
    tests/PlotDataExport/test_RasterCSVExport.cpp \
    tests/PlotDataExport/test_HistogramCSVExport.cpp \
    tests/PlotDataExport/test_HeatmapCSVExport.cpp \
    > tidy_log.txt 2>&1
grep -c "error:\|warning:" tidy_log.txt || echo "Clean"
```

Re-run build after auto-fixes to confirm nothing broke.

### Step 4.3 — Rebuild and re-test

```bash
cmake --build --preset linux-clang-release > build_log.txt 2>&1
ctest --preset linux-clang-release -R "PlotDataExport" --output-on-failure > test_log.txt 2>&1
```

---

## Phase 5: Documentation

### Step 5.1 — Developer docs ✅

Already done (this directory). Ensure `index.qmd` is up to date with final API.

Add per-header `.qmd` files if the API surface warrants detailed docs:
- `docs/developer/PlotDataExport/RasterCSVExport.qmd`
- `docs/developer/PlotDataExport/HistogramCSVExport.qmd`
- `docs/developer/PlotDataExport/HeatmapCSVExport.qmd`

### Step 5.2 — User guide ✅

Create `docs/user_guide/export/csv_plot_export.qmd` with:

1. **Overview** — which widgets support CSV export.
2. **How to export** — button location, file dialog.
3. **Format specifications** — column descriptions + example CSV snippets for each format.
4. **Python examples** — `pd.read_csv(comment='#')` → seaborn/matplotlib plotting code.
   - Raster: `sns.stripplot(x='relative_time', y='trial_index', hue='event_key')`
   - PSTH: `sns.lineplot(x='bin_center', y='value', hue='event_key')`
   - Heatmap: `pd.read_csv(index_col=0)` → `sns.heatmap()`
   - Per-trial: `pivot_table()` → `sns.heatmap()`
5. **R examples** — `readr::read_csv(comment='#')` → ggplot2 code.
   - Raster: `ggplot(aes(x=relative_time, y=trial_index, color=event_key)) + geom_point()`
   - PSTH: `ggplot(aes(x=bin_center, y=value, color=event_key)) + geom_line()`
   - Heatmap: `ggplot(aes(x=bin_center, y=unit_key, fill=value)) + geom_tile()`

### Step 5.3 — Register in quarto navigation ✅

Update `docs/_quarto.yml`:
- Add `user_guide/export/csv_plot_export.qmd` under the "Export" section.
- Add `developer/PlotDataExport/index.qmd` under the developer section.

### Step 5.4 — Update copilot-instructions.md ✅

Add a one-sentence description of `PlotDataExport` to the **Project Architecture
(Libraries)** section in `.github/copilot-instructions.md`:

> **`PlotDataExport` (`src/PlotDataExport`)**: Header-only library providing free
> functions to serialize plot data (raster events, PSTH histograms, heatmaps) to
> CSV strings. Depends on `GatherResult`, `CorePlotting`, and `TimeFrame`. No Qt
> dependency.

---

## Checklist Summary

- [x] `src/PlotDataExport/CMakeLists.txt`
- [x] `src/PlotDataExport/RasterCSVExport.hpp`
- [x] `src/PlotDataExport/HistogramCSVExport.hpp`
- [x] `src/PlotDataExport/HeatmapCSVExport.hpp`
- [x] `src/CMakeLists.txt` updated
- [x] `tests/PlotDataExport/CMakeLists.txt`
- [x] `tests/PlotDataExport/test_RasterCSVExport.cpp`
- [x] `tests/PlotDataExport/test_HistogramCSVExport.cpp`
- [x] `tests/PlotDataExport/test_HeatmapCSVExport.cpp`
- [x] `tests/CMakeLists.txt` updated
- [x] All tests pass
- [x] EventPlotWidget: Export CSV button + handler
- [x] PSTHWidget: Export CSV button + handler
- [x] HeatmapWidget: Export CSV button + handler (aggregate; per-trial deferred to Phase 5)
- [x] Widget CMakeLists link updated (EventPlotWidget, PSTHWidget, HeatmapWidget)
- [x] clang-format on all new/modified files
- [x] clang-tidy on all new/modified files
- [x] `docs/developer/PlotDataExport/index.qmd` finalized
- [x] `docs/user_guide/export/csv_plot_export.qmd` created
- [x] `docs/_quarto.yml` updated
- [x] `.github/copilot-instructions.md` updated
