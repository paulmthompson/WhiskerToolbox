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
7. Phase 5: Compatibility and migration testing
8. Phase 6: Documentation and handoff

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
- Ordering abstractions roadmap: not started
- Phase 4b: not started
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
- Drag-and-drop should mutate the same source of truth (`series_lane_overrides` / `lane_overrides`) rather than introducing a parallel ordering path.

### UX Model (first release)

- Drag handle surface: `MultiLaneVerticalAxisWidget` lane label area.
- Start drag: left-click press within a visible lane hit region.
- During drag: show insertion marker between target lanes and ghost highlight on dragged lane.
- Drop: commit a new lane ordering by updating lane-level order, then recompute layout.
- Cancel: Escape key or mouse release outside valid target keeps existing order.

### Data/State Model Requirements

1. Add stable lane identity to axis descriptors:
	- Extend `LaneAxisDescriptor` with `lane_id` (stable key used by overrides).
	- Keep `label` as display-only.

2. Add optional lane order at lane-level state:
	- Introduce lane-level order field in lane metadata (recommended: `lane_order` in `LaneOverrideData`).
	- Keep per-series `lane_order` for explicit series-level overrides; lane-level order is preferred for drag/drop lane moves.

3. Ordering precedence update:
	1. explicit lane-level order (interactive drag/drop result)
	2. explicit per-series order override
	3. analog spike-sorter fallback (analog-only/no explicit overrides)
	4. legacy type-based fallback

### Implementation Tasks

- [ ] Extend axis descriptor payload with stable lane identity and any needed drag metadata.
- [ ] Add mouse interaction handling to `MultiLaneVerticalAxisWidget` (`mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`, optional `keyPressEvent` for Escape).
- [ ] Add hit-testing helpers in axis widget to map pixel Y to lane index and insertion slot.
- [ ] Add a lane reorder callback/signal from axis widget to DataViewer (e.g., `laneReorderRequested(source_lane_id, target_slot)`).
- [ ] In `DataViewer_Widget`, translate reorder requests into deterministic `DataViewerState` override updates.
- [ ] Ensure OpenGL layout refresh is triggered from override changes (signal wiring for `seriesLaneOverrideChanged` / `laneOverrideChanged`).
- [ ] Add rendering affordances in axis widget for drag preview and insertion marker.
- [ ] Add tests for drag reorder behavior, state persistence, and mixed analog/event interspersing outcomes.

### Primary Files

- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisStateData.hpp
- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/MultiLaneVerticalAxisWidget.hpp
- src/WhiskerToolbox/Plots/Common/MultiLaneVerticalAxisWidget/MultiLaneVerticalAxisWidget.cpp
- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.cpp
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerStateData.hpp
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerState.hpp
- src/WhiskerToolbox/DataViewer_Widget/Core/DataViewerState.cpp
- src/WhiskerToolbox/DataViewer_Widget/Rendering/OpenGLWidget.cpp
- src/CorePlotting/Layout/StackedLayoutStrategy.cpp
- src/WhiskerToolbox/DataViewer_Widget/UI/DataViewer_Widget.test.cpp

### Acceptance Criteria

- [ ] User can drag a visible lane and drop it between two lanes using the Y-axis widget.
- [ ] Reorder result persists in `DataViewerState` and survives serialization roundtrip.
- [ ] Mixed analog/event lanes can be interspersed interactively without manual JSON edits.
- [ ] Reordering a shared overlay lane moves the lane as a unit (all member series remain grouped).
- [ ] Full-canvas event/interval behavior remains unchanged.

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
