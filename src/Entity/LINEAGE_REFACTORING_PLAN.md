# Entity Library Lineage Refactoring Plan

## Executive Summary

This document outlines a refactoring plan to move lineage tracking functionality from
`DataManager` into the `Entity` library. The goal is to create a clean, minimal-dependency
Entity library that provides both entity identification and lineage tracking, making it
reusable across different data management systems.

## Current Architecture Analysis

### Current State

```
src/Entity/                          src/DataManager/Lineage/
├── EntityTypes.hpp                  ├── LineageTypes.hpp
├── EntityRegistry.hpp/cpp           ├── LineageRegistry.hpp/cpp
├── EntityGroupManager.hpp/cpp       ├── EntityResolver.hpp/cpp
└── EntityGroupManager.test.cpp      └── LineageRecorder.hpp/cpp

src/DataManager/Observer/            
├── Observer_Data.hpp                
├── Observer_Data.cpp                
├── Observer_Data.test.cpp           
└── CMakeLists.txt                   
```

**Dependencies:**
- `Entity` library: Depends on `TimeFrame`, includes `DataManager/Observer` (problematic)
- `Observer` library: **No dependencies** (static library, self-contained)
- `Lineage` subsystem: Depends on `DataManager`, all data types (MaskData, LineData, etc.)

### Identified Issues

#### 1. **Tight Coupling in EntityResolver**
The `EntityResolver` class has ~640 lines of code that directly depends on:
- `DataManager.hpp` for data access
- Every data type (`MaskData`, `LineData`, `PointData`, `DigitalEventSeries`, etc.)
- Switch statements dispatching on `DM_DataType` enum

```cpp
// Current problematic pattern in EntityResolver.cpp
DM_DataType const type = _dm->getType(data_key);
switch (type) {
    case DM_DataType::Line:
        if (auto data = _dm->getData<LineData>(data_key)) { ... }
    case DM_DataType::Mask:
        if (auto data = _dm->getData<MaskData>(data_key)) { ... }
    // ... 10+ more cases
}
```

#### 2. **LineageTypes Already Has Minimal Dependencies**
`LineageTypes.hpp` only includes:
- `Entity/EntityTypes.hpp` (already in Entity library)
- Standard library headers

This file could be moved to Entity with no changes.

#### 3. **LineageRegistry Has No DataManager Dependencies**
`LineageRegistry.hpp/cpp` only depends on:
- `LineageTypes.hpp`
- Standard library headers

This could also be moved to Entity with no changes.

#### 4. **Observer Library is Misplaced**
The `ObserverData` library in `DataManager/Observer/` is actually a **self-contained
static library with no dependencies**. It should be promoted to a top-level library
(sibling to Entity, TimeFrame, etc.) and Entity should properly depend on it.

Currently `EntityGroupManager` includes `DataManager/Observer/Observer_Data.hpp`
which creates an implicit (and incorrect) dependency path.

#### 5. **API Inconsistency**
- `EntityRegistry` is thread-safe (uses `std::mutex`)
- `LineageRegistry` is not thread-safe (caller must synchronize)
- `EntityGroupManager` is not thread-safe

---

## Proposed Architecture

### New Library Structure

```
src/
├── Observer/                        # MOVED: Promoted to top-level library
│   ├── CMakeLists.txt
│   ├── Observer_Data.hpp
│   ├── Observer_Data.cpp
│   └── Observer_Data.test.cpp
│
├── Entity/                          # UPDATED: Now depends on Observer
│   ├── CMakeLists.txt
│   ├── EntityTypes.hpp
│   ├── EntityRegistry.hpp/cpp
│   ├── EntityRegistry.test.cpp
│   ├── EntityGroupManager.hpp/cpp
│   ├── EntityGroupManager.test.cpp
│   └── Lineage/                     # NEW: Moved from DataManager
│       ├── LineageTypes.hpp
│       ├── LineageRegistry.hpp/cpp
│       ├── LineageRegistry.test.cpp
│       └── LineageResolver.hpp/cpp
│       └── LineageResolver.test.cpp
│
├── DataManager/
│   ├── Lineage/                     # SIMPLIFIED: Only DataManager-specific code
│       ├── EntityResolver.hpp/cpp   # Stays here, implements LineageResolver
│       ├── LineageRecorder.hpp/cpp  # Stays here, depends on transforms
│       └── DataManagerEntityDataSource.hpp/cpp  # NEW: Bridges to Entity
│
└── TimeFrame/                       # Unchanged
```

