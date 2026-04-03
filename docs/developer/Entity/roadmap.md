# Entity / Lineage Subsystem — Development Roadmap

This document tracks planned but not-yet-implemented work for the Entity and Lineage systems.
Completed phases are recorded in the developer index (`index.qmd`).

## Overall Status

| Phase | Description | Status |
|-------|-------------|--------|
| Core Entity & Lineage types | EntityRegistry, EntityGroupManager, LineageTypes, LineageRegistry | ✅ Complete |
| Generic resolution | IEntityDataSource, LineageResolver | ✅ Complete |
| DataManager integration | DataManagerEntityDataSource, EntityResolver, LineageRecorder | ✅ Complete |
| Transform pipeline wiring | TransformLineageType in metadata; v2 transform registrations | ✅ Complete |
| **Phase 4** | **UI Integration** | ⬜ Not started |
| **Phase 5** | **Testing gaps** | 🔶 Partial |

---

## Phase 4 — UI Integration

None of the items below have been implemented. They convert the backend lineage machinery
into user-visible features.

### 4.1 Click-to-Select with Lineage Resolution

Wire `EntityResolver` into interactive canvas widgets (e.g., a future SpatialOverlayWidget or
the existing ScatterPlotWidget) so that clicking on a derived data point automatically
selects its root source entity.

Sketch of intended wiring:

```cpp
// In a widget that receives canvas click events:
void onPointClicked(std::string const& data_key, TimeFrameIndex time, std::size_t index) {
    EntityResolver resolver(_data_manager);
    auto source_ids = resolver.resolveToRoot(data_key, time, index);
    if (!source_ids.empty()) {
        _selection_context->setSelectedEntities(source_ids);
    }
}
```

**Acceptance criteria:**
- Clicking a derived value (e.g., an area measurement) highlights the originating mask/line/point in all other open views.
- Works across at least two levels of derivation (e.g., peaks ← areas ← masks).
- No crash when the container has no registered lineage (graceful no-op).

### 4.2 Lineage Visualization Widget (Optional)

A read-only debug/exploration widget that renders the lineage graph as a directed acyclic
graph. Useful for understanding complex multi-step transform pipelines.

Proposed display:

```
┌──────────────────────────────────┐
│  Lineage Graph                   │
├──────────────────────────────────┤
│  [masks]                         │
│     │ OneToOneByTime             │
│     ▼                            │
│  [mask_areas]                    │
│     │ AllToOneByTime             │
│     ▼                            │
│  [total_area]                    │
└──────────────────────────────────┘
```

Implementation notes:
- Read lineage chain from `LineageRegistry::getLineageChain()`.
- Label each edge with `getLineageTypeName()`.
- Could be a simple `QTreeWidget` for a first pass; a proper graph layout is optional.
- Should be read-only (no editing of lineage from this widget).

### 4.3 User Documentation

Once the UI features exist, add at least a stub article under `docs/user_guide/` explaining:

- What lineage tracking means for end users.
- How to use click-to-select to trace derived values back to source data.
- How the lineage graph widget (if implemented) can be opened.

Suggested path: `docs/user_guide/lineage/index.qmd`.

---

## Phase 5 — Testing Gaps

### 5.1 Error Path / Malformed Lineage Tests for EntityResolver

The current `EntityResolver.test.cpp` covers happy-path scenarios. The following
error paths lack coverage:

| Scenario | Expected Behaviour |
|----------|-------------------|
| `resolveToRoot()` on a key with a circular lineage chain | Returns empty or breaks cycle safely; does not hang |
| `resolveToRoot()` when an intermediate key is missing from DataManager | Returns empty vector; no exception |
| `resolveByEntityId()` with `EntityMappedLineage` where the EntityId is not in the map | Returns empty vector |
| `resolveByEntityId()` on a container that uses `OneToOneByTime` (wrong call) | Returns empty vector |
| `LineageRecorder::record()` with `TransformLineageType::None` | Throws `std::invalid_argument` |
| `LineageRecorder::recordMultiInput()` with an empty `input_keys` vector | Throws `std::invalid_argument` |

### 5.2 Entity-Bearing Derived Container Integration Tests

There are no integration tests covering the scenario where a derived container has both
its own EntityIds **and** parent lineage (e.g., `LineData` created from `MaskData` via
skeletonisation). Required test cases:

1. Create `MaskData` with EntityIds → run a 1:1 skeletonisation transform → store resulting `LineData` with `EntityMappedLineage`.
2. Verify `resolver.resolveByEntityId("skeleton_lines", derived_id)` returns the correct parent mask EntityId.
3. Verify `resolver.resolveByEntityIdToRoot()` traverses multi-level chains correctly.
4. Verify that `skeleton_lines.getEntityIdsAtTime(T)` (own EntityIds) and `resolver.resolveToSource(...)` (parent EntityIds) return distinct, correct values.

Suggested test file location: `tests/DataManager/Lineage/test_entity_bearing_lineage_integration.test.cpp`.

### 5.3 DataManagerEntityDataSource — RaggedAnalogTimeSeries Coverage

Verify that the `getElementCount()` and `getAllEntityIdsAtTime()` dispatches work correctly
for the `RaggedAnalogTimeSeries` backend (this type required a bug fix in Phase 3 to be
recognised by `DataManager::getType()`). A targeted regression test would prevent regressions.
