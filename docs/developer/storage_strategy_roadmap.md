# Storage Strategy Unification Roadmap

## Overview

This document outlines a plan to unify the storage abstraction patterns across all data types in WhiskerToolbox. The goal is to provide flexible storage backends (in-memory, memory-mapped, lazy transforms, views) while maintaining high performance.

## 📊 Current Progress

**Phase 1: Foundation - ✅ COMPLETED**

- ✅ `RaggedStorageCache<TData>` struct with `is_contiguous` flag
- ✅ `tryGetCacheImpl()` implemented in `OwningRaggedStorage` and `ViewRaggedStorage`
- ✅ `RaggedStorageWrapper<TData>` type-erased wrapper with StorageConcept/StorageModel pattern
- ✅ Mock data types (`SimpleData`, `HeavyData`, `NoCopyData`, `TaggedData`)
- ✅ 18 comprehensive test sections (all passing)
- ✅ Build successful with no errors

**Phase 2: Integration - ✅ COMPLETED**

- ✅ Replaced `OwningRaggedStorage<TData>` with `RaggedStorageWrapper<TData>` in `RaggedTimeSeries`
- ✅ Added mutation methods to `RaggedStorageWrapper` (append, clear, removeByEntityId)
- ✅ Added `RaggedStorageCache<TData>` member with `_cacheOptimizationPointers()` update logic
- ✅ Updated `RaggedIterator` to use fast-path when cached pointers are valid
- ✅ Implemented `createView()` static factory method for view-based instances
- ✅ All existing tests pass - LineData, MaskData, PointData unchanged
- ✅ Cache optimization integrated throughout all mutation methods
- ✅ Build successful with zero errors/warnings

**Next Phase: Phase 3 Lazy Transforms - 🔄 IN QUEUE**
Enable lazy transforms for `RaggedTimeSeries` with `LazyRaggedStorage<TData, ViewType>`.

## Current State Analysis

### Storage Abstraction by Data Type

| Data Type | Storage Abstraction | Memory-Mapped | View/Subset | Lazy Transform |
|-----------|---------------------|---------------|-------------|----------------|
| `AnalogTimeSeries` | ✅ Full (CRTP + Type-erasure) | ✅ | ✅ LazyViewStorage | ✅ |
| `RaggedTimeSeries<T>` | ✅ Full (CRTP + Type-erasure wrapper) | ❌ | ✅ ViewRaggedStorage factory | 🔄 Ready (Phase 3) |
| `RaggedAnalogTimeSeries` | ❌ Raw `std::map` | ❌ | ❌ | ❌ |
| `DigitalEventSeries` | ❌ Raw `std::vector` | ❌ | ❌ | ❌ |
| `DigitalIntervalSeries` | ❌ Raw `std::vector` | ❌ | ❌ | ❌ |

### What's Working Well

1. **Strong Time Type System** - All types use `TimeFrameIndex` and support `TimeFrame` conversion
2. **Observer Pattern** - Consistent notification via `ObserverData` base class
3. **CRTP Base Classes** - Zero-overhead polymorphism when type is known at compile time

---

## Two Polymorphism Patterns

### Pattern 1: Type Erasure (Virtual Dispatch)

**Used in:** `AnalogTimeSeries::DataStorageWrapper`

```cpp
class DataStorageWrapper {
    struct StorageConcept {  // Abstract interface
        virtual float getValueAt(size_t) const = 0;
        virtual std::span<float const> getSpan() const = 0;
        // ...
    };
    
    template<typename Impl>
    struct StorageModel : StorageConcept {  // Concrete wrapper
        Impl _storage;
        float getValueAt(size_t i) const override { 
            return _storage.getValueAt(i); 
        }
    };
    
    std::unique_ptr<StorageConcept> _impl;
};
```

**Pros:**
- Open extension - add new storage types without changing wrapper
- Template parameter hiding - `LazyViewStorage<ViewType>` can be stored without exposing `ViewType`
- Familiar OOP pattern

**Cons:**
- Virtual call overhead per access
- Heap allocation for storage instance

### Pattern 2: `std::variant` (Compile-time Dispatch)

**Used in:** `RaggedStorageVariant` (currently unused)

```cpp
template<typename TData>
class RaggedStorageVariant {
    std::variant<OwningRaggedStorage<TData>, ViewRaggedStorage<TData>> _storage;
    
    TData const& getData(size_t idx) const {
        return std::visit([idx](auto const& s) -> TData const& { 
            return s.getData(idx); 
        }, _storage);
    }
};
```

**Pros:**
- No heap allocation (inline storage)
- Compiler can optimize `std::visit` aggressively
- Type-safe access via `std::get_if<>`

**Cons:**
- Closed type set - adding types requires modifying variant
- Cannot hide template parameters (no `LazyViewStorage<ViewType>` support)

### Recommendation: Use Type Erasure

**Rationale:** Lazy transforms require template parameter hiding. The pattern `LazyViewStorage<ViewType>` cannot be stored in a `std::variant` because `ViewType` is unbounded.

Since lazy transforms are a priority for the future, **type erasure is the correct choice** for all data types.

---

## Key Optimization: Contiguous Pointer Caching

The main performance concern with type erasure is virtual dispatch overhead during iteration. `AnalogTimeSeries` solves this with a **cached pointer optimization**:

### How It Works

1. **At construction time**, attempt to extract raw pointers to contiguous data:
   ```cpp
   void _cacheOptimizationPointers() {
       _contiguous_data_ptr = _data_storage.tryGetContiguousPointer();
   }
   ```

2. **In iterators**, check the cached pointer before virtual dispatch:
   ```cpp
   float val = (_contiguous_data_ptr)
       ? _contiguous_data_ptr[_current_index.getValue()]  // Fast path: direct access
       : _series->_data_storage.getValueAt(_current_index.getValue());  // Slow path: virtual
   ```

3. **Bulk access** via `getSpan()` returns a `std::span` with one virtual call, then iterate with zero overhead.

### Performance Characteristics

