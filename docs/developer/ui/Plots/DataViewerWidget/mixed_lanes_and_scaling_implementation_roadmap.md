# DataViewer Mixed-Lanes and Y-Scaling Implementation Roadmap

## Purpose

This document is an execution roadmap for two related deliverables:

1. Fix the current DataViewer Y-scaling behavior so AnalogTimeSeries and DigitalEventSeries respond predictably to global scaling.
2. Add lane placement controls that support:
- interspersed analog and digital-event lanes
- multiple signals in the same lane (overlay), scoped to AnalogTimeSeries plus DigitalEventSeries for the first release

This roadmap is intentionally implementation-oriented and can be followed phase by phase by a developer.

## Scope

In scope:

- Y-scaling bug fix and regression protection
- Layout model for mixed lane ordering and lane sharing
- Serialization for lane placement specification
- Backward compatibility with existing saved workspaces
- Documentation updates

Out of scope for this first delivery:

- Full UI editor for lane assignment (JSON and state API first)
- Generalized overlay of all series types (first release is AnalogTimeSeries plus DigitalEventSeries)
- Any behavior changes for FullCanvas event and interval rendering

## Success Criteria

1. Analog channels are visible at sane global scale defaults in high-channel-count datasets.
2. Mixed analog and digital-event datasets are readable without extreme tuning.
3. Developers can define lane placement and overlay via serialized state.
4. Backward compatibility with legacy serialized JSON is not required for this implementation branch.
5. Existing analog external sorter behavior remains intact as a fallback ordering source.

## High-Level Delivery Order

1. Phase 1: Scaling bug fix and stabilization (completed)
2. Phase 2: Mixed-lane data model and serialization
3. Phase 3: Layout engine implementation for grouped and weighted lanes
4. Phase 4: DataViewer wiring for ordering, rendering, and axis descriptors
5. Ordering abstractions roadmap (prerequisite for Phase 4b)
6. Phase 4b: Interactive lane drag-and-drop reordering
7. Phase 4C: Per-series relative ordering UI (autocomplete dialog)
8. Phase 4D: JSON lane layout save and load
9. Phase 4E: CSV spike-to-analog pairing loader
10. Phase 5: Compatibility and migration testing
11. Phase 6: Documentation and handoff

## Progress Update (2026-04-21)

Completed in Phase 1:

- Simplified analog Y-transform composition to remove duplicated normalization effects.
- Updated auto global scaling heuristic to align with normalized lane occupancy behavior.
- Made event glyph vertical sizing explicit and consistent across renderers.
- Added and validated focused regression tests for transform behavior and mixed occupancy expectations.
- Rebuilt with preset and ran targeted DataViewer/plotting test bundles successfully.

Current status by phase:

- Phase 1: completed
- Phase 2: completed
- Phase 3: completed
- Phase 4: completed
- Ordering abstractions roadmap: completed
- Phase 4b: completed
- Phase 4C: completed
- Phase 4D: not started
- Phase 4E: not started
- Phase 5: not started
- Phase 6: in progress (roadmap/doc updates)

Branch decision for this implementation:

- Legacy JSON backward compatibility is intentionally not maintained.
- The state schema now prioritizes the mixed-lane model directly.

---

## Phase 1: Scaling Bug Fix and Stabilization

### Goal

Eliminate inconsistent scaling behavior that currently makes analog traces require very large global_y_scale values while events appear correctly sized.

### Expected Root-Cause Areas to Verify

- Analog transform composition includes duplicated normalization effects.
- Auto-arrange and auto-scale logic compounds analog shrink.
- Event vertical sizing semantics are inconsistent with analog semantics.

### Tasks

- [x] Audit and simplify analog Y transform composition in the DataViewer transform pipeline.
- [x] Ensure analog normalization uses one coherent standard-deviation normalization path.
- [x] Rework auto global scale heuristics to match the corrected transform equation.
- [x] Resolve event height semantics so event sizing is explicit and predictable.
- [x] Add targeted tests for analog occupancy and mixed analog/event occupancy.

### Primary Files

- src/WhiskerToolbox/DataViewer_Widget/Rendering/TransformComposers.hpp
- src/WhiskerToolbox/DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.cpp
- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp
- src/WhiskerToolbox/DataViewer_Widget/Rendering/SceneBuildingHelpers.cpp
- src/WhiskerToolbox/DataViewer_Widget/Rendering/OpenGLWidget.cpp

