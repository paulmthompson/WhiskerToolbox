# Lineage System Implementation Plan

## Overview

This document outlines the implementation plan for a **Container Lineage** system that tracks relationships between data containers and enables **Entity Propagation** - the ability to trace derived values back to their source EntityIds.

### Problem Statement

When data is transformed (e.g., `MaskData → AnalogTimeSeries` via MaskArea), users need to:
1. Query "which source entities produced this derived value?"
2. Filter derived data and trace back to source entities
3. Create groups from filtered results

Currently, this requires carrying EntityIds through every transformation, which:
- Adds complexity to transforms
- Creates storage overhead in containers that don't naturally have entities
- Couples transforms to entity tracking logic

### Solution

Decouple **entity storage** from **entity resolution**:
- Source containers (MaskData, LineData, PointData) store EntityIds (existing behavior)
- Derived containers store lightweight **Lineage** metadata
- An **EntityResolver** uses lineage to trace back to source entities on-demand

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            DataManager                                   │
│                                                                          │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────────────────┐ │
│  │ Source Data     │  │ Derived Data     │  │ Lineage Registry       │ │
│  │                 │  │                  │  │                        │ │
│  │ MaskData        │  │ AnalogTimeSeries │  │ "mask_areas" →         │ │
│  │ (has EntityIds) │  │ (no EntityIds)   │  │   OneToOneByTime{      │ │
│  │                 │  │                  │  │     source: "masks"    │ │
│  │ LineData        │  │ DigitalEvents    │  │   }                    │ │
│  │ (has EntityIds) │  │ (has EntityIds)  │  │                        │ │
│  └─────────────────┘  └──────────────────┘  │ "small_areas" →        │ │
│                                              │   SubsetLineage{       │ │
│                                              │     source: "masks",   │ │
│                                              │     entities: {1,3,7}  │ │
│                                              │   }                    │ │
│                                              └────────────────────────┘ │
│                                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                       EntityResolver                              │  │
│  │  resolve("mask_areas", TimeFrameIndex(5))                        │  │
│  │    → Looks up lineage → Finds source "masks"                     │  │
│  │    → Returns masks.getEntityIdsAtTime(5)                         │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Core Lineage Types (Week 1)

### 1.1 Create Lineage Type Definitions

**File:** `src/DataManager/Lineage/LineageTypes.hpp`

```cpp
namespace WhiskerToolbox::Lineage {

// No lineage - this is source data or data loaded from file
struct Source {};

// 1:1 mapping: derived[time, idx] ← source[time, idx]
struct OneToOneByTime {
    std::string source_key;
};

// N:1 mapping: derived[time] ← ALL source entities at time
struct AllToOneByTime {
    std::string source_key;
};

// Subset mapping: derived came from specific subset of source
struct SubsetLineage {
    std::string source_key;
    std::unordered_set<EntityId> included_entities;
    // Optional: intermediate container this was filtered from
    std::optional<std::string> filtered_from_key;
};

// Multi-source: derived from multiple parent containers
struct MultiSourceLineage {
    std::vector<std::string> source_keys;
    enum class CombineStrategy { ZipByTime, Cartesian, Custom } strategy;
};

// Explicit per-element contributors (highest flexibility, highest storage)
struct ExplicitLineage {
    std::string source_key;
    // contributors[derived_idx] = vector of source EntityIds
    std::vector<std::vector<EntityId>> contributors;
};

// Type-erased variant
using Descriptor = std::variant<
    Source,
    OneToOneByTime,
    AllToOneByTime,
    SubsetLineage,
    MultiSourceLineage,
    ExplicitLineage
>;

} // namespace WhiskerToolbox::Lineage
```

### 1.1b Containers With Own EntityIds AND Parent Lineage

