# DataViewer Ordering Abstractions Implementation Roadmap

## Purpose

This roadmap defines a focused refactor that separates ordering concerns for DataViewer stacked lanes.

It introduces a reusable ordering model that decouples:

1. metadata ingestion (for example, spike-sorter channel positions)
2. sortable value derivation
3. ordering policy resolution
4. layout request assembly

This roadmap is intended to be completed before Phase 4b in mixed lanes and scaling work.

## Relationship to Mixed-Lanes Roadmap

This document is a prerequisite for interactive lane drag-and-drop (Phase 4b) in:

- docs/developer/ui/Plots/DataViewerWidget/mixed_lanes_and_scaling_implementation_roadmap.md

Rationale:

- Phase 4b requires a stable source of truth for lane ordering.
- Current ordering behavior still mixes policy and ingestion concerns.
- A clean ordering abstraction reduces coupling and makes drag/drop persistence rules clearer.

## Scope

In scope:

- Introduce ordering-domain abstractions for stackable lane series.
- Separate ingestion from sorting policy.
- Add policy composition with deterministic precedence.
- Add a persisted constraints model in DataViewer state.
- Keep current behavior parity before enabling new constraint types.

Out of scope:

- UI for authoring all rule types (JSON/state API first).
- Full-canvas series reordering semantics.
- Phase 4b drag/drop interaction implementation itself.

## Locked Decisions

1. Constraint scope (first release): stackable series only.
2. Persistence model: ordering rules and constraints are serialized in DataViewerState.
3. Conflict policy: explicit lane_order wins; constraints apply to unresolved relative ordering.

## Success Criteria

1. OpenGLWidget no longer owns ordering policy details.
2. Spike-sorter support is one provider in a generic sortable-ranking model.
3. Ordering remains deterministic under mixed explicit, relational, and fallback rules.
4. Existing Phase 4 behavior remains unchanged after extraction.
5. The resulting abstraction directly supports Phase 4b state updates.

## High-Level Delivery Order

1. Phase A: Baseline behavior lock
2. Phase B: Ordering domain model extraction
3. Phase C: Ingestion and ranking separation
4. Phase D: Policy resolver introduction
5. Phase E: Relational constraints model (stackable-only)
6. Phase F: UI and renderer decoupling follow-through
7. Phase G: Verification matrix and hardening

---

## Phase A: Baseline Behavior Lock

### Goal

Create a stable regression harness before refactoring ordering internals.

### Tasks

- [x] Add integration tests for explicit lane_order precedence.
- [x] Add integration tests for analog-only spike-sorter fallback.
- [x] Add deterministic tie-break tests for mixed analog/event stackable ordering.
- [x] Add tests for shared-lane overlay ordering stability.

### Acceptance Criteria

- [x] Current ordering behavior is reproducible via tests.
- [x] Refactor can be validated with behavior parity checks.

---

## Phase B: Ordering Domain Model Extraction

### Goal

Introduce a normalized ordering input model independent of file format details.

### Tasks

- [x] Add an internal ordering item model (series identity, stackability, lane hints, explicit order, normalized group/channel identity).
- [x] Extract group/channel key parsing into a dedicated identity utility.
- [x] Keep LayoutRequestBuilder as a thin assembly layer from resolved ordering to CorePlotting SeriesInfo.

### Acceptance Criteria

- [x] LayoutRequestBuilder stops containing identity parsing logic.
- [x] Ordering inputs are represented by one normalized internal type.

---

## Phase C: Ingestion and Ranking Separation

### Goal

Separate spike-sorter file parsing from ordering-policy execution.

### Tasks

- [x] Split spike-sorter module responsibilities into parser and rank adapter layers.
- [x] Convert channel-position metadata into provider-style sortable ranks.
- [x] Replace direct sort-by-file-format logic in ordering flow with provider output.

### Acceptance Criteria

- [x] Parser can be used without invoking ordering.
- [x] Ordering can consume non-spike-sorter rank providers.

---

## Phase D: Policy Resolver Introduction

### Goal

Centralize ordering precedence logic in a dedicated resolver.

### Resolver precedence

1. explicit lane_order
2. relational constraints (when present)
3. fallback sortable provider ranks
4. deterministic tie-break (series type precedence then stable key)

### Tasks

- [x] Introduce resolver API that returns resolved ordering/ranks.
- [x] Move comparator layering out of LayoutRequestBuilder.
- [x] Add lightweight diagnostics for why each series received its position.

### Acceptance Criteria

- [x] LayoutRequestBuilder delegates policy decisions to resolver.
- [x] Resolver output is deterministic and test-covered.

---

## Phase E: Relational Constraints Model (Stackable-Only)

### Goal

Add persisted relative-order constraints to support richer semantics.

### Example target use case

- Place DigitalEventSeries 2 and 3 above AnalogTimeSeries channel 7.

### Tasks