### Acceptance Criteria

- [x] 32-channel analog datasets do not require extreme global scaling to be readable.
- [x] Mixed analog plus digital-event views are readable with consistent global scaling behavior.
- [x] No regressions in existing wheel and time-window behavior tests.

---

## Phase 2: Mixed-Lane Data Model and Serialization

### Goal

Define a durable serialized representation for lane assignment, ordering, overlay, and lane weighting.

### Design Rules

- Defaults must exactly preserve legacy one-series-per-lane behavior.
- New fields are optional and backward compatible.
- No UI dependency is required to use the feature.

### Data Model Additions

Add per-series layout override data in DataViewer state:

- lane_id: string, empty means legacy auto lane
- lane_order: integer, optional explicit ordering
- lane_weight: float, default 1.0
- overlay_mode: enum, default Auto
- overlay_z: integer, default 0

Optional lane-level map keyed by lane_id:

- display_label
- lane_weight override

### Tasks

- [x] Add new serializable structs and fields in DataViewer state data.
- [x] Add state API methods for set, clear, and query of lane overrides.
- [x] Add validation for invalid or conflicting override values.
- [x] Add roundtrip tests for new schema.

### Primary Files

- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerStateData.hpp
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerState.hpp
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerState.cpp
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerStateData.test.cpp

### Acceptance Criteria

- [x] New fields serialize and deserialize correctly.
- [x] Legacy saved state loads unchanged. (intentionally not required in this implementation)
- [x] Invalid override input is rejected or normalized deterministically.

---

## Phase 3: Layout Engine Support for Grouped and Weighted Lanes

### Goal

Allow multiple series to share one lane transform and support lane-height weighting.

### Implementation Approach

- Extend series metadata for layout request with lane assignment and lane weight context.
- Group stackable series by lane_id during layout.
- Allocate lane bands by normalized lane_weight.
- Apply same lane transform to all series in a shared lane.

### Tasks

- [x] Extend layout request series metadata with lane assignment fields.
- [x] Implement grouped-lane allocation in stacked layout strategy.
- [x] Preserve visibility and culling behavior for grouped lanes.
- [x] Add CorePlotting layout tests for overlay lanes and weighted lanes.

### Primary Files

- src/CorePlotting/Layout/LayoutEngine.hpp
- src/CorePlotting/Layout/StackedLayoutStrategy.cpp
- src/CorePlotting/Layout/LayoutEngine.cpp
- tests/CorePlotting/LayoutEngine.test.cpp

### Acceptance Criteria

- [x] Equal weights reproduce current lane sizing behavior.
- [x] Weighted lanes produce expected relative heights.
- [x] Shared lanes map all assigned series to same vertical band.

---

## Phase 4: DataViewer Wiring for Interspersed and Shared Lanes

### Goal

Use the new lane metadata in DataViewer request construction, rendering, and axis presentation.

### Behavior Policy

Ordering precedence:

1. explicit lane_order override
2. existing analog external sorter for analog-only fallback order
3. legacy type-based fallback

### Tasks

- [x] Replace type-bucket request assembly with unified series ordering path.
- [x] Map per-series overrides into layout request metadata.
- [x] Keep non-stackable FullCanvas behavior unchanged.
- [x] Update lane-axis descriptor generation to lane-level aggregation in shared lanes.
- [x] Define lane label policy for overlay lanes.

### Primary Files

- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp
- src/WhiskerToolbox/DataViewer_Widget/Rendering/OpenGLWidget.cpp
- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisStateData.hpp
- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/MultiLaneVerticalAxisWidget.cpp
- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.test.cpp

### Acceptance Criteria

- [x] Developers can intersperse analog and event lanes through state overrides.
- [x] Developers can assign multiple analog/event series to a single lane.
- [x] Axis labels remain stable and meaningful for overlays.

---

## Phase 4b: Interactive Lane Drag-and-Drop Reordering

Prerequisite roadmap:

- docs/developer/ui/Plots/DataViewerWidget/ordering_abstractions_implementation_roadmap.md

### Goal

Allow users to click-drag a lane on the vertical axis and drop it between other lanes, with deterministic persistence through lane override state.