### Dependency Graph (After Refactoring)

```
                    ┌──────────────┐
                    │   Observer   │  (no dependencies)
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │  TimeFrame   │  (no dependencies)
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │    Entity    │  depends on: Observer, TimeFrame
                    │  + Lineage   │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │ DataManager  │  depends on: Entity, TimeFrame, Observer
                    └──────────────┘
```

---

## Implementation Phases

### Phase 1: Promote Observer Library to Top-Level (Week 1)

The `ObserverData` library in `DataManager/Observer/` is already a self-contained
static library with no dependencies. We simply need to move it to be a sibling
of Entity and TimeFrame.

#### 1.1 Move Observer Library

Move the entire `src/DataManager/Observer/` directory to `src/Observer/`:

```bash
# File movements
src/DataManager/Observer/CMakeLists.txt    → src/Observer/CMakeLists.txt
src/DataManager/Observer/Observer_Data.hpp → src/Observer/Observer_Data.hpp
src/DataManager/Observer/Observer_Data.cpp → src/Observer/Observer_Data.cpp
src/DataManager/Observer/Observer_Data.test.cpp → src/Observer/Observer_Data.test.cpp
```

#### 1.2 Update Observer CMakeLists.txt

Minor updates for new location:

```cmake
# src/Observer/CMakeLists.txt

add_library(ObserverData STATIC
    Observer_Data.hpp
    Observer_Data.cpp
)

target_include_directories(ObserverData PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"  # For "Observer/..." includes
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# ... compiler flags unchanged ...

add_library(WhiskerToolbox::ObserverData ALIAS ObserverData)
```

#### 1.3 Update Entity Library Dependencies

Update Entity to properly depend on the Observer library:

```cmake
# src/Entity/CMakeLists.txt (updated)

target_link_libraries(Entity PUBLIC 
    WhiskerToolbox::TimeFrame
    WhiskerToolbox::ObserverData  # NEW: Explicit dependency
)
```

#### 1.4 Update Top-Level CMakeLists.txt

Ensure Observer is built before Entity:

```cmake
# src/CMakeLists.txt (snippet)

add_subdirectory(Observer)      # NEW: Add before Entity
add_subdirectory(TimeFrame)
add_subdirectory(Entity)        # Now properly depends on Observer
add_subdirectory(DataManager)
```

#### 1.5 Update EntityGroupManager Include

Update all files that include Observer_Data to use the new path:

```cpp
// src/Entity/EntityGroupManager.hpp

// Change from:
#include "DataManager/Observer/Observer_Data.hpp"

// To:
#include "Observer/Observer_Data.hpp"
```

#### 1.6 Fix All Observer Include Paths

Update all files that include `DataManager/Observer/Observer_Data.hpp` to use `Observer/Observer_Data.hpp`.
This may include files in DataManager and other libraries.

#### Deliverables - Phase 1
- [ ] `src/Observer/` directory created with moved files
- [ ] `src/Observer/CMakeLists.txt` updated for new location
- [ ] Entity CMakeLists.txt updated with Observer dependency
- [ ] Top-level CMakeLists.txt updated with build order
- [ ] All include paths updated to `Observer/Observer_Data.hpp`
- [ ] Old `DataManager/Observer/` directory removed
- [ ] All tests passing

---

### Phase 2: Move Lineage Types to Entity (Week 1-2)

#### 2.1 Move LineageTypes.hpp

Move `src/DataManager/Lineage/LineageTypes.hpp` to `src/Entity/Lineage/LineageTypes.hpp`:

**Required Changes:**
1. Update include path: `"Entity/EntityTypes.hpp"` → `"../EntityTypes.hpp"` or `"EntityTypes.hpp"`
2. Update namespace: Keep `WhiskerToolbox::Lineage` or move to `WhiskerToolbox::Entity::Lineage`

**Recommended Namespace Structure:**
```cpp
namespace WhiskerToolbox::Entity::Lineage {
    // Types moved here
}

// For convenience, alias in the old location (deprecated)
namespace WhiskerToolbox::Lineage = WhiskerToolbox::Entity::Lineage;
```