| Access Pattern | Virtual Calls | Notes |
|----------------|---------------|-------|
| Single value via `getValueAt()` | 1 per access | Unavoidable for type-erased interface |
| Iteration via cached pointer | 0 per element | Fast path when storage is contiguous |
| Bulk access via `getSpan()` | 1 total | Returns span, then iterate natively |
| Non-contiguous storage | 1 per access | Falls back to virtual dispatch |

---

## Applying to RaggedTimeSeries

For `RaggedTimeSeries<TData>`, the data is stored in Structure-of-Arrays (SoA) layout with three parallel vectors:
- `TimeFrameIndex times[]`
- `TData data[]`  
- `EntityId entity_ids[]`

### Proposed Cache Structure

```cpp
template<typename TData>
struct RaggedStorageCache {
    TimeFrameIndex const* times_ptr = nullptr;
    TData const* data_ptr = nullptr;
    EntityId const* entity_ids_ptr = nullptr;
    size_t size = 0;
    
    [[nodiscard]] bool isValid() const noexcept {
        return times_ptr != nullptr;
    }
};
```

### Type-Erased Wrapper

```cpp
template<typename TData>
class RaggedStorageWrapper {
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        virtual size_t size() const = 0;
        virtual TimeFrameIndex getTime(size_t idx) const = 0;
        virtual TData const& getData(size_t idx) const = 0;
        virtual EntityId getEntityId(size_t idx) const = 0;
        virtual std::optional<size_t> findByEntityId(EntityId id) const = 0;
        virtual std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const = 0;
        virtual RaggedStorageCache<TData> tryGetCache() const = 0;
        virtual RaggedStorageType getStorageType() const = 0;
    };
    
    template<typename StorageImpl>
    struct StorageModel : StorageConcept {
        StorageImpl _storage;
        // ... forward all methods to _storage
        
        RaggedStorageCache<TData> tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCacheImpl(); }) {
                return _storage.tryGetCacheImpl();
            }
            return {};  // Invalid cache for non-contiguous storage
        }
    };
    
    std::unique_ptr<StorageConcept> _impl;
};
```

---

## Implementation Roadmap

### Phase 1: Foundation (Estimated: 4-6 hours) ✅ **COMPLETED**

**Goal:** Create the type-erased wrapper for `RaggedTimeSeries` without breaking existing code.

✅ **1. Added `RaggedStorageCache<TData>`** to `RaggedStorage.hpp`
   - Struct with pointers to times, data, entity_ids
   - `is_contiguous` flag for validity checking
   - Convenience accessors for cached data

✅ **2. Added `tryGetCacheImpl()` to existing storage classes**
   - `OwningRaggedStorage`: Returns valid cache with pointers to internal vectors
   - `ViewRaggedStorage`: Returns invalid cache (indices are non-contiguous)
   - `tryGetCache()` method in CRTP base class dispatches to implementations

✅ **3. Created `RaggedStorageWrapper<TData>`** using type erasure pattern
   - Same structure as `AnalogTimeSeries::DataStorageWrapper`
   - `StorageConcept` abstract interface with virtual dispatch
   - `StorageModel<StorageImpl>` concrete wrapper template
   - Includes `tryGetCache()` for fast-path optimization
   - Move-only semantics via `unique_ptr`
   - Type access via `tryGet<StorageType>()`

✅ **4. Created test fixtures and comprehensive tests**
   - `MockDataTypes.hpp`: SimpleData, HeavyData, NoCopyData, TaggedData
   - Added 18 test sections covering cache and wrapper functionality
   - Tests for empty/populated storage, move semantics, cache validity
   - All tests passing ✅

### Phase 2: Integration (Estimated: 4-6 hours) ✅ **COMPLETED**

**Goal:** Switch `RaggedTimeSeries` to use the type-erased wrapper.

✅ **1. Replaced storage member** in `RaggedTimeSeries`:
   ```cpp
   // Before:
   OwningRaggedStorage<TData> _storage;
   
   // After:
   RaggedStorageWrapper<TData> _storage;
   ```
   - All construction paths updated to create wrapper with `OwningRaggedStorage` backend
   - Wrapper constructor automatically wraps the `OwningRaggedStorage` instance

✅ **2. Added cached pointers** to `RaggedTimeSeries`:
   ```cpp
   RaggedStorageCache<TData> _cached_storage;
   
   void _cacheOptimizationPointers() {
       _cached_storage = _storage.tryGetCache();
   }
   ```
   - Cache updated after every mutation (append, clear, remove, etc.)
   - Invalidates automatically when needed
   - Zero overhead when storage is contiguous

✅ **3. Updated RaggedIterator** to use fast path when available:
   ```cpp
   // Element access uses cached pointers when valid, falls back to virtual dispatch
   TimeFrameIndex time = _cached_storage.isValid() 
       ? _cached_storage.getTime(_index)
       : _series->_storage.getTime(_index);
   ```
   - 0 virtual calls per element when storage is contiguous (owning storage or contiguous views)
   - 1 virtual call per element when storage is non-contiguous (filtered/sparse views)
   
   ✅ **Optimization Implemented:** `ViewRaggedStorage::tryGetCacheImpl()` now detects when indices form a contiguous range `[k, k+1, ..., m]` and returns a valid cache pointing to the underlying storage with appropriate offset. This includes all elements (`setAllIndices()`) and consecutive ranges.

✅ **4. Added factory methods** for creating view-based instances:
   ```cpp
   static std::shared_ptr<RaggedTimeSeries<TData>> createView(
       std::shared_ptr<RaggedTimeSeries<TData> const> source,
       std::unordered_set<EntityId> const& entity_ids);
   ```
   - Creates new series with `ViewRaggedStorage` backend
   - Filters by entity IDs efficiently
   - No data materialization needed

✅ **5. Verified all tests pass** - `LineData`, `MaskData`, `PointData` work unchanged
   - All existing tests pass without modification
   - Cache optimization transparent to users
   - Performance characteristics identical to Phase 1

### Phase 3: Lazy Transform Support (Estimated: 6-8 hours) 🔄 **IN QUEUE**

**Goal:** Enable lazy transforms for `RaggedTimeSeries`.