### Why this fits after Phase 4

- Phase 4 establishes lane identity and ordering in DataViewer request construction.
- Drag-and-drop should mutate the same source of truth (`series_lane_overrides`) rather than introducing a parallel ordering path.

### Context: What the Ordering Abstractions Roadmap Delivered

The completed ordering abstractions roadmap changed several design constraints that Phase 4b was planned against:

- **Per-series `lane_order` is the universal ordering mechanism.** Spike-sorter electrode positions are now converted to `SeriesLaneOverrideData.lane_order` overrides at load time inside `OpenGLWidget::loadSpikeSorterConfiguration`. There is no longer a separate spike-sorter fallback path in the resolver.
- **Resolver precedence (Phase D):** explicit `lane_order` → relational constraints → deterministic tie-break. The spike-sorter fallback level is gone.
- **Signal wiring is already complete.** `seriesLaneOverrideChanged` and `laneOverrideChanged` are wired in `OpenGLWidget::setState` to set `layout_response_dirty`, clear the scene, and call `update()`. Task 6 below requires no additional wiring.
- **`LaneOverrideData` has no `lane_order` field.** The ordering abstractions roadmap (Phase E) implemented relational constraints (`StackableOrderingConstraintData`) but did not add `lane_order` to `LaneOverrideData`. Given that the spike-sorter F2 approach routes through per-series `lane_order`, drag/drop should follow the same pattern: when a lane is dragged, update `lane_order` on all series that share the dragged `lane_id` to the new resolved rank. No new field in `LaneOverrideData` is needed.

### UX Model (first release)

- Drag handle surface: `MultiLaneVerticalAxisWidget` lane label area.
- Start drag: left-click press within a visible lane hit region.
- During drag: show insertion marker between target lanes and ghost highlight on dragged lane.
- Drop: commit a new lane ordering by updating `lane_order` on all series sharing the dragged `lane_id`, then recompute layout.
- Cancel: Escape key or mouse release outside valid target keeps existing order.

### Data/State Model Requirements

1. Add stable lane identity to axis descriptors:
	- Extend `LaneAxisDescriptor` with `lane_id` (stable key matching the `lane_id` field in `SeriesLaneOverrideData`).
	- Keep `label` as display-only.
	- This is required so the axis widget can identify which override key to mutate on drop.

2. No new lane-level `lane_order` field needed:
	- `LaneOverrideData` does not need a `lane_order` field. Drag/drop reorder writes `lane_order` directly onto all `SeriesLaneOverrideData` entries whose `lane_id` matches the dragged lane — the same pattern used by `loadSpikeSorterConfiguration`.
	- For a single-series lane: update the one series' `lane_order`.
	- For a shared lane (multiple series with the same `lane_id`): update all member series to the same new `lane_order` value.

3. Ordering precedence (no change from resolver):
	1. explicit `lane_order` in `SeriesLaneOverrideData` (covers both spike-sorter-loaded and drag/drop results)
	2. relational constraints (`StackableOrderingConstraintData`)
	3. deterministic tie-break (series type precedence then stable key)

### Implementation Tasks

- [x] Extend `LaneAxisDescriptor` with a `lane_id` field and populate it in the axis descriptor update path (`_updateLaneDescriptors` / `SceneBuildingHelpers`).
- [x] Add mouse interaction handling to `MultiLaneVerticalAxisWidget` (`mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`, optional `keyPressEvent` for Escape).
- [x] Add hit-testing helpers in axis widget to map pixel Y to lane index and insertion slot.
- [x] Add a `laneReorderRequested(QString source_lane_id, int target_slot)` signal to `MultiLaneVerticalAxisWidget`.
- [x] In `DataViewer_Widget`, connect `laneReorderRequested` and translate it into deterministic `DataViewerState` override updates: compute new `lane_order` ranks, call `state->setSeriesLaneOverride()` for all series sharing `source_lane_id`.
- [x] Ensure OpenGL layout refresh is triggered from override changes — already wired: `seriesLaneOverrideChanged` / `laneOverrideChanged` → layout dirty + repaint in `OpenGLWidget::setState`.
- [x] Add rendering affordances in axis widget for drag preview and insertion marker.
- [x] Add tests for drag reorder behavior, state persistence, and mixed analog/event interspersing outcomes.