Some derived containers have **both**:
- Their own EntityIds (because they're entity-bearing types like LineData)
- Parent lineage (because they were derived from another container)

**Example:** `LineData` created from `MaskData` via skeletonization
- Each Line2D gets its own EntityId (it's a distinct line entity)
- But we also want to know "which Mask produced this Line?"

**Solution:** Lineage tracks parent relationship; own EntityIds are orthogonal.

```cpp
// LineData from MaskData (1:1 transform)
// 
// MaskData "masks":
//   Time T0: Mask A (EntityId=100)
//   Time T1: Mask B (EntityId=101)
//
// LineData "skeleton_lines" (derived):
//   Time T0: Line X (EntityId=200) ← came from Mask A (EntityId=100)
//   Time T1: Line Y (EntityId=201) ← came from Mask B (EntityId=101)
//
// The LineData has:
//   - Own EntityIds: 200, 201 (for selecting/grouping lines)
//   - Lineage: OneToOneByTime{source_key="masks"}
//
// Resolution:
//   resolver.resolveToSource("skeleton_lines", T0) → [EntityId=100] (the mask)
//   skeleton_lines.getEntityIdsAtTime(T0) → [EntityId=200] (the line itself)
```

**Key Insight:** These are two different questions:
1. "What is this line's identity?" → Own EntityId (200)
2. "Where did this line come from?" → Parent EntityId via lineage (100)

Both are valid and serve different purposes:
- Own EntityId: For grouping lines, selecting lines, line-specific operations
- Parent EntityId: For tracing provenance, understanding data flow

### 1.1c DigitalEventSeries and DigitalIntervalSeries

These are **non-ragged** but still have EntityIds:

```cpp
// DigitalEventSeries storage:
std::vector<TimeFrameIndex> _data;      // Event times
std::vector<EntityId> _entity_ids;       // Parallel EntityId vector

// DigitalIntervalSeries storage (similar):
std::vector<TimeFrameInterval> _data;   // Intervals
std::vector<EntityId> _entity_ids;       // Parallel EntityId vector
```

**Scenarios:**

**A. Source DigitalEventSeries (e.g., loaded from file)**
- Has own EntityIds
- Lineage: `Source{}`

**B. Derived DigitalEventSeries (e.g., from AnalogTimeSeries threshold)**
- Has own EntityIds (each event is an entity)
- Lineage: depends on source

```cpp
// AnalogTimeSeries "signal" (no EntityIds, no lineage - raw sensor data)
// → Threshold transform →
// DigitalEventSeries "threshold_events"
//   - Has own EntityIds (one per event)
//   - Lineage: Source{} (analog doesn't have entities to trace to)

// AnalogTimeSeries "mask_areas" (no EntityIds, lineage to "masks")
// → Threshold transform →
// DigitalEventSeries "area_threshold_events"  
//   - Has own EntityIds (one per event)
//   - Lineage: OneToOneByTime{source_key="mask_areas"}
//   - Transitive resolution: event → mask_areas time → mask EntityId
```

**C. Derived from entity-bearing source**

```cpp
// DigitalEventSeries "lick_events" (source, has EntityIds)
// → Gather events in intervals →
// New DigitalEventSeries "gathered_licks"
//   - Has own EntityIds (new identities for gathered events)
//   - Lineage: could be SubsetLineage or ExplicitLineage to original events
```

### 1.1d Container Classification

| Container Type | Has Own EntityIds | Can Have Parent Lineage | Notes |
|---------------|-------------------|------------------------|-------|
| MaskData | ✅ Yes | ✅ Yes | Ragged, entity-bearing |
| LineData | ✅ Yes | ✅ Yes | Ragged, entity-bearing |
| PointData | ✅ Yes | ✅ Yes | Ragged, entity-bearing |
| DigitalEventSeries | ✅ Yes | ✅ Yes | Dense, entity-bearing |
| DigitalIntervalSeries | ✅ Yes | ✅ Yes | Dense, entity-bearing |
| AnalogTimeSeries | ❌ No | ✅ Yes | Dense, no entities |
| TensorData | ❌ No | ✅ Yes | Dense, no entities |
| MediaData | ❌ No | ❌ No | Special case |

### 1.1e Updated Lineage Types for Entity-Bearing Derived Containers

For containers that have **both** own EntityIds and parent lineage, we need to map between them:

```cpp
namespace WhiskerToolbox::Lineage {

// Extended lineage for entity-bearing derived containers
// Maps: derived EntityId → parent EntityId(s)
struct EntityMappedLineage {
    std::string source_key;
    
    // Mapping from this container's EntityIds to parent's EntityIds
    // derived_entity_id → [parent_entity_ids]
    std::unordered_map<EntityId, std::vector<EntityId>> entity_mapping;
    
    // For 1:1 transforms, this is a simple bijection
    // For N:1, multiple parent EntityIds per derived EntityId
    // For 1:N, same parent EntityId appears for multiple derived EntityIds
};

// Alternative: Implicit mapping (no storage, computed on demand)
// Works when both containers have same time structure
struct ImplicitEntityMapping {
    std::string source_key;
    
    enum class Cardinality {
        OneToOne,    // derived[time, i] ← source[time, i]
        AllToOne,    // derived[time, 0] ← all source[time, *]
        OneToAll     // derived[time, *] ← source[time, 0]
    } cardinality = Cardinality::OneToOne;
};

// Updated variant
using Descriptor = std::variant<
    Source,
    OneToOneByTime,
    AllToOneByTime,
    SubsetLineage,
    MultiSourceLineage,
    ExplicitLineage,
    EntityMappedLineage,    // NEW: explicit entity-to-entity mapping
    ImplicitEntityMapping   // NEW: implicit mapping by position
>;

} // namespace WhiskerToolbox::Lineage
```

### 1.2 Create Lineage Registry

**File:** `src/DataManager/Lineage/LineageRegistry.hpp`

```cpp
class LineageRegistry {
public:
    void setLineage(std::string const& data_key, Lineage::Descriptor lineage);
    [[nodiscard]] std::optional<Lineage::Descriptor> getLineage(std::string const& data_key) const;
    void removeLineage(std::string const& data_key);
    void clear();
    
    // Query helpers
    [[nodiscard]] bool hasLineage(std::string const& data_key) const;
    [[nodiscard]] bool isSource(std::string const& data_key) const;
    [[nodiscard]] std::vector<std::string> getSourceKeys(std::string const& data_key) const;
    
private:
    std::unordered_map<std::string, Lineage::Descriptor> _lineages;
};
```

### 1.3 Integrate with DataManager

**Modifications to:** `src/DataManager/DataManager.hpp`

```cpp
class DataManager {
    // ... existing members ...
    
    // NEW: Lineage tracking
    std::unique_ptr<LineageRegistry> _lineage_registry;
    
public:
    // NEW: Lineage API
    [[nodiscard]] LineageRegistry* getLineageRegistry();
    [[nodiscard]] LineageRegistry const* getLineageRegistry() const;
    
    // Convenience: set data with lineage in one call
    template<typename T>
    void setDerivedData(std::string const& key, 
                        std::shared_ptr<T> data,
                        TimeKey const& time_key,
                        Lineage::Descriptor lineage);
};
```

### Deliverables - Phase 1
- [ ] `LineageTypes.hpp` with all lineage variant types
- [ ] `LineageRegistry.hpp/cpp` with storage and queries
- [ ] `DataManager` integration
- [ ] Unit tests for lineage storage and retrieval
- [ ] Documentation comments

---

## Phase 2: Entity Resolver (Week 2)

### 2.1 Create EntityResolver

**File:** `src/DataManager/Lineage/EntityResolver.hpp`

```cpp
class EntityResolver {
public:
    explicit EntityResolver(DataManager const* dm);
    
    /**
     * @brief Resolve derived element to source EntityIds
     * 
     * @param data_key Key of the (possibly derived) container
     * @param time TimeFrameIndex of element
     * @param local_index For ragged containers, which element at that time
     * @return Vector of source EntityIds (empty if no lineage or not found)
     */
    [[nodiscard]] std::vector<EntityId> resolveToSource(
        std::string const& data_key,
        TimeFrameIndex time,
        int local_index = 0) const;
    
    /**
     * @brief Resolve by this container's EntityId to parent EntityIds
     * 
     * For entity-bearing derived containers (LineData from MaskData),
     * maps from the derived entity's ID to its parent entity's ID.
     * 
     * @param data_key Key of the derived container
     * @param derived_entity_id EntityId of element in this container
     * @return Vector of parent EntityIds
     */
    [[nodiscard]] std::vector<EntityId> resolveByEntityId(
        std::string const& data_key,
        EntityId derived_entity_id) const;
    
    /**
     * @brief Resolve all the way to root source containers
     * 
     * Traverses lineage chain until reaching Source containers.
     */
    [[nodiscard]] std::vector<EntityId> resolveToRoot(
        std::string const& data_key,
        TimeFrameIndex time,
        int local_index = 0) const;
    
    /**
     * @brief Resolve by EntityId all the way to root
     * 
     * For entity-bearing derived containers, traces the full lineage.
     */
    [[nodiscard]] std::vector<EntityId> resolveByEntityIdToRoot(
        std::string const& data_key,
        EntityId derived_entity_id) const;
    
    /**
     * @brief Find source entities matching a predicate on derived data
     * 
     * @param data_key Key of derived container
     * @param predicate Function returning true for elements to include
     * @return Set of source EntityIds that contributed to matching elements
     */
    template<typename Predicate>
    [[nodiscard]] std::unordered_set<EntityId> findSourceEntitiesWhere(
        std::string const& data_key,
        Predicate pred) const;
    
    /**
     * @brief Get lineage chain for a data key
     * 
     * @return Vector of data keys from derived to source(s)
     */
    [[nodiscard]] std::vector<std::string> getLineageChain(
        std::string const& data_key) const;

private:
    DataManager const* _dm;
    
    // Dispatch to appropriate resolution strategy
    std::vector<EntityId> resolveOneToOne(
        OneToOneByTime const& lineage, TimeFrameIndex time, int local_index) const;
    std::vector<EntityId> resolveAllToOne(
        AllToOneByTime const& lineage, TimeFrameIndex time) const;
    std::vector<EntityId> resolveSubset(
        SubsetLineage const& lineage, TimeFrameIndex time) const;
    std::vector<EntityId> resolveMultiSource(
        MultiSourceLineage const& lineage, TimeFrameIndex time) const;
    std::vector<EntityId> resolveExplicit(
        ExplicitLineage const& lineage, size_t derived_index) const;
    std::vector<EntityId> resolveEntityMapped(
        EntityMappedLineage const& lineage, EntityId derived_entity_id) const;
    std::vector<EntityId> resolveImplicit(
        ImplicitEntityMapping const& lineage, TimeFrameIndex time, int local_index) const;
};
```

### 2.2 Integration with ViewRaggedStorage

Enhance `ViewRaggedStorage` to work seamlessly with lineage:

```cpp
// ViewRaggedStorage already has:
// - _source: shared_ptr to source storage
// - _indices: which source elements are included
// - getEntityIdsAtTime(): returns EntityIds for view elements at time

// NEW: Helper to create SubsetLineage from view
SubsetLineage createLineageFromView(
    std::string const& source_key,
    ViewRaggedStorage<TData> const& view);
```

### Deliverables - Phase 2
- [ ] `EntityResolver.hpp/cpp` with all resolution strategies
- [ ] Helper functions for ViewRaggedStorage ↔ SubsetLineage
- [ ] Unit tests for each resolution strategy
- [ ] Integration tests with DataManager

---

## Phase 3: Transform Pipeline Integration (Week 3)

### 3.1 Automatic Lineage in Transform Pipeline

Modify the V2 transform pipeline to optionally track lineage:

**File:** `src/DataManager/transforms/v2/core/TransformPipeline.hpp`

```cpp
template<typename InputContainer, typename OutputContainer = void>
class TransformPipeline {
    // ... existing ...
    
    // NEW: Lineage tracking mode
    bool _track_lineage = false;
    
public:
    TransformPipeline& withLineageTracking(bool enable = true) {
        _track_lineage = enable;
        return *this;
    }
    
    // Execute and register lineage
    auto executeWithLineage(
        InputContainer const& input,
        std::string const& input_key,
        std::string const& output_key,
        DataManager* dm) const;
};
```

### 3.2 Lineage Inference for Common Transforms

Create helpers that automatically determine lineage type:

```cpp
namespace WhiskerToolbox::Lineage {

// Infer lineage from transform characteristics
template<typename Transform>
Descriptor inferLineage(std::string const& source_key) {
    if constexpr (Transform::is_element_transform) {
        // Element transforms are 1:1
        return OneToOneByTime{source_key};
    } else if constexpr (Transform::is_reduction) {
        // Reductions are N:1
        return AllToOneByTime{source_key};
    } else {
        // Default: no lineage inference
        return Source{};
    }
}

} // namespace WhiskerToolbox::Lineage
```

### Deliverables - Phase 3
- [ ] Pipeline lineage tracking option
- [ ] Lineage inference helpers
- [ ] Integration tests with real transforms (MaskArea, etc.)
- [ ] Update IMPLEMENTATION_GUIDE.md with lineage examples

---

## Phase 4: UI Integration (Week 4)

### 4.1 EntityResolver in Analysis Dashboard

Enable tracing from visualization clicks back to source data:

```cpp
// In SpatialOverlayWidget or similar
void onPointClicked(ClickEvent const& event) {
    // Find which entity was clicked
    auto [data_key, time, local_index] = hitTest(event.pos);
    
    // Resolve to source entities
    EntityResolver resolver(dm);
    auto source_entities = resolver.resolveToRoot(data_key, time, local_index);
    
    // Add to group or highlight
    if (!source_entities.empty()) {
        dm->getEntityGroupManager()->addToGroup("selection", source_entities);
    }
}
```

### 4.2 Lineage Visualization Widget (Optional)

A debug/exploration widget showing the lineage graph:

```
┌─────────────────────────────────────────┐
│ Lineage Graph                           │
├─────────────────────────────────────────┤
│                                         │
│  [masks] ──────────┐                    │
│     │              │                    │
│     │ 1:1          │                    │
│     ▼              │                    │
│  [mask_areas]      │ subset             │
│     │              │ {E1,E3,E7}         │
│     │ filter       │                    │
│     ▼              ▼                    │
│  [small_areas] ◄───┘                    │
│                                         │
└─────────────────────────────────────────┘
```

### Deliverables - Phase 4
- [ ] EntityResolver usage in Analysis Dashboard widgets
- [ ] Click-to-select with lineage resolution
- [ ] (Optional) Lineage visualization widget
- [ ] User documentation

---

## Phase 5: Testing & Documentation (Week 5)

### 5.0 Concrete Examples: Entity-Bearing Derived Containers

Before diving into testing, let's trace through the key scenarios:

#### Example A: LineData from MaskData (Both Have EntityIds)

```cpp
// Source: MaskData "masks"
// Time T0: Mask (EntityId=100), Mask (EntityId=101)
// Time T1: Mask (EntityId=102)

// Transform: Skeletonize each mask → Line
auto lines = skeletonize(masks);  // 1:1 transform

// Derived: LineData "skeleton_lines"
// Time T0: Line (EntityId=200), Line (EntityId=201)  // NEW EntityIds!
// Time T1: Line (EntityId=202)

// Register lineage with entity mapping
dm->getLineageRegistry()->setLineage("skeleton_lines", 
    Lineage::ImplicitEntityMapping{
        .source_key = "masks",
        .cardinality = Cardinality::OneToOne
    });

// Query 1: "What is this line?" → Use line's own EntityId
auto line_id = lines->getEntityIdsAtTime(T0).front();  // EntityId=200

// Query 2: "What mask produced this line?" → Use lineage
EntityResolver resolver(dm);
auto parent_ids = resolver.resolveByEntityId("skeleton_lines", EntityId(200));
// Returns: [EntityId=100]

// Query 3: "Find all lines from masks with area < 50"
// First: get mask EntityIds with small area (via mask_areas lineage)
auto small_mask_ids = resolver.findSourceEntitiesWhere("mask_areas", 
    [](float area) { return area < 50.0f; });
// Returns: {EntityId=100, EntityId=102}

// Then: find lines derived from those masks (reverse lookup)
auto small_lines = resolver.findDerivedFromSources("skeleton_lines", small_mask_ids);
// Returns: {EntityId=200, EntityId=202}
```

#### Example B: DigitalEventSeries from AnalogTimeSeries (Chain Resolution)

```cpp
// Chain: MaskData → AnalogTimeSeries (areas) → DigitalEventSeries (threshold crossings)

// 1. MaskData "masks" (source)
//    Has EntityIds: 100, 101, 102, ...

// 2. AnalogTimeSeries "mask_areas" (derived, no own EntityIds)
//    Lineage: OneToOneByTime{source_key="masks"}
//    Times: T0, T1, T2, ...

// 3. DigitalEventSeries "area_peaks" (derived, has own EntityIds)
//    Events at: T0, T2 (where area peaked above threshold)
//    Own EntityIds: 300, 301
//    Lineage: OneToOneByTime{source_key="mask_areas"}

// Query: "Event 300 at T0 - which mask caused this?"
EntityResolver resolver(dm);

// Step 1: Resolve event → analog time series (no entity, just time)
// Step 2: Resolve analog → mask (has entity)
auto root_ids = resolver.resolveByEntityIdToRoot("area_peaks", EntityId(300));
// Internally:
//   1. Find time for event 300 → T0
//   2. Look up "mask_areas" lineage → OneToOneByTime to "masks"
//   3. Get mask EntityId at T0 → EntityId=100
// Returns: [EntityId=100]
```

#### Example C: DigitalIntervalSeries Gathered from Events

```cpp
// Source: DigitalEventSeries "lick_events"
//   Events: T5 (EntityId=400), T10 (EntityId=401), T15 (EntityId=402), T50 (EntityId=403)

// Transform: Gather events within trial intervals
// Trial 1: [T0, T20] contains events at T5, T10, T15
// Trial 2: [T40, T60] contains event at T50

// Result: DigitalIntervalSeries "trial_lick_intervals"
//   Interval [T0, T20] (EntityId=500) ← gathered from events 400, 401, 402
//   Interval [T40, T60] (EntityId=501) ← gathered from event 403

// This is N:1 aggregation with entity mapping
dm->getLineageRegistry()->setLineage("trial_lick_intervals",
    Lineage::EntityMappedLineage{
        .source_key = "lick_events",
        .entity_mapping = {
            {EntityId(500), {EntityId(400), EntityId(401), EntityId(402)}},
            {EntityId(501), {EntityId(403)}}
        }
    });

// Query: "Which lick events are in trial interval 500?"
auto source_events = resolver.resolveByEntityId("trial_lick_intervals", EntityId(500));
// Returns: [EntityId=400, EntityId=401, EntityId=402]
```

#### Example D: ViewRaggedStorage as Implicit Lineage

```cpp
// Source: MaskData "masks" with OwningRaggedStorage
//   All masks with EntityIds: 100, 101, 102, 103, 104

// Create filtered view (subset)
auto view_storage = std::make_shared<ViewRaggedStorage<Mask2D>>(
    masks->getStorage().getOwning()
);
view_storage->filterByEntityIds({EntityId(100), EntityId(102), EntityId(104)});

// Wrap in new MaskData
auto filtered_masks = std::make_shared<MaskData>();
filtered_masks->setStorage(RaggedStorageVariant<Mask2D>(*view_storage));
dm->setData("selected_masks", filtered_masks, time_key);

// The ViewRaggedStorage IS the lineage - no separate registration needed!
// The _source pointer and _indices define the subset relationship.

// To resolve: just use the view's built-in EntityId lookup
// view_storage->getEntityIdsAtTime(T0) returns EntityIds from source

// BUT: If we want to query via LineageRegistry, register it:
dm->getLineageRegistry()->setLineage("selected_masks",
    Lineage::SubsetLineage{
        .source_key = "masks",
        .included_entities = {EntityId(100), EntityId(102), EntityId(104)}
    });
```

### 5.1 Comprehensive Test Suite

**Unit Tests:**
- LineageRegistry CRUD operations
- Each lineage type resolution
- ViewRaggedStorage ↔ SubsetLineage conversion
- Multi-level lineage chain resolution

**Integration Tests:**
- Full workflow: Load → Transform → Filter → Resolve
- Pipeline with lineage tracking
- UI selection → group creation workflow

**Fuzz Tests:**
- Lineage descriptor serialization (if we add JSON support)
- EntityResolver with malformed lineage chains

### 5.2 Documentation

- Update `docs/developer/data_manager.qmd` with lineage concepts
- Add lineage examples to transform V2 documentation
- API reference for EntityResolver
- Migration guide for existing code

### Deliverables - Phase 5
- [ ] 90%+ test coverage for lineage code
- [ ] Developer documentation
- [ ] Example code snippets
- [ ] Migration notes

---

## File Structure

```
src/DataManager/Lineage/
├── CMakeLists.txt
├── LineageTypes.hpp           # Lineage variant types
├── LineageRegistry.hpp        # Storage for lineage metadata
├── LineageRegistry.cpp
├── EntityResolver.hpp         # Resolution queries
├── EntityResolver.cpp
├── LineageHelpers.hpp         # Utility functions
└── README.md                  # Module documentation

tests/DataManager/Lineage/
├── LineageTypes.test.cpp
├── LineageRegistry.test.cpp
├── EntityResolver.test.cpp
└── Integration.test.cpp
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Performance overhead from lineage lookups | Medium | Medium | Cache resolution results; lazy evaluation |
| Lineage becomes stale when source data changes | Medium | High | Observer pattern to invalidate lineage |
| Complex multi-level lineage chains | Low | Medium | Limit chain depth; provide debugging tools |
| Storage overhead for ExplicitLineage | Low | Low | Use sparingly; prefer implicit types |

---

## Success Criteria

1. **Functional:** Users can trace any derived value back to source EntityIds
2. **Performance:** Resolution adds <1ms overhead for typical queries
3. **Usability:** UI click-to-select works seamlessly with derived data
4. **Maintainability:** Lineage is opt-in; non-lineage code paths unchanged
5. **Testability:** Comprehensive test coverage for all lineage types

---

## Open Questions

1. ~~**Persistence:** Should lineage be saved/loaded with project files?~~
   - **Decision:** No. Lineage is session-scoped only. When data is saved/loaded, lineage
     must be re-established (either automatically by re-running transforms or manually).

2. ~~**Lineage Invalidation:** What happens when source data changes?~~
   - **Decision:** Use Observer pattern. See Section 6 below for implementation details.
   - DataManager establishes observer links when lineage is registered
   - Source changes trigger callbacks to invalidate/update derived lineage

3. **ViewRaggedStorage Ownership:** Who owns the source storage?
   - Currently: `shared_ptr<OwningRaggedStorage const>`
   - Concern: Circular references if view is stored in same container
   - **Recommendation:** Keep as-is; document ownership rules

4. **Integration with EntityGroupManager:** Should groups automatically create SubsetLineage?
   - Pro: Unified model for selections
   - Con: Groups are user-facing; lineage is internal
   - **Recommendation:** Keep separate but provide helpers to convert between them

5. **Dual Identity (Own EntityId + Parent EntityId):** How to handle in UI?
   - When user clicks on a Line derived from Mask, which EntityId to use?
   - Option A: Always use own EntityId (Line), provide "show parent" action
   - Option B: Context-dependent (grouping uses own, provenance uses parent)
   - **Recommendation:** Option B with clear UI affordances

6. **EntityMappedLineage Storage Cost:** For large N:1 aggregations, explicit mapping may be expensive
   - Option A: Always store explicit mapping (simple, but O(n) storage)
   - Option B: Support "computed" mappings (e.g., "all entities in time range X-Y")
   - **Recommendation:** Start with explicit; add computed mappings if profiling shows need

7. **DigitalEventSeries from Ragged Source:** When threshold creates events from ragged data
   - If MaskData has 3 masks at T0 and threshold triggers, which mask caused it?
   - May need per-event tracking of which local_index triggered
   - **Recommendation:** Use ExplicitLineage for complex cases; document limitations

---

## Section 6: Observer-Based Lineage Synchronization

### 6.1 Problem Statement

When source data changes, derived data's lineage may become invalid:

```
MaskData "masks" (source)
    │
    │ ← User adds new mask at T5
    │
    ▼
AnalogTimeSeries "mask_areas" (derived, 1:1 lineage)
    │
    │ ← Now stale! Missing area for new mask at T5
    │
    ▼
DigitalEventSeries "area_peaks" (derived from mask_areas)
    │
    │ ← Also stale! Chain is broken
```

### 6.2 Solution: Observer Callbacks via DataManager

DataManager already has an observer system for data changes. We extend this to manage lineage:

```cpp
class LineageRegistry {
public:
    // ... existing API ...
    
    // NEW: Observer integration
    
    /**
     * @brief Register lineage and set up observer callback
     * 
     * When lineage is registered, DataManager automatically subscribes
     * to changes on the source container(s). When source changes,
     * the callback is invoked to handle invalidation.
     */
    void setLineageWithObserver(
        std::string const& derived_key,
        Lineage::Descriptor lineage,
        DataManager* dm);
    
    /**
     * @brief Callback type for lineage invalidation
     * 
     * @param derived_key The derived container whose lineage is affected
     * @param source_key The source container that changed
     * @param change_type What kind of change occurred
     */
    enum class SourceChangeType {
        DataAdded,      // New elements added to source
        DataRemoved,    // Elements removed from source
        DataModified,   // Existing elements modified
        EntityIdsChanged // EntityIds were reassigned
    };
    
    using InvalidationCallback = std::function<void(
        std::string const& derived_key,
        std::string const& source_key,
        SourceChangeType change_type
    )>;
    
    /**
     * @brief Set custom invalidation handler
     * 
     * Default behavior marks lineage as stale. Custom handlers can:
     * - Re-run the transform to update derived data
     * - Propagate invalidation to downstream containers
     * - Notify UI to refresh
     */
    void setInvalidationHandler(InvalidationCallback handler);

private:
    // Track observer subscription IDs for cleanup
    std::unordered_map<std::string, std::vector<int>> _observer_subscriptions;
    
    InvalidationCallback _invalidation_handler;
};
```

### 6.3 DataManager Integration

```cpp
class DataManager {
    // ... existing ...
    
    // Modified setLineage to auto-subscribe observers
    void registerLineageWithSync(
        std::string const& derived_key,
        Lineage::Descriptor lineage)
    {
        _lineage_registry->setLineage(derived_key, lineage);
        
        // Extract source keys from lineage
        auto source_keys = _lineage_registry->getSourceKeys(derived_key);
        
        for (auto const& source_key : source_keys) {
            // Subscribe to source changes
            int callback_id = addCallbackToData(source_key, [this, derived_key, source_key]() {
                onSourceDataChanged(derived_key, source_key);
            });
            
            // Track subscription for cleanup
            _lineage_observer_ids[derived_key].push_back({source_key, callback_id});
        }
    }
    
private:
    // Map: derived_key → [(source_key, callback_id), ...]
    std::unordered_map<std::string, 
        std::vector<std::pair<std::string, int>>> _lineage_observer_ids;
    
    void onSourceDataChanged(
        std::string const& derived_key,
        std::string const& source_key)
    {
        // Option 1: Mark lineage as stale (lazy invalidation)
        _lineage_registry->markStale(derived_key);
        
        // Option 2: Propagate to downstream (eager invalidation)
        // Find all containers that depend on derived_key and invalidate them too
        
        // Option 3: Call custom handler if registered
        if (_lineage_invalidation_handler) {
            _lineage_invalidation_handler(derived_key, source_key);
        }
        
        // Notify UI observers that derived data may be stale
        _notifyObservers();
    }
    
    // Cleanup when derived data is removed
    void unregisterLineage(std::string const& derived_key) {
        // Unsubscribe from all source observers
        if (auto it = _lineage_observer_ids.find(derived_key); 
            it != _lineage_observer_ids.end()) 
        {
            for (auto const& [source_key, callback_id] : it->second) {
                removeCallbackFromData(source_key, callback_id);
            }
            _lineage_observer_ids.erase(it);
        }
        
        _lineage_registry->removeLineage(derived_key);
    }
};
```

### 6.4 Staleness Tracking

Add a "stale" flag to lineage entries:

```cpp
struct LineageEntry {
    Lineage::Descriptor descriptor;
    bool is_stale = false;
    std::chrono::steady_clock::time_point last_validated;
};

class LineageRegistry {
    std::unordered_map<std::string, LineageEntry> _lineages;
    
public:
    void markStale(std::string const& derived_key) {
        if (auto it = _lineages.find(derived_key); it != _lineages.end()) {
            it->second.is_stale = true;
        }
    }
    
    void markValid(std::string const& derived_key) {
        if (auto it = _lineages.find(derived_key); it != _lineages.end()) {
            it->second.is_stale = false;
            it->second.last_validated = std::chrono::steady_clock::now();
        }
    }
    
    [[nodiscard]] bool isStale(std::string const& derived_key) const {
        if (auto it = _lineages.find(derived_key); it != _lineages.end()) {
            return it->second.is_stale;
        }
        return true; // No lineage = assume stale
    }
};
```

### 6.5 Entity-Bearing Derived Containers: EntityId Sync

For containers like LineData derived from MaskData, we may need to update EntityId mappings:

```cpp
void onSourceDataChanged(
    std::string const& derived_key,
    std::string const& source_key,
    SourceChangeType change_type)
{
    auto lineage = _lineage_registry->getLineage(derived_key);
    if (!lineage) return;
    
    // For ImplicitEntityMapping, the mapping is computed on-demand
    // So we just mark as stale - no explicit update needed
    if (std::holds_alternative<Lineage::ImplicitEntityMapping>(*lineage)) {
        _lineage_registry->markStale(derived_key);
        return;
    }
    
    // For EntityMappedLineage, we may need to update the mapping
    if (auto* mapped = std::get_if<Lineage::EntityMappedLineage>(&*lineage)) {
        if (change_type == SourceChangeType::EntityIdsChanged) {
            // Source EntityIds changed - mapping is now invalid
            // Option A: Clear mapping, require re-registration
            mapped->entity_mapping.clear();
            _lineage_registry->markStale(derived_key);
            
            // Option B: Try to update mapping if we have enough info
            // (This is complex and may not always be possible)
        }
    }
    
    // Propagate to downstream containers
    propagateInvalidation(derived_key);
}

void propagateInvalidation(std::string const& key) {
    // Find all containers that have 'key' as their source
    for (auto const& [derived, entry] : _lineage_registry->getAllLineages()) {
        auto sources = _lineage_registry->getSourceKeys(derived);
        if (std::find(sources.begin(), sources.end(), key) != sources.end()) {
            _lineage_registry->markStale(derived);
            propagateInvalidation(derived); // Recursive
        }
    }
}
```

### 6.6 Sequence Diagram: Source Change Propagation

```
User adds mask    DataManager       LineageRegistry      AnalogTimeSeries
     │                 │                  │                    │
     │ addAtTime()     │                  │                    │
     ├────────────────>│                  │                    │
     │                 │                  │                    │
     │                 │ notifyObservers()│                    │
     │                 ├─────────────────>│                    │
     │                 │                  │                    │
     │                 │  (lineage callback triggered)         │
     │                 │<─────────────────┤                    │
     │                 │                  │                    │
     │                 │ markStale("mask_areas")               │
     │                 ├─────────────────>│                    │
     │                 │                  │                    │
     │                 │ propagateInvalidation()               │
     │                 ├─────────────────>│                    │
     │                 │                  │ markStale("area_peaks")
     │                 │                  ├───────────────────>│
     │                 │                  │                    │
     │                 │ _notifyObservers() (UI refresh)       │
     │                 ├──────────────────────────────────────>│
     │                 │                  │                    │
```

### 6.7 Updated Phase 1 Deliverables

Add to Phase 1:
- [ ] `LineageEntry` struct with staleness tracking
- [ ] Observer subscription management in `LineageRegistry`
- [ ] `DataManager::registerLineageWithSync()` method
- [ ] Invalidation propagation logic
- [ ] Unit tests for observer-based invalidation

---

## Timeline Summary

| Phase | Duration | Focus |
|-------|----------|-------|
| Phase 1 | Week 1 | Core lineage types and registry |
| Phase 2 | Week 2 | EntityResolver implementation |
| Phase 3 | Week 3 | Transform pipeline integration |
| Phase 4 | Week 4 | UI integration |
| Phase 5 | Week 5 | Testing and documentation |

**Total Estimated Time:** 5 weeks

---

## Next Steps

1. Review and approve this implementation plan
2. Create `src/DataManager/Lineage/` directory structure
3. Begin Phase 1 implementation
4. Set up PR review process for lineage code