1. **Create `LazyRaggedStorage<TData, ViewType>`**:
   ```cpp
   template<typename TData, typename ViewType>
   class LazyRaggedStorage {
       ViewType _view;
       std::vector<EntityId> _entity_ids;  // Strategy depends on transform type
       // See "Design Decisions" section below for EntityId handling approach
   };
   ```

2. **Implement EntityId handling based on transform type** (see § Design Decisions)
   - Use lineage system (`LineageTypes.hpp`) to determine appropriate strategy
   - 1:1 transforms: Pass through source EntityIds
   - N:1 transforms: Store computed EntityId mapping
   - New entity transforms: Generate new EntityIds with lineage tracking
   - Implementation will follow existing patterns from `AnalogTimeSeries` lazy storage

3. **Add `createFromView()` factory method** to `RaggedTimeSeries`

4. **Add `materialize()` method** to convert lazy storage to owning

### Phase 4: Other Data Types (Estimated: 8-12 hours) ⏳ **PLANNED**

**Goal:** Bring storage abstraction to remaining types.

1. **`RaggedAnalogTimeSeries`**
   - Similar pattern to `RaggedTimeSeries` but simpler (no EntityId)
   - `std::map<TimeFrameIndex, std::vector<float>>` → wrapper

2. **`DigitalEventSeries`**
   - Simple case: just `std::vector<TimeFrameIndex>`
   - Storage wrapper for vector vs memory-mapped vs view

3. **`DigitalIntervalSeries`**
   - Similar to events but stores `Interval` instead of `TimeFrameIndex`

### Phase 5: Testing & Documentation (Estimated: 4-6 hours)

1. **Unit tests for each storage type**
   - Owning storage basic operations
   - View storage filtering
   - Lazy storage computation
   - Cache optimization correctness

2. **Performance benchmarks**
   - Compare iteration speed: cached vs uncached
   - Memory usage: owning vs view

3. **Documentation**
   - Usage examples for each factory method
   - Performance guidance (when to use views vs copies)

---

## Design Decisions to Make

### 1. EntityId Handling in Lazy Transforms ✅ RESOLVED

**Problem:** A lazy transform like `masks | transform(skeletonize)` produces new data. Should the output have:
- Same EntityIds as input? (identity preservation)
- New EntityIds? (new entities)
- No EntityIds? (computation only)

**Resolution:** The Entity library's lineage system provides a comprehensive answer via `LineageTypes.hpp`:

| Transform Type | EntityId Strategy | Lineage Type |
|---------------|-------------------|--------------|
| 1:1 same entity (e.g., MaskArea) | **Preserve** input EntityId | `OneToOneByTime` |
| 1:1 new entity (e.g., Skeletonize) | **New** EntityId, track parent | `EntityMappedLineage` |
| N:1 aggregation (e.g., SumAreas) | **New** EntityId, track all parents | `AllToOneByTime` |
| Subset/filter | **Preserve** input EntityId | `SubsetLineage` |
| Lazy view (no transform) | **Same** as source | `ImplicitEntityMapping` |

**Implementation guidance for storage:**

```cpp
// Storage layer: Just store EntityIds, don't generate
template<typename TData>
class LazyRaggedStorage {
    // Option A: EntityId passthrough (1:1 transforms)
    EntityId getEntityId(size_t idx) const {
        return _source->getEntityId(_transform.sourceIndexFor(idx));
    }
    
    // Option B: New EntityIds (N:1 or new-entity transforms)
    // Requires materialization or pre-computed mapping
    std::vector<EntityId> _output_entity_ids;  // Filled during construction
};
```

**Key insight from LineageResolver:** The `IEntityDataSource` interface allows lineage resolution to work regardless of storage type. The storage just needs to provide `getEntityId()` and `getAllEntityIdsAtTime()` - the lineage system handles provenance tracking.

### 2. View Invalidation

**Problem:** `ViewRaggedStorage` holds a `shared_ptr` to source. If source is modified, view becomes invalid.

**Options:**
- A) Document as undefined behavior (current approach)
- B) Add version counter, check on access
- C) Use weak_ptr, throw if source is gone

**Recommendation:** Option A for now, consider B if bugs arise.

### 3. Memory-Mapped Support for Ragged Data

**Problem:** Memory-mapped files work well for fixed-stride data. Ragged data has variable-length records.

**Options:**
- A) Don't support (materialize on load)
- B) Use index file + data file format
- C) Support only for known formats (e.g., HDF5 via external library)

**Recommendation:** Option A for initial implementation. Ragged data is typically smaller than analog data, so materialization is acceptable.

---

## Integration with Entity/Lineage System

The storage refactoring must integrate cleanly with the existing Entity library. This section documents the interface boundaries.

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Entity Library (Independent)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  EntityRegistry          LineageRegistry         LineageResolver             │
│  - ensureId()            - setLineage()          - resolveToSource()         │
│  - get()                 - getLineage()          - resolveToRoot()           │
│                          - isStale()             - uses IEntityDataSource    │
└─────────────────────────────────────────────────────────────────────────────┘
                                    ▲
                                    │ implements IEntityDataSource
                                    │
┌─────────────────────────────────────────────────────────────────────────────┐
│                     DataManager Library (Depends on Entity)                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  DataManagerEntityDataSource     RaggedTimeSeries<T>     AnalogTimeSeries    │
│  - getEntityIds()                - RaggedStorageWrapper  - DataStorageWrapper│
│  - getAllEntityIdsAtTime()       - cached pointers       - cached pointers   │
│  - dispatches by DM_DataType                                                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Storage Responsibilities (What storage MUST provide)

The `IEntityDataSource` interface requires these capabilities from data containers:

```cpp
// From Entity/Lineage/LineageResolver.hpp
class IEntityDataSource {
    virtual std::vector<EntityId> getEntityIds(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index) const = 0;
        
    virtual std::vector<EntityId> getAllEntityIdsAtTime(
        std::string const& data_key,
        TimeFrameIndex time) const = 0;
        
    virtual std::unordered_set<EntityId> getAllEntityIds(
        std::string const& data_key) const = 0;
        
    virtual std::size_t getElementCount(
        std::string const& data_key,
        TimeFrameIndex time) const = 0;
};
```