### Primary Files

- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisStateData.hpp — add `lane_id` to `LaneAxisDescriptor`
- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/MultiLaneVerticalAxisWidget.hpp — add `laneReorderRequested` signal and mouse event overrides
- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/MultiLaneVerticalAxisWidget.cpp — mouse interaction and drag rendering
- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp — connect `laneReorderRequested`, compute new `lane_order` ranks, call `setSeriesLaneOverride`
- src/WhiskerToolbox/DataViewer_Widget/Rendering/SceneBuildingHelpers.cpp — populate `lane_id` on `LaneAxisDescriptor` entries
- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.test.cpp — reorder behavior and persistence tests
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerStateData.hpp — no new fields needed; `SeriesLaneOverrideData.lane_order` is the write target

### Acceptance Criteria

- [x] User can drag a visible lane and drop it between two lanes using the Y-axis widget.
- [x] Reorder result persists in `DataViewerState` via `SeriesLaneOverrideData.lane_order` and survives serialization roundtrip.
- [x] Mixed analog/event lanes can be interspersed interactively without manual JSON edits.
- [x] Reordering a shared overlay lane moves the lane as a unit (all member series sharing `lane_id` receive matching new `lane_order`).
- [x] Full-canvas event/interval behavior remains unchanged.

---

## Phase 4C: Per-Series Relative Ordering UI

### Goal

Allow a user to right-click any individual series currently displayed in the DataViewer tree and interactively place it above or below another named series. This complements axis drag-and-drop (Phase 4b) for high-channel-count datasets (e.g. 384-channel Neuropixel) where a submenu list would be impractical.

### UX Model

- Right-click a leaf series item in `Feature_Tree_Widget` → context menu shows "Place relative to…".
- Action opens `SeriesOrderingDialog`: a `QLineEdit` with `QCompleter` fed by all other currently-displayed series keys, plus a `QComboBox` ("Above" / "Below").
- User types a partial key; the completer filters live matches. On accept the relative placement is committed to `DataViewerState` as a `lane_order` override.
- Reuses the same `(N - i) * 10` rank-assignment algorithm as `_handleLaneReorderRequest`.

### Implementation Tasks

- [x] Create `SeriesOrderingDialog` (`DataViewer_Widget/UI/SeriesOrderingDialog.hpp/.cpp`): `QLineEdit` + `QCompleter` + `QComboBox` ("Above" / "Below") + OK/Cancel. Constructor receives `std::vector<std::string> available_keys`. Returns `(target_key, bool above)` via accessor after `exec()`.
- [x] Extend `DataViewerPropertiesWidget.cpp` context menu handler (line ~356): add `else` branch for leaf items (`hasChildren == false`). Collect all other leaf keys from the tree, show a `QMenu` with "Place relative to…", on trigger open `SeriesOrderingDialog`.
- [x] Add signal to `DataViewerPropertiesWidget.hpp`: `void seriesRelativePlacementRequested(QString source_key, QString target_key, bool above)`.
- [x] Wire signal in `DataViewerWidgetRegistration.cpp` → `DataViewer_Widget::_handleSeriesRelativePlacement`.
- [x] Implement `DataViewer_Widget::_handleSeriesRelativePlacement`: resolve both series' `lane_id`s from overrides (falling back to `"__auto_lane__" + key`), locate the target's visual slot in `_multi_lane_axis_widget->state()->lanes()`, insert source at `slot` (above) or `slot + 1` (below), then assign `lane_order = (N - i) * 10` to all lanes in the new visual order.
- [x] Add tests in `DataViewer_Widget.test.cpp`: verify that after placement, source is immediately above/below target in visual order and that the result survives a `toJson`/`fromJson` roundtrip.

### Primary Files