- [x] Add serialized constraints schema to DataViewerStateData.
- [x] Add state API for set/clear/query constraints.
- [x] Add normalization and validation in DataViewerState.
- [x] Implement stable topological ordering for stackable-series constraints.
- [x] Define deterministic behavior for cycles/conflicts.

### Acceptance Criteria

- [x] Constraints survive serialization roundtrip.
- [x] Constraints combine correctly with explicit lane_order precedence.
- [x] Constraint conflicts produce deterministic outcomes.

---

## Phase F: UI and Renderer Decoupling Follow-Through

### Goal

Ensure ownership and signal flow align with EditorState architecture.

### Tasks

- [x] **F1 – Extract metadata types into a dedicated header.**
  Create `ChannelPositionMetadata.hpp` containing `ChannelPosition`, `SpikeSorterConfigMap`,
  `SortableRankMap`, `SortableRankProvider`, and `NormalizedSeriesIdentity`.
  `SpikeSorterConfigLoader.hpp` becomes a pure parser header that includes
  `ChannelPositionMetadata.hpp`. Future loaders reference the metadata header only,
  not the spike-sorter parser.

- [ ] **F2 – Move `_spike_sorter_configs` ownership from renderer to state.**
  Remove `SpikeSorterConfigMap _spike_sorter_configs` and its load/clear methods from
  `OpenGLWidget`. Add a non-serializable runtime map to `DataViewerState` (or a
  companion runtime-state struct). Route `loadSpikeSorterConfiguration()` and
  `clearSpikeSorterConfiguration()` calls through state, emitting `orderingConstraintsChanged`
  (or a new `rankProviderChanged`) to trigger layout invalidation.

- [ ] **F3 – Replace `spike_sorter_configs` in `LayoutRequestBuildContext` with a `SortableRankProvider`.**
  `LayoutRequestBuildContext::spike_sorter_configs` (type `SpikeSorterConfigMap const &`)
  becomes `SortableRankProvider rank_provider` (or a pre-built `SortableRankMap`).
  The caller (OpenGLWidget) binds `buildSpikeSorterSortableRanks` to the provider using
  the state-owned config map. The context is then source-agnostic.

- [ ] **F4 – Wire `SortableRankProvider` through `applyOrderingRules`.**
  Replace the direct `buildSpikeSorterSortableRanks` call in `applyOrderingRules` with
  an invocation of the provider from the context. This makes the already-declared
  `SortableRankProvider` type alias functional rather than decorative.

- [ ] **F5 – Remove the legacy `TimeSeriesDataStore::buildLayoutRequest()` spike-sorter path.**
  `TimeSeriesDataStore::buildLayoutRequest(float, float, SpikeSorterConfigMap const &)` still
  contains its own spike-sorter ordering call that bypasses the resolver entirely. Remove this
  overload (or strip the ordering logic from it) and redirect any remaining callers to
  `DataViewer::buildLayoutRequest()`.

- [ ] **F6 – Fix the `allCandidatesAnalog` gate in `applyOrderingRules`.**
  The current guard disables all fallback rank ordering the moment any stacked
  `DigitalEventSeries` is present. Instead, apply the rank provider only to analog candidates;
  digital candidates should still participate in constraints and deterministic tie-break
  regardless of whether a rank provider is present.

### Acceptance Criteria

- [ ] OpenGLWidget has no policy-specific ingestion dependencies.
- [ ] `LayoutRequestBuildContext` contains no spike-sorter-specific field.
- [ ] Spike-sorter ranks apply to analog candidates even when stacked digital series are present.
- [ ] Ordering changes flow through state and update layout predictably.
- [ ] `TimeSeriesDataStore` legacy ordering path is removed.

---

## Phase G: Verification Matrix and Hardening

### Goal

Provide robust confidence for Phase 4b and future ordering features.

### Verification matrix

1. Explicit lane_order only
2. Fallback ranking only
3. Constraints only
4. Explicit plus constraints
5. Constraints plus fallback
6. Explicit plus constraints plus fallback
7. Shared-lane overlays with mixed series types

### Tasks

- [ ] Add resolver-level unit tests for composition and conflict behavior.
- [ ] Add DataViewer integration tests for end-to-end ordering outcomes.
- [ ] Re-run relevant DataViewer and CorePlotting layout tests.

### Acceptance Criteria

- [ ] No regressions in existing Phase 4 behavior.
- [ ] Policy composition logic is deterministic and documented.
- [ ] Phase 4b can be implemented on top of stable ordering abstractions.

---

## Suggested PR Slicing

1. PR 1: Baseline tests and ordering domain scaffolding
2. PR 2: Spike-sorter parser/rank split
3. PR 3: Policy resolver extraction
4. PR 4: Relational constraints persistence and solver
5. PR 5: Integration cleanup and docs

## Definition of Done

The ordering-abstractions roadmap is complete when:

- all phase acceptance criteria are met
- ordering policy is modular and ingestion-agnostic
- deterministic tests cover precedence and conflicts
- mixed-lanes Phase 4 behavior remains intact
- the codebase is ready to begin Phase 4b interactive drag/drop work