**Implication for storage:** Each storage backend must support:

| Capability | Storage Method | Notes |
|------------|----------------|-------|
| Single EntityId access | `getEntityId(idx)` | O(1) for owning, O(1) for view |
| All EntityIds at time | `getEntityIdsInTimeRange(start, end)` | Uses time index map |
| All EntityIds | `getAllEntityIds()` | Return full set |
| Element count at time | `getTimeRange(time)` → `end - start` | Already implemented |
| EntityId → index lookup | `findByEntityId(id)` | Hash map lookup |

### EntityId Generation vs Storage

**Important distinction:**

| Concern | Responsibility | Location |
|---------|---------------|----------|
| **Generate** EntityIds | Container class (e.g., `LineData`) | Via `EntityRegistry::ensureId()` |
| **Store** EntityIds | Storage backend | In `entity_ids[]` vector |
| **Track lineage** | `LineageRegistry` + `LineageRecorder` | Entity library + DataManager |
| **Resolve lineage** | `LineageResolver` | Via `IEntityDataSource` interface |

### Integration Points for Storage Backends

Each storage type needs to support the lineage system:

#### OwningRaggedStorage

```cpp
template<typename TData>
class OwningRaggedStorage {
    std::vector<EntityId> _entity_ids;
    
    // Required for IEntityDataSource
    EntityId getEntityId(size_t idx) const { return _entity_ids[idx]; }
    std::optional<size_t> findByEntityId(EntityId id) const;
    
    // Required for cache optimization
    RaggedStorageCache<TData> tryGetCacheImpl() const {
        return {_times.data(), _data.data(), _entity_ids.data(), _times.size()};
    }
};
```

#### ViewRaggedStorage

```cpp
template<typename TData>
class ViewRaggedStorage {
    std::shared_ptr<OwningRaggedStorage<TData> const> _source;
    std::vector<size_t> _indices;  // Indices into source
    
    // Delegate to source with index mapping
    EntityId getEntityId(size_t idx) const {
        return _source->getEntityId(_indices[idx]);
    }
    
    // Detects contiguous index ranges and returns valid cache when possible
    RaggedStorageCache<TData> tryGetCacheImpl() const {
        // Check if indices form a contiguous range [start, start+1, ..., start+n-1]
        if (_indices.empty()) return {};
        size_t const start_idx = _indices[0];
        bool is_contiguous = true;
        for (size_t i = 1; i < _indices.size(); ++i) {
            if (_indices[i] != start_idx + i) {
                is_contiguous = false;
                break;
            }
        }
        if (is_contiguous) {
            auto src_times = _source->getTimes();
            auto src_data = _source->getData();
            auto src_entity_ids = _source->getEntityIds();
            return RaggedStorageCache<TData>{
                src_times.data() + start_idx,
                src_data.data() + start_idx,
                src_entity_ids.data() + start_idx,
                _indices.size(), true
            };
        }
        return {};  // Non-contiguous: invalid cache
    }
};
```

#### LazyRaggedStorage (Future)

```cpp
template<typename TData, typename ViewType>
class LazyRaggedStorage {
    ViewType _view;
    // EntityId handling depends on transform type:
    
    // Option A: 1:1 transform - delegate to source
    // Option B: N:1 transform - store computed EntityIds
    // Option C: New entity transform - generate new EntityIds
    
    std::variant<
        std::monostate,                    // Delegate to source
        std::vector<EntityId>,             // Stored mapping
        std::function<EntityId(size_t)>    // Computed on demand
    > _entity_id_strategy;
};
```

### Lineage Types and Storage Implications

| Lineage Type | Storage Requirement | Example |
|--------------|---------------------|---------|
| `Source` | Full EntityId storage | Original MaskData |
| `OneToOneByTime` | Can delegate to source | MaskArea transform |
| `AllToOneByTime` | New EntityIds per time | Sum of areas |
| `SubsetLineage` | Subset of source EntityIds | Filtered masks |
| `EntityMappedLineage` | New EntityIds with explicit mapping | Skeletonization |
| `ImplicitEntityMapping` | Delegate to source (same time structure) | Lazy views |

### Testing Integration with Entity System

When testing storage backends, verify Entity system integration:

```cpp
TEST_CASE("Storage integrates with EntityRegistry", "[storage][entity]") {
    EntityRegistry registry;
    OwningRaggedStorage<SimpleData> storage;
    
    // Container generates EntityId via registry
    EntityId id = registry.ensureId("test_data", EntityKind::LineEntity, 
                                     TimeFrameIndex(10), 0);
    
    // Storage stores and retrieves it
    storage.append(TimeFrameIndex(10), SimpleData{1, 1.0f}, id);
    
    REQUIRE(storage.getEntityId(0) == id);
    REQUIRE(storage.findByEntityId(id) == 0);
}

TEST_CASE("Storage supports IEntityDataSource queries", "[storage][lineage]") {
    // Test that storage can answer all queries needed by DataManagerEntityDataSource
    OwningRaggedStorage<SimpleData> storage;
    
    // Add data at multiple times
    storage.append(TimeFrameIndex(10), SimpleData{1, 1.0f}, EntityId(100));
    storage.append(TimeFrameIndex(10), SimpleData{2, 2.0f}, EntityId(101));
    storage.append(TimeFrameIndex(20), SimpleData{3, 3.0f}, EntityId(102));
    
    // getEntityIds(time, local_index) - single element
    REQUIRE(storage.getEntityId(0) == EntityId(100));
    
    // getAllEntityIdsAtTime(time) - all at time
    auto [start, end] = storage.getTimeRange(TimeFrameIndex(10));
    REQUIRE(end - start == 2);
    
    // getElementCount(time)
    REQUIRE(storage.getTimeRange(TimeFrameIndex(10)).second - 
            storage.getTimeRange(TimeFrameIndex(10)).first == 2);
}
```

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/DataManager/utils/RaggedStorage.hpp` | Add cache struct, add `tryGetCacheImpl()`, create wrapper |
| `src/DataManager/utils/RaggedTimeSeries.hpp` | Switch to wrapper, add factory methods, update iterators |
| `src/DataManager/Lines/LineData.hpp` | Verify compatibility (may need no changes) |
| `src/DataManager/Masks/MaskData.hpp` | Verify compatibility |
| `src/DataManager/Points/PointData.hpp` | Verify compatibility |
| `tests/DataManager/utils/RaggedStorage.test.cpp` | New test file for storage types |

---

## Success Criteria

1. **All existing tests pass** - No regression in `LineData`, `MaskData`, `PointData` functionality
2. **New tests for storage types** - Owning, View, and Lazy storage all tested
3. **Performance parity** - Iteration over contiguous data is as fast as before (within 10%)
4. **Factory methods work** - Can create views and lazy transforms via documented API
5. **Documentation complete** - Usage examples in code and developer docs

---

## Testing Strategy

### The Challenge

Storage backends are template classes that require a concrete `TData` type to instantiate. This creates several testing challenges:

1. **Can't compile templates in isolation** - `OwningRaggedStorage<TData>` needs a `TData`
2. **Real types are complex** - `Mask2D` has move semantics, large data, etc.
3. **Failure attribution** - If a test fails, is it the storage or the data type?
4. **Test organization** - Where do storage tests live if they depend on types?

### Solution: Layered Testing with Mock Types

We use a **three-layer testing approach**:

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 3: System Tests                                      │
│  - Full pipelines with real data                            │
│  - Test transform chains, serialization, visualization      │
│  - Location: tests/WhiskerToolbox/ or integration/          │
└─────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────────────────────────────────────┐
│  Layer 2: Integration Tests                                 │
│  - Storage + Real Types (Mask2D, Line2D, Point2D<float>)    │
│  - Verify real types work correctly with storage            │
│  - Location: tests/DataManager/{Lines,Masks,Points}/        │
└─────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────────────────────────────────────┐
│  Layer 1: Unit Tests (with Mock Types)                      │
│  - Pure storage logic in isolation                          │
│  - Simple mock types defined in test fixtures               │
│  - Fast compilation, clear failure attribution              │
│  - Location: tests/DataManager/utils/                       │
└─────────────────────────────────────────────────────────────┘
```

### Layer 1: Unit Tests with Mock Types

Create a test fixtures header with minimal types designed for testing:

**File: `tests/mocks/MockDataTypes.hpp`**

```cpp
#ifndef MOCK_DATA_TYPES_HPP
#define MOCK_DATA_TYPES_HPP

#include <string>
#include <vector>

namespace TestFixtures {

/**
 * @brief Minimal data type for testing storage operations
 * 
 * Deliberately simple to isolate storage logic from data type complexity.
 * Tracks copies/moves for verifying efficiency.
 */
struct SimpleData {
    int id = 0;
    float value = 0.0f;
    
    SimpleData() = default;
    SimpleData(int i, float v) : id(i), value(v) {}
    
    bool operator==(SimpleData const&) const = default;
};

/**
 * @brief Data type with expensive copy to test move semantics
 */
struct HeavyData {
    std::vector<float> buffer;
    
    // Track operations for testing
    inline static int copy_count = 0;
    inline static int move_count = 0;
    static void resetCounters() { copy_count = 0; move_count = 0; }
    
    HeavyData() = default;
    explicit HeavyData(size_t size) : buffer(size, 1.0f) {}
    
    HeavyData(HeavyData const& other) : buffer(other.buffer) { ++copy_count; }
    HeavyData(HeavyData&& other) noexcept : buffer(std::move(other.buffer)) { ++move_count; }
    
    HeavyData& operator=(HeavyData const& other) { buffer = other.buffer; ++copy_count; return *this; }
    HeavyData& operator=(HeavyData&& other) noexcept { buffer = std::move(other.buffer); ++move_count; return *this; }
    
    bool operator==(HeavyData const&) const = default;
};

/**
 * @brief Data type that throws on copy (to verify no-copy paths)
 */
struct NoCopyData {
    int value = 0;
    
    NoCopyData() = default;
    explicit NoCopyData(int v) : value(v) {}
    
    NoCopyData(NoCopyData const&) { throw std::runtime_error("Unexpected copy!"); }
    NoCopyData(NoCopyData&&) noexcept = default;
    NoCopyData& operator=(NoCopyData const&) { throw std::runtime_error("Unexpected copy!"); }
    NoCopyData& operator=(NoCopyData&&) noexcept = default;
};

} // namespace TestFixtures

#endif // MOCK_DATA_TYPES_HPP
```

**File: `tests/DataManager/utils/RaggedStorage.test.cpp`**