- `src/WhiskerToolbox/DataViewer_Widget/UI/SeriesOrderingDialog.hpp/.cpp` — new
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.hpp` — new signal
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.cpp` — leaf context menu branch
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerWidgetRegistration.cpp` — new connection
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.hpp` — new private slot
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp` — implement `_handleSeriesRelativePlacement`
- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt` — add new source files
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.test.cpp` — placement behavior tests

### Acceptance Criteria

- [x] Right-clicking a leaf series in the tree shows "Place relative to…".
- [x] Dialog `QCompleter` filters all currently-displayed keys as the user types.
- [x] Committing "Below voltage_3" places the source series immediately below `voltage_3` in visual order.
- [x] Placement result persists in `DataViewerState` and survives a `toJson`/`fromJson` roundtrip.
- [x] Behavior is correct for both AnalogTimeSeries and DigitalEventSeries leaf items.

---

## Phase 4D: JSON Lane Layout Save and Load

### Goal

Allow a developer or user to save the current DataViewer lane configuration — which series are displayed, their colors, and all lane overrides — to a `.dvlayout` JSON file, and reload it later. On load, series listed in the file that exist in the DataManager but are not currently displayed are automatically added.

### JSON Schema

```json
{
  "version": 1,
  "displayed_series": [
    {"key": "voltage_1", "color": "#FF0000"}
  ],
  "series_lane_overrides": {
    "voltage_1": {"lane_id": "", "lane_order": 100, "lane_weight": 1.0, "overlay_mode": "Auto", "overlay_z": 0}
  },
  "lane_overrides": {}
}
```

`series_lane_overrides` and `lane_overrides` reuse the existing `SeriesLaneOverrideData` and `LaneOverrideData` structs directly via `rfl::json`.

### Implementation Tasks

- [ ] Create `LaneLayoutFile.hpp/.cpp` (`DataViewer_Widget/Ordering/`): define `LaneLayoutDisplayedSeries { std::string key; std::string color; }` and `LaneLayoutFile { int version; std::vector<LaneLayoutDisplayedSeries> displayed_series; std::map<std::string, SeriesLaneOverrideData> series_lane_overrides; std::map<std::string, LaneOverrideData> lane_overrides; }`. Implement `serializeLaneLayout()` and `deserializeLaneLayout()` via `rfl::json`.
- [ ] Edit `DataViewerPropertiesWidget.ui`: add a `QGroupBox` "Layout Presets" (after the "Actions" group) with buttons `save_lane_layout_button` ("Save Lane Layout") and `load_lane_layout_button` ("Load Lane Layout").
- [ ] Add signals to `DataViewerPropertiesWidget.hpp`: `void saveLaneLayoutRequested()` and `void loadLaneLayoutRequested()`.
- [ ] Wire buttons in `DataViewerPropertiesWidget.cpp` → emit signals.
- [ ] Wire signals in `DataViewerWidgetRegistration.cpp` → new `DataViewer_Widget` slots.
- [ ] Implement `DataViewer_Widget::_saveLaneLayout()`: collect currently-displayed keys and their colors from `OpenGLWidget`, build `LaneLayoutFile` from `_state->data()` overrides, serialize, open `QFileDialog::getSaveFileName` (filter `*.dvlayout`) and write.
- [ ] Implement `DataViewer_Widget::_loadLaneLayout()`: open `QFileDialog::getOpenFileName`, parse with `deserializeLaneLayout`, for each entry in `displayed_series` call `DataManager::getType(key)` — skip if not found, call `addFeature(key, color)` if not already displayed. Apply all `setSeriesLaneOverride` and `setLaneOverride` entries from the file, then call `updateCanvas()`.
- [ ] Add tests in `LaneLayoutFile.test.cpp` or `DataViewer_Widget.test.cpp`: roundtrip serialize/deserialize; verify that auto-add adds missing series and skips absent ones.

### Primary Files

- `src/WhiskerToolbox/DataViewer_Widget/Ordering/LaneLayoutFile.hpp/.cpp` — new
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.ui` — new GroupBox + buttons
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.hpp` — new signals
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.cpp` — button wiring
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerWidgetRegistration.cpp` — new connections
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.hpp` — new private slots
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp` — implement `_saveLaneLayout`, `_loadLaneLayout`
- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt` — add new source files

### Acceptance Criteria

- [ ] "Save Lane Layout" serializes displayed series, colors, and all overrides to a `.dvlayout` JSON file.
- [ ] "Load Lane Layout" re-adds series that exist in DataManager but are not currently displayed.
- [ ] Series absent from DataManager are silently skipped during load.
- [ ] Lane overrides are fully restored after a save/load roundtrip.
- [ ] Layout roundtrip test passes for a mixed analog plus digital-event configuration.