#### 2.2 Move LineageRegistry

Move `src/DataManager/Lineage/LineageRegistry.hpp/cpp` to `src/Entity/Lineage/`:

**Required Changes:**
1. Update include paths
2. Update namespace
3. Optionally add thread-safety (to match EntityRegistry)

**Also move the unit test:**
```bash
src/DataManager/Lineage/LineageRegistry.test.cpp → src/Entity/Lineage/LineageRegistry.test.cpp
```

#### 2.3 Add Thread-Safety to LineageRegistry (Optional)

For consistency with `EntityRegistry`, consider making `LineageRegistry` thread-safe:

```cpp
class LineageRegistry {
private:
    mutable std::mutex _mutex;  // Add mutex protection
    // ... existing members
};
```

**Decision Point:** Thread-safety adds overhead. If lineage operations are always
performed on the UI thread, this may be unnecessary. Document the thread-safety
contract clearly either way.

#### 2.4 Create Backward Compatibility Headers

```cpp
// src/DataManager/Lineage/LineageTypes.hpp (deprecated)
#ifndef WHISKERTOOLBOX_LINEAGE_TYPES_HPP_DEPRECATED
#define WHISKERTOOLBOX_LINEAGE_TYPES_HPP_DEPRECATED

#pragma message("Warning: Include Entity/Lineage/LineageTypes.hpp instead")

#include "Entity/Lineage/LineageTypes.hpp"

// Re-export in old namespace for compatibility
namespace WhiskerToolbox::Lineage {
    using namespace WhiskerToolbox::Entity::Lineage;
}

#endif
```

#### Deliverables - Phase 2
- [ ] `src/Entity/Lineage/` directory created
- [ ] `LineageTypes.hpp` moved with updated includes
- [ ] `LineageRegistry.hpp/cpp` moved with updated includes
- [ ] `LineageRegistry.test.cpp` moved to `src/Entity/Lineage/`
- [ ] Backward compatibility headers in old location (deprecated)
- [ ] Update DataManager CMakeLists.txt
- [ ] Update Entity CMakeLists.txt to include Lineage sources

---

### Phase 3: Create Abstract Resolution Interface (Week 2)

#### 3.1 Design the Resolver Interface

The key insight is that lineage *types* and *storage* don't need DataManager, but
*resolution* (actually fetching EntityIds from containers) does. We solve this
with an abstract interface:

```cpp
// src/Entity/Lineage/LineageResolver.hpp
#ifndef ENTITY_LINEAGE_RESOLVER_HPP
#define ENTITY_LINEAGE_RESOLVER_HPP

#include "Entity/EntityTypes.hpp"
#include "Entity/Lineage/LineageTypes.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <string>
#include <vector>
#include <unordered_set>

namespace WhiskerToolbox::Entity::Lineage {

/**
 * @brief Interface for data-source-specific entity resolution
 * 
 * This interface abstracts away the concrete data storage from lineage
 * resolution. Implementations (e.g., DataManagerResolver) provide the
 * actual data access.
 * 
 * By programming to this interface, the lineage system can be tested
 * independently of DataManager and reused with different storage backends.
 */
class IEntityDataSource {
public:
    virtual ~IEntityDataSource() = default;

    /**
     * @brief Get EntityIds from a container at a specific time and index
     * 
     * @param data_key Container identifier
     * @param time Time frame index
     * @param local_index Index within that time (for ragged containers)
     * @return EntityIds found at that location (empty if none/not found)
     */
    [[nodiscard]] virtual std::vector<EntityId> getEntityIds(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index) const = 0;

    /**
     * @brief Get ALL EntityIds from a container at a specific time
     * 
     * @param data_key Container identifier
     * @param time Time frame index
     * @return All EntityIds at that time
     */
    [[nodiscard]] virtual std::vector<EntityId> getAllEntityIdsAtTime(
        std::string const& data_key,
        TimeFrameIndex time) const = 0;

    /**
     * @brief Get all EntityIds in a container (across all times)
     * 
     * @param data_key Container identifier
     * @return Set of all EntityIds in the container
     */
    [[nodiscard]] virtual std::unordered_set<EntityId> getAllEntityIds(
        std::string const& data_key) const = 0;

    /**
     * @brief Get the count of elements at a specific time (for iteration)
     * 
     * @param data_key Container identifier
     * @param time Time frame index
     * @return Number of elements at that time
     */
    [[nodiscard]] virtual std::size_t getElementCount(
        std::string const& data_key,
        TimeFrameIndex time) const = 0;
};

/**
 * @brief Generic lineage resolver using abstract data source
 * 
 * This class implements lineage resolution logic without knowing the
 * concrete data types. It uses an IEntityDataSource to access EntityIds.
 */
class LineageResolver {
public:
    /**
     * @brief Construct resolver with data source and registry
     * 
     * @param data_source Provides entity lookup (caller owns, must outlive resolver)
     * @param registry Lineage metadata (caller owns, must outlive resolver)
     */
    LineageResolver(IEntityDataSource const* data_source,
                    LineageRegistry const* registry);

    /**
     * @brief Resolve derived element to source EntityIds (single step)
     */
    [[nodiscard]] std::vector<EntityId> resolveToSource(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index = 0) const;

    /**
     * @brief Resolve all the way to root source containers
     */
    [[nodiscard]] std::vector<EntityId> resolveToRoot(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index = 0) const;

    /**
     * @brief Resolve by EntityId to parent EntityIds
     */
    [[nodiscard]] std::vector<EntityId> resolveByEntityId(
        std::string const& data_key,
        EntityId derived_entity_id) const;

    /**
     * @brief Get lineage chain (data keys from derived to sources)
     */
    [[nodiscard]] std::vector<std::string> getLineageChain(
        std::string const& data_key) const;

    // Delegated queries
    [[nodiscard]] bool hasLineage(std::string const& data_key) const;
    [[nodiscard]] bool isSource(std::string const& data_key) const;

private:
    IEntityDataSource const* _data_source;
    LineageRegistry const* _registry;

    // Resolution strategy dispatch (moved from EntityResolver)
    [[nodiscard]] std::vector<EntityId> resolveOneToOne(
        OneToOneByTime const& lineage,
        TimeFrameIndex time,
        std::size_t local_index) const;

    [[nodiscard]] std::vector<EntityId> resolveAllToOne(
        AllToOneByTime const& lineage,
        TimeFrameIndex time) const;

    // ... other resolution methods
};

} // namespace WhiskerToolbox::Entity::Lineage

#endif // ENTITY_LINEAGE_RESOLVER_HPP
```

#### 3.2 Implement LineageResolver

Move the resolution logic from `EntityResolver` to `LineageResolver`, but use the
abstract `IEntityDataSource` interface instead of direct DataManager calls.

**Create unit test alongside implementation:**
```bash
src/Entity/Lineage/LineageResolver.hpp
src/Entity/Lineage/LineageResolver.cpp
src/Entity/Lineage/LineageResolver.test.cpp  # Unit test with mock data source
```

```cpp
// src/Entity/Lineage/LineageResolver.cpp

std::vector<EntityId> LineageResolver::resolveToSource(
    std::string const& data_key,
    TimeFrameIndex time,
    std::size_t local_index) const
{
    if (!_data_source || !_registry) {
        return {};
    }

    auto lineage_opt = _registry->getLineage(data_key);
    if (!lineage_opt) {
        // No lineage - return EntityIds from container itself
        return _data_source->getEntityIds(data_key, time, local_index);
    }

    return std::visit(
        [this, &data_key, &time, local_index](auto const& lineage) -> std::vector<EntityId> {
            using T = std::decay_t<decltype(lineage)>;

            if constexpr (std::is_same_v<T, Source>) {
                return _data_source->getEntityIds(data_key, time, local_index);
            } else if constexpr (std::is_same_v<T, OneToOneByTime>) {
                return _data_source->getEntityIds(lineage.source_key, time, local_index);
            } else if constexpr (std::is_same_v<T, AllToOneByTime>) {
                return _data_source->getAllEntityIdsAtTime(lineage.source_key, time);
            }
            // ... etc
        },
        *lineage_opt);
}
```

#### 3.3 Unit Test with Mock Data Source