```cpp
#include "utils/RaggedStorage.hpp"
#include "mocks/MockDataTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

using namespace TestFixtures;

// =============================================================================
// Test with multiple types using TEMPLATE_TEST_CASE
// =============================================================================

TEMPLATE_TEST_CASE("OwningRaggedStorage - Basic Operations", 
                   "[storage][owning][unit]",
                   SimpleData, HeavyData) {
    
    OwningRaggedStorage<TestType> storage;
    
    SECTION("Empty storage") {
        REQUIRE(storage.size() == 0);
        REQUIRE(storage.empty());
        REQUIRE(storage.getTimeCount() == 0);
    }
    
    SECTION("Append and retrieve") {
        storage.append(TimeFrameIndex(10), TestType{}, EntityId(1));
        storage.append(TimeFrameIndex(10), TestType{}, EntityId(2));
        storage.append(TimeFrameIndex(20), TestType{}, EntityId(3));
        
        REQUIRE(storage.size() == 3);
        REQUIRE(storage.getTimeCount() == 2);
        
        auto [start, end] = storage.getTimeRange(TimeFrameIndex(10));
        REQUIRE(end - start == 2);
    }
    
    SECTION("EntityId lookup") {
        storage.append(TimeFrameIndex(10), TestType{}, EntityId(42));
        
        auto idx = storage.findByEntityId(EntityId(42));
        REQUIRE(idx.has_value());
        REQUIRE(*idx == 0);
        
        auto missing = storage.findByEntityId(EntityId(999));
        REQUIRE_FALSE(missing.has_value());
    }
}

// =============================================================================
// Test move semantics with HeavyData
// =============================================================================

TEST_CASE("OwningRaggedStorage - Move Semantics", "[storage][owning][unit]") {
    HeavyData::resetCounters();
    
    OwningRaggedStorage<HeavyData> storage;
    
    SECTION("Move append avoids copy") {
        HeavyData data(1000);  // 1000 floats
        storage.append(TimeFrameIndex(0), std::move(data), EntityId(1));
        
        REQUIRE(HeavyData::move_count == 1);
        REQUIRE(HeavyData::copy_count == 0);
    }
    
    SECTION("Copy append when needed") {
        HeavyData data(1000);
        storage.append(TimeFrameIndex(0), data, EntityId(1));  // lvalue
        
        REQUIRE(HeavyData::copy_count == 1);
    }
}

// =============================================================================
// Test cache optimization
// =============================================================================

TEST_CASE("OwningRaggedStorage - Cache Optimization", "[storage][owning][cache][unit]") {
    OwningRaggedStorage<SimpleData> storage;
    
    // Add some data
    for (int i = 0; i < 100; ++i) {
        storage.append(TimeFrameIndex(i / 10), SimpleData{i, static_cast<float>(i)}, EntityId(i));
    }
    
    SECTION("Cache is valid for owning storage") {
        auto cache = storage.tryGetCache();
        REQUIRE(cache.isValid());
        REQUIRE(cache.size == 100);
    }
    
    SECTION("Cache provides direct access") {
        auto cache = storage.tryGetCache();
        
        // Verify cache matches storage
        for (size_t i = 0; i < cache.size; ++i) {
            REQUIRE(cache.getTime(i) == storage.getTime(i));
            REQUIRE(cache.getData(i) == storage.getData(i));
            REQUIRE(cache.getEntityId(i) == storage.getEntityId(i));
        }
    }
}

// =============================================================================
// Test ViewRaggedStorage
// =============================================================================

TEST_CASE("ViewRaggedStorage - Filtering", "[storage][view][unit]") {
    auto source = std::make_shared<OwningRaggedStorage<SimpleData>>();
    
    // Populate source
    source->append(TimeFrameIndex(10), SimpleData{1, 1.0f}, EntityId(1));
    source->append(TimeFrameIndex(10), SimpleData{2, 2.0f}, EntityId(2));
    source->append(TimeFrameIndex(20), SimpleData{3, 3.0f}, EntityId(3));
    source->append(TimeFrameIndex(30), SimpleData{4, 4.0f}, EntityId(4));
    
    ViewRaggedStorage<SimpleData> view(source);
    
    SECTION("Filter by EntityId set") {
        std::unordered_set<EntityId> ids{EntityId(1), EntityId(3)};
        view.filterByEntityIds(ids);
        
        REQUIRE(view.size() == 2);
        REQUIRE(view.getData(0).id == 1);
        REQUIRE(view.getData(1).id == 3);
    }
    
    SECTION("Filter by time range") {
        view.filterByTimeRange(TimeFrameIndex(10), TimeFrameIndex(20));
        
        REQUIRE(view.size() == 3);  // Two at t=10, one at t=20
    }
    
    SECTION("View cache is invalid (non-contiguous)") {
        view.setAllIndices();
        auto cache = view.tryGetCache();
        REQUIRE_FALSE(cache.isValid());
    }
}
```

### Layer 2: Integration Tests with Real Types

These tests verify that real data types work correctly with storage. They live alongside the data type tests:

**File: `tests/DataManager/Lines/LineData.test.cpp`** (add to existing)

```cpp
TEST_CASE("LineData - Storage Backend Integration", "[linedata][storage][integration]") {
    
    SECTION("Storage cache is valid") {
        LineData lines;
        lines.addAtTime(TimeFrameIndex(0), Line2D{{0,0}, {1,1}}, NotifyObservers::No);
        lines.addAtTime(TimeFrameIndex(0), Line2D{{2,2}, {3,3}}, NotifyObservers::No);
        
        // Access internal storage and verify cache works
        // (May need friend access or public method for testing)
        auto cache = lines.getStorageCache();  // If exposed
        REQUIRE(cache.isValid());
    }
    
    SECTION("Large dataset performance") {
        LineData lines;
        
        // Add 10,000 lines
        for (int t = 0; t < 1000; ++t) {
            for (int i = 0; i < 10; ++i) {
                lines.addAtTime(TimeFrameIndex(t), 
                               Line2D{{0,0}, {static_cast<float>(i), static_cast<float>(i)}},
                               NotifyObservers::No);
            }
        }
        
        // Iteration should be fast (cache-optimized)
        size_t count = 0;
        for (auto [time, entry] : lines.elements()) {
            count++;
        }
        REQUIRE(count == 10000);
    }
}
```

### Layer 3: Property-Based Testing

Use Catch2's generators for property-based tests that verify invariants:

```cpp
TEST_CASE("Storage Invariants", "[storage][property][unit]") {
    
    SECTION("Size equals sum of time range sizes") {
        OwningRaggedStorage<SimpleData> storage;
        
        // Add random data
        for (int i = 0; i < 50; ++i) {
            TimeFrameIndex time(i % 10);  // 10 distinct times
            storage.append(time, SimpleData{i, 0.0f}, EntityId(i));
        }
        
        // Verify invariant
        size_t sum = 0;
        for (auto const& [time, range] : storage.timeRanges()) {
            sum += range.second - range.first;
        }
        REQUIRE(sum == storage.size());
    }
    
    SECTION("EntityId lookup is consistent") {
        OwningRaggedStorage<SimpleData> storage;
        
        std::vector<EntityId> ids;
        for (int i = 0; i < 100; ++i) {
            EntityId id(i + 1000);
            ids.push_back(id);
            storage.append(TimeFrameIndex(i), SimpleData{i, 0.0f}, id);
        }
        
        // Every inserted EntityId should be findable
        for (EntityId id : ids) {
            auto idx = storage.findByEntityId(id);
            REQUIRE(idx.has_value());
            REQUIRE(storage.getEntityId(*idx) == id);
        }
    }
}
```

### Test File Organization