---

## Phase 4E: CSV Spike-to-Analog Pairing Loader

### Goal

Parse a CSV file where each row records a spike event `(timestamp, digital_channel, analog_channel)`, compute the modal analog channel assignment for each digital channel, and then place each `spikes_N` series adjacent to (or overlaid on) its paired `voltage_M` series. This mirrors the Swindale spike-sorter loader but derives relative ordering between digital and analog series rather than absolute electrode positions.

### CSV Format

Whitespace- or comma-delimited; three columns: `timestamp` (ignored), `digital_channel` (1-based), `analog_channel` (1-based).
Example:
```
0.01493,  8, 20
0.01670, 24,  6
0.04320, 24,  6
```
For each unique `digital_channel`, the `analog_channel` that appears most often (mode) is the pairing. Both channel numbers are converted to 0-based before lookup.

### Placement Modes (configurable per load)

- **Adjacent Below**: `spikes_N` placed in its own lane immediately below `voltage_M`.
- **Adjacent Above**: `spikes_N` placed in its own lane immediately above `voltage_M`.
- **Overlay**: `spikes_N` shares `voltage_M`'s `lane_id` (rendered in the same lane band).

### Implementation Tasks

- [ ] Create `SpikeToAnalogPairingLoader.hpp/.cpp` (`DataViewer_Widget/Ordering/`): define `struct SpikeToAnalogPairing { int digital_channel; int analog_channel; }` (both 0-based). Implement `parseSpikeToAnalogCSV(std::string const& text)` — skip malformed rows, count `(digital_ch → analog_ch)` occurrences, return one `SpikeToAnalogPairing` per unique digital channel (mode analog channel).
- [ ] Create `SpikeToAnalogConfigDialog.hpp/.cpp` (`DataViewer_Widget/UI/`): `QLineEdit` for digital group prefix (default `"spikes_"`), `QLineEdit` for analog group prefix (default `"voltage_"`), `QComboBox` for placement mode ("Adjacent Below" / "Adjacent Above" / "Overlay"), a file-path `QLineEdit` + "Browse…" button, OK/Cancel.
- [ ] Edit `DataViewerPropertiesWidget.ui`: add a `QPushButton` `load_spike_analog_button` ("Load Spike-to-Analog Pairing…") to an appropriate existing or new group box.
- [ ] Add signal to `DataViewerPropertiesWidget.hpp`: `void loadSpikeToAnalogPairingRequested()`.
- [ ] Wire button in `DataViewerPropertiesWidget.cpp` → emit signal.
- [ ] Wire signal in `DataViewerWidgetRegistration.cpp` → `DataViewer_Widget::_loadSpikeToAnalogPairing`.
- [ ] Add `OpenGLWidget::loadSpikeToAnalogPairing(digital_group, analog_group, pairings, PlacementMode)` (`OpenGLWidget.hpp/.cpp`). For **Overlay**: get `voltage_M`'s effective `lane_id` from overrides (or `"__auto_lane__" + key`), set `spikes_N.lane_id` to same and `overlay_mode = Overlay`. For **Adjacent**: read current visual order from `_multi_lane_axis_state->lanes()`, insert each digital auto-lane at `analog_slot + 1` (Below) or `analog_slot` (Above) — process in reverse analog-slot order to avoid index shifts — then assign `lane_order = (N - i) * 10` and call `setSeriesLaneOverride` for each digital key.
- [ ] Implement `DataViewer_Widget::_loadSpikeToAnalogPairing()` and test-accessible `_loadSpikeToAnalogPairingFromText(QString digital_group, QString analog_group, QString text, int placement_mode)` (matching the pattern of `_loadSpikeSorterConfigurationFromText`): show `SpikeToAnalogConfigDialog`, on accept parse CSV, delegate to `openGLWidget->loadSpikeToAnalogPairing(...)`, call `updateCanvas()`.
- [ ] Add tests in `DataViewer_Widget.test.cpp`: CSV parse correctness (mode wins ties deterministically), adjacent below placement, adjacent above placement, overlay lane sharing.

### Primary Files