```cpp
// src/Entity/Lineage/LineageResolver.test.cpp

#include <catch2/catch_test_macros.hpp>
#include "Entity/Lineage/LineageResolver.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"

namespace {

// Mock data source for testing without DataManager
class MockEntityDataSource : public WhiskerToolbox::Entity::Lineage::IEntityDataSource {
public:
    void setEntityIds(std::string const& key, TimeFrameIndex time,
                      std::vector<EntityId> ids) {
        _entity_ids[{key, time.getValue()}] = std::move(ids);
    }

    std::vector<EntityId> getEntityIds(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index) const override
    {
        auto it = _entity_ids.find({data_key, time.getValue()});
        if (it == _entity_ids.end()) return {};
        if (local_index >= it->second.size()) return {};
        return {it->second[local_index]};
    }

    std::vector<EntityId> getAllEntityIdsAtTime(
        std::string const& data_key,
        TimeFrameIndex time) const override
    {
        auto it = _entity_ids.find({data_key, time.getValue()});
        if (it == _entity_ids.end()) return {};
        return it->second;
    }

    // ... other overrides
private:
    std::map<std::pair<std::string, int64_t>, std::vector<EntityId>> _entity_ids;
};

} // anonymous namespace

TEST_CASE("LineageResolver with mock data source", "[entity][lineage]") {
    using namespace WhiskerToolbox::Entity::Lineage;
    
    MockEntityDataSource data_source;
    LineageRegistry registry;

    // Configure mock
    data_source.setEntityIds("masks", TimeFrameIndex(10), 
                              {EntityId(100), EntityId(101)});

    // Setup lineage
    registry.setLineage("mask_areas", OneToOneByTime{"masks"});

    // Create resolver
    LineageResolver resolver(&data_source, &registry);

    SECTION("OneToOneByTime resolution") {
        auto ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == EntityId(100));
    }
    
    SECTION("AllToOneByTime resolution") {
        registry.setLineage("total_area", AllToOneByTime{"masks"});
        auto ids = resolver.resolveToSource("total_area", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 2);
    }
}
```

#### Deliverables - Phase 3
- [ ] `src/Entity/Lineage/LineageResolver.hpp` - Interface + generic resolver
- [ ] `src/Entity/Lineage/LineageResolver.cpp` - Implementation
- [ ] `src/Entity/Lineage/LineageResolver.test.cpp` - Unit tests with mock
- [ ] Documentation for implementing IEntityDataSource

---

### Phase 4: DataManager Integration Adapter (Week 3)

#### 4.1 Implement DataManagerEntityDataSource

Create an adapter in DataManager that implements `IEntityDataSource`:

```cpp
// src/DataManager/Lineage/DataManagerEntityDataSource.hpp
#ifndef DATAMANAGER_ENTITY_DATA_SOURCE_HPP
#define DATAMANAGER_ENTITY_DATA_SOURCE_HPP

#include "Entity/Lineage/LineageResolver.hpp"

// Forward declaration
class DataManager;

namespace WhiskerToolbox::Lineage {

/**
 * @brief Implements IEntityDataSource using DataManager for data access
 * 
 * This adapter bridges the abstract Entity lineage system to the
 * concrete DataManager storage.
 */
class DataManagerEntityDataSource 
    : public WhiskerToolbox::Entity::Lineage::IEntityDataSource 
{
public:
    explicit DataManagerEntityDataSource(DataManager const* dm);

    [[nodiscard]] std::vector<EntityId> getEntityIds(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index) const override;

    [[nodiscard]] std::vector<EntityId> getAllEntityIdsAtTime(
        std::string const& data_key,
        TimeFrameIndex time) const override;

    [[nodiscard]] std::unordered_set<EntityId> getAllEntityIds(
        std::string const& data_key) const override;

    [[nodiscard]] std::size_t getElementCount(
        std::string const& data_key,
        TimeFrameIndex time) const override;

private:
    DataManager const* _dm;
};

} // namespace WhiskerToolbox::Lineage

#endif
```

```cpp
// src/DataManager/Lineage/DataManagerEntityDataSource.cpp
#include "Lineage/DataManagerEntityDataSource.hpp"
#include "DataManager.hpp"

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"

// The switch-on-type logic moves here, contained to DataManager layer

std::vector<EntityId> DataManagerEntityDataSource::getEntityIds(
    std::string const& data_key,
    TimeFrameIndex time,
    std::size_t local_index) const
{
    if (!_dm) return {};

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                // ... existing logic from EntityResolver
            }
            break;
        // ... all other cases
    }
    return {};
}
```