```
tests/
├── mocks/
│   └── MockDataTypes.hpp           # Test fixture types (SimpleData, HeavyData)
│
├── Entity/
│   └── Lineage/
│       ├── LineageRegistry.test.cpp    # Existing: lineage metadata
│       └── LineageResolver.test.cpp    # Existing: resolution with mocks
│
├── DataManager/
│   ├── utils/
│   │   ├── RaggedStorage.test.cpp      # Layer 1: Unit tests with mocks
│   │   └── StorageEntityIntegration.test.cpp  # Storage + EntityRegistry
│   │
│   ├── Lineage/
│   │   ├── DataManagerEntityDataSource.test.cpp  # Existing: type dispatch
│   │   └── EntityResolver.test.cpp               # Existing: full resolution
│   │
│   ├── Lines/
│   │   └── LineData.test.cpp       # Layer 2: Integration (includes storage)
│   │
│   ├── Masks/
│   │   └── MaskData.test.cpp       # Layer 2: Integration
│   │
│   └── Points/
│       └── PointData.test.cpp      # Layer 2: Integration
│
└── integration/                     # Layer 3: System tests
    ├── TransformPipeline.test.cpp      # Transform chains with lineage
    └── test_transform_lineage_integration.test.cpp  # Existing: 700+ lines
```

### Test Tagging Strategy

Use Catch2 tags to enable selective test execution:

| Tag | Meaning | When to Run |
|-----|---------|-------------|
| `[unit]` | Isolated unit test | Always (CI, local) |
| `[integration]` | Tests multiple components | CI, before merge |
| `[storage]` | Storage-related test | When modifying storage |
| `[owning]` | OwningRaggedStorage tests | Specific debugging |
| `[view]` | ViewRaggedStorage tests | Specific debugging |
| `[cache]` | Cache optimization tests | Performance work |
| `[property]` | Property-based invariant tests | CI, before merge |
| `[entity]` | EntityRegistry integration | When modifying entity handling |
| `[lineage]` | Lineage system integration | When modifying lineage support |

**Example commands:**
```bash
# Run only storage unit tests
ctest --preset linux-clang-release -R "RaggedStorage" --output-on-failure

# Run all unit tests (fast)
ctest --preset linux-clang-release -L "unit" --output-on-failure

# Run storage tests across all layers
ctest --preset linux-clang-release -L "storage" --output-on-failure

# Run all entity-related tests (storage + lineage)
ctest --preset linux-clang-release -L "entity|lineage" --output-on-failure
```

### Concepts for Compile-Time Interface Checking

Use C++20 concepts to verify storage implementations satisfy the required interface:

**File: `src/DataManager/utils/StorageConcepts.hpp`**

```cpp
#ifndef STORAGE_CONCEPTS_HPP
#define STORAGE_CONCEPTS_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include <concepts>
#include <optional>

/**
 * @brief Concept for ragged storage implementations
 * 
 * Any type satisfying this concept can be used as a storage backend.
 * Use in tests to verify implementations at compile time.
 */
template<typename Storage, typename TData>
concept RaggedStorageConcept = requires(Storage const& s, size_t idx, 
                                        TimeFrameIndex time, EntityId id) {
    // Size operations
    { s.size() } -> std::convertible_to<size_t>;
    { s.empty() } -> std::convertible_to<bool>;
    
    // Element access
    { s.getTime(idx) } -> std::convertible_to<TimeFrameIndex>;
    { s.getData(idx) } -> std::convertible_to<TData const&>;
    { s.getEntityId(idx) } -> std::convertible_to<EntityId>;
    
    // Lookups
    { s.findByEntityId(id) } -> std::convertible_to<std::optional<size_t>>;
    { s.getTimeRange(time) } -> std::convertible_to<std::pair<size_t, size_t>>;
    { s.getTimeCount() } -> std::convertible_to<size_t>;
    
    // Type identification
    { s.getStorageType() } -> std::convertible_to<RaggedStorageType>;
};

// Compile-time verification in test file:
// static_assert(RaggedStorageConcept<OwningRaggedStorage<SimpleData>, SimpleData>);
// static_assert(RaggedStorageConcept<ViewRaggedStorage<SimpleData>, SimpleData>);

#endif // STORAGE_CONCEPTS_HPP
```

**In test file:**
```cpp
#include "utils/StorageConcepts.hpp"

// These will fail to compile if interface is broken
static_assert(RaggedStorageConcept<OwningRaggedStorage<SimpleData>, SimpleData>,
              "OwningRaggedStorage must satisfy RaggedStorageConcept");
static_assert(RaggedStorageConcept<ViewRaggedStorage<SimpleData>, SimpleData>,
              "ViewRaggedStorage must satisfy RaggedStorageConcept");
```

### Key Testing Principles

1. **Mock types isolate storage logic** - When `RaggedStorage.test.cpp` fails, you know it's the storage, not `Mask2D`

2. **Real type tests catch edge cases** - `Mask2D` has specific move semantics that mocks don't replicate

3. **Cache tests are separate** - Cache optimization is a performance feature; test it explicitly

4. **Property tests catch invariant violations** - "Size equals sum of ranges" should always hold

5. **Concepts catch interface drift** - If you change the storage API, compile fails immediately

6. **Entity integration is explicit** - Storage + EntityRegistry tests verify the contract between layers

---

## Timeline Estimate

| Phase | Effort | Status | Dependencies |
|-------|--------|--------|--------------|
| Phase 1: Foundation | 4-6 hours | ✅ **COMPLETED** | None |
| Phase 2: Integration | 4-6 hours | ✅ **COMPLETED** | Phase 1 ✅ |
| Phase 3: Lazy Transforms | 6-8 hours | 🔄 **IN QUEUE** | Phase 2 ✅ |
| Phase 4: Other Data Types | 8-12 hours | ⏳ **PLANNED** | Phase 2 ✅ |
| Phase 5: Testing & Docs | 4-6 hours | ⏳ **PLANNED** | All phases |

**Progress Summary:**
- **Completed:** 8-12 hours (Phase 1 + Phase 2 implemented and tested)
- **Remaining:** 18-26 hours (3 phases)
- **Total Scope:** 26-38 hours
- **Current Achievement:** 21-32% complete