- `src/WhiskerToolbox/DataViewer_Widget/Ordering/SpikeToAnalogPairingLoader.hpp/.cpp` — new
- `src/WhiskerToolbox/DataViewer_Widget/UI/SpikeToAnalogConfigDialog.hpp/.cpp` — new
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.ui` — new button
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.hpp` — new signal
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewerPropertiesWidget.cpp` — button wiring
- `src/WhiskerToolbox/DataViewer_Widget/DataViewerWidgetRegistration.cpp` — new connection
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.hpp` — new private slots
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp` — implement `_loadSpikeToAnalogPairing`, `_loadSpikeToAnalogPairingFromText`
- `src/WhiskerToolbox/DataViewer_Widget/Rendering/OpenGLWidget.hpp/.cpp` — implement `loadSpikeToAnalogPairing`
- `src/WhiskerToolbox/DataViewer_Widget/CMakeLists.txt` — add new source files
- `src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.test.cpp` — CSV parse and placement tests

### Acceptance Criteria

- [ ] `parseSpikeToAnalogCSV` returns one pairing per unique digital channel (mode analog channel); malformed rows are skipped.
- [ ] "Adjacent Below" places each `spikes_N` in its own lane immediately below its paired `voltage_M`.
- [ ] "Adjacent Above" places each `spikes_N` immediately above its paired `voltage_M`.
- [ ] "Overlay" assigns `spikes_N` to `voltage_M`'s lane band (same `lane_id`).
- [ ] Placements are committed via `setSeriesLaneOverride` and survive a `toJson`/`fromJson` roundtrip.
- [ ] A test-accessible text-based entry point exists for automated tests (no file dialog).

---

## Phase 5: Backward Compatibility, Migration, and Test Matrix

### Goal

Guarantee safe adoption in existing workflows and robust regression coverage.

### Test Matrix

1. Legacy workspace load and save unchanged
2. Analog-only high channel count scaling
3. Mixed analog plus event with no overrides
4. Mixed analog plus event with explicit lane ordering
5. Shared lane overlays with multiple events per analog lane
6. Spike-sorter ordering fallback with no explicit lane_order

### Tasks

- [ ] Add compatibility fixtures for legacy JSON blobs.
- [ ] Add deterministic assertions for ordering precedence.
- [ ] Add overlay lane visibility and culling checks.
- [ ] Re-run existing DataViewer wheel and interaction tests.

### Acceptance Criteria

- [ ] No behavioral regressions in legacy mode.
- [ ] New behavior is deterministic and test-covered.
- [ ] Save and restore for override configurations is stable.

---

## Phase 6: Developer Documentation and Rollout

### Goal

Make the implementation discoverable, maintainable, and easy to continue.

### Tasks

- [ ] Update DataViewer architecture docs with new lane model.
- [ ] Update roadmap docs to cross-link this implementation roadmap.
- [ ] Add a short JSON example in developer docs for lane override usage.
- [ ] Note known constraints and follow-up items.

### Primary Files

- docs/developer/ui/Plots/DataViewerWidget/index.qmd
- docs/developer/ui/Plots/DataViewerWidget/roadmap.qmd
- docs/developer/ui/Plots/DataViewerWidget/mixed_lanes_and_scaling_implementation_roadmap.md

### Acceptance Criteria

- [ ] Developers can implement, test, and extend the feature from docs alone.
- [ ] Follow-up items are clearly separated from first-release scope.

---

## Suggested PR Slicing

1. PR A: Scaling fix plus tests
2. PR B: State schema and serialization tests
3. PR C: CorePlotting grouped and weighted lane logic plus tests
4. PR D: DataViewer integration and axis aggregation
5. PR E: Docs and cleanup

This ordering minimizes integration risk and keeps each review set focused.

## Open Technical Decisions To Resolve Early

1. Event vertical sizing source of truth: event_height, line_thickness, or explicit combined model.
2. Overlay lane label policy: primary analog label only versus composite lane labels.
3. Validation strategy for conflicting lane_order values: strict reject versus deterministic tie-break.
4. Lane drag-drop order authority: lane-level `lane_order` only, or synchronized lane-level + per-series order fields.

## Definition of Done

The work is done when:

- all phase acceptance criteria are met
- all relevant tests pass
- legacy workspaces remain behaviorally unchanged
- mixed-lane configuration is fully serializable and restorable
- developer docs clearly explain implementation and extension paths