#### 4.2 Update EntityResolver to Use New Infrastructure

Refactor `EntityResolver` to compose with the new classes:

```cpp
// src/DataManager/Lineage/EntityResolver.hpp (simplified)

class EntityResolver {
public:
    explicit EntityResolver(DataManager* dm);

    // Public API unchanged - maintains backward compatibility
    [[nodiscard]] std::vector<EntityId> resolveToSource(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index = 0) const;

    [[nodiscard]] std::vector<EntityId> resolveToRoot(
        std::string const& data_key,
        TimeFrameIndex time,
        std::size_t local_index = 0) const;

    // ... other public methods unchanged

private:
    DataManager* _dm;
    std::unique_ptr<DataManagerEntityDataSource> _data_source;
    std::unique_ptr<Entity::Lineage::LineageResolver> _resolver;
};
```

```cpp
// src/DataManager/Lineage/EntityResolver.cpp (simplified)

EntityResolver::EntityResolver(DataManager* dm)
    : _dm(dm)
{
    if (_dm) {
        _data_source = std::make_unique<DataManagerEntityDataSource>(_dm);
        _resolver = std::make_unique<Entity::Lineage::LineageResolver>(
            _data_source.get(),
            _dm->getLineageRegistry()
        );
    }
}

std::vector<EntityId> EntityResolver::resolveToSource(
    std::string const& data_key,
    TimeFrameIndex time,
    std::size_t local_index) const
{
    if (!_resolver) return {};
    return _resolver->resolveToSource(data_key, time, local_index);
}
```

#### Deliverables - Phase 4
- [ ] `DataManagerEntityDataSource.hpp/cpp` implementing `IEntityDataSource`
- [ ] Refactored `EntityResolver` using composition
- [ ] All existing tests passing
- [ ] No API changes visible to consumers

---

### Phase 5: Update CMake and Dependencies (Week 3)

#### 5.1 Update Entity CMakeLists.txt

```cmake
# src/Entity/CMakeLists.txt

add_library(Entity SHARED
    # Core types
    EntityTypes.hpp
    EntityRegistry.hpp
    EntityRegistry.cpp
    EntityGroupManager.hpp
    EntityGroupManager.cpp
    
    # Lineage subsystem
    Lineage/LineageTypes.hpp
    Lineage/LineageRegistry.hpp
    Lineage/LineageRegistry.cpp
    Lineage/LineageResolver.hpp
    Lineage/LineageResolver.cpp
)

target_include_directories(Entity PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# Dependencies: Observer (for notifications), TimeFrame (for time types)
target_link_libraries(Entity PUBLIC 
    WhiskerToolbox::ObserverData
    WhiskerToolbox::TimeFrame
)

add_library(WhiskerToolbox::Entity ALIAS Entity)
```

#### 5.2 Update DataManager CMakeLists.txt

Remove moved files, keep adapter layer:

```cmake
# In DataManager CMakeLists.txt
set(LINEAGE_SOURCES
    Lineage/EntityResolver.cpp
    Lineage/LineageRecorder.cpp
    Lineage/DataManagerEntityDataSource.cpp
)

# DataManager depends on Entity (which now includes lineage types)
target_link_libraries(DataManager PUBLIC WhiskerToolbox::Entity)
```

#### 5.3 Update Top-Level src/CMakeLists.txt

Ensure correct build order:

```cmake
# src/CMakeLists.txt

# Foundation libraries (no internal dependencies)
add_subdirectory(Observer)
add_subdirectory(TimeFrame)

# Entity depends on Observer and TimeFrame
add_subdirectory(Entity)

# DataManager depends on Entity
add_subdirectory(DataManager)

# ... rest of libraries
```

#### 5.4 Add Backward Compatibility Headers

Create deprecated forwarding headers in the old locations:

```cpp
// src/DataManager/Lineage/LineageTypes.hpp (deprecated)
#ifndef WHISKERTOOLBOX_LINEAGE_TYPES_HPP
#define WHISKERTOOLBOX_LINEAGE_TYPES_HPP

#pragma message("Warning: Include Entity/Lineage/LineageTypes.hpp instead of DataManager/Lineage/LineageTypes.hpp")

#include "Entity/Lineage/LineageTypes.hpp"

// Re-export in old namespace for compatibility
namespace WhiskerToolbox::Lineage {
    using namespace WhiskerToolbox::Entity::Lineage;
}

#endif
```