**Recent Achievements (Phase 2):**
- ✅ Migrated `RaggedTimeSeries` to use `RaggedStorageWrapper`
- ✅ Implemented cache optimization with `_cacheOptimizationPointers()` method
- ✅ Updated all mutation methods to invalidate/update cache
- ✅ Modified `RaggedIterator` for fast-path access
- ✅ Added `createView()` static factory for view-based instances
- ✅ Extended `RaggedStorageWrapper` with mutation methods
- ✅ Implemented contiguous view cache detection and optimization
- ✅ Updated all tests to verify contiguous vs non-contiguous behavior
- ✅ All tests passing: LineData, MaskData, PointData verified
- ✅ Build successful with zero errors/warnings

---

---

## Phase 2 Implementation Details

### What Changed in RaggedTimeSeries

**Storage Member:**
```cpp
// Member variable in RaggedTimeSeries<TData>
private:
    RaggedStorageWrapper<TData> _storage;  // Type-erased wrapper
    RaggedStorageCache<TData> _cached_storage;  // Fast-path pointers
```

**Mutation Methods Now Update Cache:**
```cpp
void RaggedTimeSeries<TData>::addAtTime(TimeFrameIndex time, TData const& data, NotifyObservers notify_observers) {
    auto owning = _storage.tryGetMutableOwning();
    if (!owning) throw std::runtime_error("Cannot mutate non-owning storage");
    owning->append(time, data, entity_id);
    _cacheOptimizationPointers();  // Update cache after mutation
    notifyObservers_(notify_observers);
}
```

**Fast-Path Iterator Access:**
```cpp
// In RaggedIterator<TData>::operator*
// Element access uses cached pointers when valid, falls back to virtual dispatch
TimeFrameIndex time = _cached_storage.isValid() 
    ? _cached_storage.getTime(_index)
    : _series->_storage.getTime(_index);
```

**View Creation Factory:**
```cpp
template<typename TData>
std::shared_ptr<RaggedTimeSeries<TData>> RaggedTimeSeries<TData>::createView(
    std::shared_ptr<RaggedTimeSeries<TData> const> source,
    std::unordered_set<EntityId> const& entity_ids)
{
    auto view_storage = std::make_shared<ViewRaggedStorage<TData>>(
        std::const_pointer_cast<RaggedTimeSeries<TData>>(source)->_storage.tryGetMutableOwning()
    );
    view_storage->filterByEntityIds(entity_ids);
    
    auto result = std::make_shared<RaggedTimeSeries<TData>>(source->_time_frame);
    result->_storage = RaggedStorageWrapper<TData>{std::move(view_storage)};
    result->_cacheOptimizationPointers();
    return result;
}
```

### Performance Impact

**Iteration Speed (per element):**
| Scenario | Phase 1 | Phase 2 | Change |
|----------|---------|---------|--------|
| Owning storage | 1 virtual call | 0 virtual calls | **✓ Faster** |
| View storage (filtered) | 2 virtual calls + index map | 1 virtual call + index map | **✓ Faster** |
| View storage (contiguous*) | 2 virtual calls + index map | 0 virtual calls** | **✓✓ Implemented** |
| Memory overhead | 0 bytes | ~24 bytes (3 pointers + size) | Negligible |

\* Contiguous views: all elements selected, or consecutive element range  
\*\* Future optimization - current implementation uses virtual dispatch for all views

**Mutation Overhead:**
- Cache update: O(1) constant time
- No slowdown for append/clear operations

### Backward Compatibility

**API Changes:** None
- All public methods unchanged
- All return types unchanged
- All semantics preserved

**Internal Changes:**
- `_storage` member type changed (transparent to users)
- Cache member added (internal only)
- Iterator implementation optimized (transparent to users)

**Test Impact:** All existing tests pass without modification
- LineData, MaskData, PointData: No changes needed
- Storage abstraction is transparent at usage level

---

## Summary: Storage vs Entity Responsibilities

| Concern | Layer | Class | Notes |
|---------|-------|-------|-------|
| **Store data** | Storage | `OwningRaggedStorage<T>` | SoA layout: times[], data[], entity_ids[] |
| **Store EntityIds** | Storage | `OwningRaggedStorage<T>` | Part of SoA layout |
| **Generate EntityIds** | Container | `LineData::addAtTime()` | Via `EntityRegistry::ensureId()` |
| **Canonical EntityId source** | Entity | `EntityRegistry` | Session-scoped, deterministic |
| **Track provenance** | Entity | `LineageRegistry` | 8 lineage types |
| **Resolve lineage** | Entity | `LineageResolver` | Via `IEntityDataSource` interface |
| **Implement IEntityDataSource** | DataManager | `DataManagerEntityDataSource` | Type dispatch to containers |
| **Expose storage for lineage** | Container | `LineData::getEntityId()` | Called by `DataManagerEntityDataSource` |

**Key insight:** The storage refactoring is **orthogonal** to the Entity system. Storage backends just need to store and retrieve EntityIds. The Entity system handles generation, tracking, and resolution. This separation allows:

1. Testing storage without Entity system (mock EntityIds)
2. Testing Entity system without real storage (mock `IEntityDataSource`)
3. Clean integration where storage provides data, Entity provides meaning

---

## References

- [AnalogDataStorage.hpp](../src/DataManager/AnalogTimeSeries/AnalogDataStorage.hpp) - Reference implementation
- [Analog_Time_Series.hpp](../src/DataManager/AnalogTimeSeries/Analog_Time_Series.hpp) - DataStorageWrapper and cache optimization
- [RaggedStorage.hpp](../src/DataManager/utils/RaggedStorage.hpp) - Current CRTP implementation
- [Entity/Lineage/LineageTypes.hpp](../src/Entity/Lineage/LineageTypes.hpp) - 8 lineage strategies
- [Entity/Lineage/LineageResolver.hpp](../src/Entity/Lineage/LineageResolver.hpp) - IEntityDataSource interface
- [LINEAGE_REFACTORING_PLAN.md](../src/Entity/LINEAGE_REFACTORING_PLAN.md) - Entity system architecture