#### Deliverables - Phase 5
- [ ] Updated Entity CMakeLists.txt with Lineage sources
- [ ] Updated DataManager CMakeLists.txt
- [ ] Updated top-level src/CMakeLists.txt for build order
- [ ] Backward compatibility headers
- [ ] Verify no circular dependencies
- [ ] CI builds passing on all platforms

---

### Phase 6: Integration Testing and Documentation (Week 4)

#### 6.1 Integration Tests

Integration tests go in the `tests/` directory, mirroring the `src/` structure:

```
tests/
├── Entity/
│   └── Lineage/
│       └── LineageIntegration.test.cpp  # Integration tests
└── DataManager/
    └── Lineage/
        └── EntityResolverIntegration.test.cpp  # Full stack tests
```

```cpp
// tests/DataManager/Lineage/EntityResolverIntegration.test.cpp

#include <catch2/catch_test_macros.hpp>
#include "DataManager.hpp"
#include "Lineage/EntityResolver.hpp"
#include "Masks/Mask_Data.hpp"

TEST_CASE("Full lineage integration with DataManager", "[lineage][integration]") {
    DataManager dm;
    
    // Setup source data
    auto masks = std::make_shared<MaskData>();
    // ... add test data
    dm.setData("masks", masks, TimeKey("time"));
    
    // Setup derived data with lineage
    auto areas = computeMaskAreas(masks);
    dm.setData("areas", areas, TimeKey("time"));
    dm.getLineageRegistry()->setLineage("areas", 
        WhiskerToolbox::Entity::Lineage::OneToOneByTime{"masks"});
    
    // Test resolution
    EntityResolver resolver(&dm);
    auto source_ids = resolver.resolveToSource("areas", TimeFrameIndex(10));
    
    // Verify resolution works
    REQUIRE_FALSE(source_ids.empty());
}
```

#### 6.2 Unit Test File Locations (Recap)

Unit tests are co-located with their translation units:

```
src/Observer/
├── Observer_Data.hpp
├── Observer_Data.cpp
└── Observer_Data.test.cpp          # Unit test (moved with library)

src/Entity/
├── EntityRegistry.hpp/cpp
├── EntityRegistry.test.cpp         # Unit test (already exists)
├── EntityGroupManager.hpp/cpp
├── EntityGroupManager.test.cpp     # Unit test (already exists)
└── Lineage/
    ├── LineageRegistry.hpp/cpp
    ├── LineageRegistry.test.cpp    # Unit test (moved from DataManager)
    ├── LineageResolver.hpp/cpp
    └── LineageResolver.test.cpp    # Unit test (new)

src/DataManager/Lineage/
├── EntityResolver.hpp/cpp
├── EntityResolver.test.cpp         # Unit test (already exists)
├── DataManagerEntityDataSource.hpp/cpp
└── DataManagerEntityDataSource.test.cpp  # Unit test (new)
```

#### 6.3 Documentation Updates

- [ ] Update `docs/developer/data_manager.qmd` with new architecture
- [ ] Create `docs/developer/entity_library.qmd` for Entity library docs
- [ ] Add migration guide for users of old API
- [ ] Update API reference documentation

#### Deliverables - Phase 6
- [ ] Integration tests in `tests/` directory
- [ ] All unit tests co-located with source files and passing
- [ ] 90%+ code coverage for new code
- [ ] Updated developer documentation
- [ ] Migration guide

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking existing code | Medium | High | Backward compat headers; extensive testing |
| Circular dependency | Low | High | Careful CMake ordering; dependency analysis |
| Performance regression | Low | Medium | Benchmark before/after; inline critical paths |
| API complexity increase | Medium | Low | Hide complexity behind existing APIs |
| Build time increase | Low | Low | Entity library is small |

---

## API Stability Commitments

### Stable (No Changes)
- `EntityId`, `EntityKind`, `EntityDescriptor`, `EntityTupleKey`
- `EntityRegistry` public API
- `EntityGroupManager` public API
- `EntityResolver` public API (in DataManager)

### Deprecated (Keep Working, Mark for Removal)
- `#include "DataManager/Lineage/LineageTypes.hpp"` → Use `Entity/Lineage/LineageTypes.hpp`
- `#include "DataManager/Lineage/LineageRegistry.hpp"` → Use `Entity/Lineage/LineageRegistry.hpp`
- `WhiskerToolbox::Lineage` namespace → Use `WhiskerToolbox::Entity::Lineage`

### New APIs
- `IEntityDataSource` interface
- `LineageResolver` (in Entity library)
- `DataManagerEntityDataSource` (in DataManager)

---

## Success Criteria

1. **Independence:** Entity library has no DataManager dependency
2. **Testability:** Lineage can be unit tested without DataManager
3. **Backward Compatibility:** Existing code compiles with deprecation warnings only
4. **Performance:** No measurable performance regression (< 5% overhead)
5. **Thread Safety:** Consistent thread-safety contracts documented
6. **Documentation:** All new APIs documented with examples

---

## Timeline Summary

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1: Observer | 3 days | None |
| Phase 2: Move Lineage Types | 3 days | Phase 1 |
| Phase 3: Abstract Resolver | 4 days | Phase 2 |
| Phase 4: DataManager Adapter | 3 days | Phase 3 |
| Phase 5: CMake Updates | 2 days | Phase 4 |
| Phase 6: Testing/Docs | 5 days | Phase 5 |

**Total Estimated Time:** 4 weeks

---

## Open Questions for Review

1. **Namespace Decision:** Should we use `WhiskerToolbox::Entity::Lineage` or keep
   `WhiskerToolbox::Lineage` as a top-level namespace?

2. **Thread Safety:** Should `LineageRegistry` be made thread-safe to match
   `EntityRegistry`? This adds overhead but ensures consistent behavior.

3. **Deprecation Timeline:** How long should backward compatibility headers be
   maintained? Suggest: 2 major releases.

4. **Virtual Function Overhead:** Is the virtual function call overhead in
   `IEntityDataSource` acceptable? Could use CRTP for static dispatch if needed.

---

## Appendix: File Movement Summary

| Current Location | New Location | Action |
|-----------------|--------------|--------|
| `DataManager/Observer/Observer_Data.hpp` | `Observer/Observer_Data.hpp` | Move |
| `DataManager/Observer/Observer_Data.cpp` | `Observer/Observer_Data.cpp` | Move |
| `DataManager/Observer/Observer_Data.test.cpp` | `Observer/Observer_Data.test.cpp` | Move |
| `DataManager/Observer/CMakeLists.txt` | `Observer/CMakeLists.txt` | Move + Update |
| `DataManager/Lineage/LineageTypes.hpp` | `Entity/Lineage/LineageTypes.hpp` | Move |
| `DataManager/Lineage/LineageRegistry.hpp` | `Entity/Lineage/LineageRegistry.hpp` | Move |
| `DataManager/Lineage/LineageRegistry.cpp` | `Entity/Lineage/LineageRegistry.cpp` | Move |
| `DataManager/Lineage/LineageRegistry.test.cpp` | `Entity/Lineage/LineageRegistry.test.cpp` | Move |
| `DataManager/Lineage/EntityResolver.hpp` | (stays) | Refactor |
| `DataManager/Lineage/EntityResolver.cpp` | (stays) | Refactor |
| `DataManager/Lineage/EntityResolver.test.cpp` | (stays) | Update |
| `DataManager/Lineage/LineageRecorder.hpp` | (stays) | Update includes |
| `DataManager/Lineage/LineageRecorder.cpp` | (stays) | Update includes |
| (new) | `Entity/Lineage/LineageResolver.hpp` | Create |
| (new) | `Entity/Lineage/LineageResolver.cpp` | Create |
| (new) | `Entity/Lineage/LineageResolver.test.cpp` | Create |
| (new) | `DataManager/Lineage/DataManagerEntityDataSource.hpp` | Create |
| (new) | `DataManager/Lineage/DataManagerEntityDataSource.cpp` | Create |
| (new) | `DataManager/Lineage/DataManagerEntityDataSource.test.cpp` | Create |
| `DataManager/Observer/` | (deleted) | Remove after moving |
| (new) | `DataManager/Lineage/LineageTypes.hpp` | Deprecated forwarding header |
| (new) | `DataManager/Lineage/LineageRegistry.hpp` | Deprecated forwarding header |
