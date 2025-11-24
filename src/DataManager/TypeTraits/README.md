# DataManager Type Traits System

## Overview

This directory contains the type trait infrastructure for DataManager container types. Type traits provide compile-time metadata about data containers, enabling generic programming patterns and type-safe transformations.

## Purpose

Type traits serve as the **fundamental layer of introspection** for DataManager types. They answer questions like:
- What element type does this container hold?
- Can this container have multiple values per time point? (raggedness)
- Does this container track EntityIds?
- Does this container represent spatial data?

These properties are used by:
- **Transform system** (v2): Determine appropriate output container types
- **Generic algorithms**: Write code that works across multiple container types
- **UI widgets**: Adapt display based on container properties
- **Serialization**: Handle different storage strategies

## Design Philosophy

### Intrusive Traits (Recommended)

Each container type defines its own `DataTraits` nested struct:

```cpp
class MaskData : public RaggedTimeSeries<Mask2D> {
public:
    struct DataTraits : TypeTraits::DataTypeTraitsBase<MaskData, Mask2D> {
        static constexpr bool is_ragged = true;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = true;
        static constexpr bool is_spatial = true;
    };
    
    // ... rest of class
};
```

**Benefits:**
- Self-documenting: traits are co-located with type definition
- Minimal header dependencies
- Easy to add custom properties per type
- No central registry to maintain

### Usage in Generic Code

```cpp
#include "TypeTraits/DataTypeTraits.hpp"

// Query traits
using namespace WhiskerToolbox::TypeTraits;

static_assert(is_ragged_v<MaskData>, "MaskData should be ragged");
using ElementType = element_type_t<MaskData>;  // Mask2D

// Use concepts
template<RaggedContainer T>
void processRaggedData(T const& container) {
    // Works for MaskData, LineData, PointData, RaggedAnalogTimeSeries
}

template<typename T>
requires TemporalContainer<T> && has_entity_ids_v<T>
void processTrackedTimeSeries(T const& container) {
    // Works for containers with TimeFrame and EntityId tracking
}
```

## Standard Trait Properties

All container types should define these properties:

| Property | Type | Description |
|----------|------|-------------|
| `container_type` | `using` | The container type itself |
| `element_type` | `using` | Type of elements stored (e.g., `Mask2D`, `float`) |
| `is_ragged` | `bool` | True if multiple elements can exist at each time point |
| `is_temporal` | `bool` | True if container has TimeFrame association |
| `has_entity_ids` | `bool` | True if elements are tracked with EntityIds |
| `is_spatial` | `bool` | True if elements represent spatial data |

## Container Type Classification

### Ragged Containers (`is_ragged = true`)
- Multiple elements per time point
- Examples: `MaskData`, `LineData`, `PointData`, `RaggedAnalogTimeSeries`
- Iterator access: `time_slices()`, `elements()`

### Non-Ragged Containers (`is_ragged = false`)
- Single value per time point
- Examples: `AnalogTimeSeries`, `DigitalEventSeries`
- Iterator access: `getAllSamples()`, `getTimeIndices()`

## Adding Traits to a New Container Type

1. Include the trait header:
```cpp
#include "TypeTraits/DataTypeTraits.hpp"
```

2. Define a nested `DataTraits` struct:
```cpp
class MyNewContainer {
public:
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<MyNewContainer, MyElement> {
        static constexpr bool is_ragged = false;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = false;
        // Add custom properties as needed
        static constexpr bool is_my_special_property = true;
    };
    
    // ... rest of class
};
```

3. Verify with static assertions in your tests:
```cpp
#include "TypeTraits/DataTypeTraits.hpp"

static_assert(WhiskerToolbox::TypeTraits::HasDataTraits<MyNewContainer>);
static_assert(!WhiskerToolbox::TypeTraits::is_ragged_v<MyNewContainer>);
```

## Integration with Transform System

The transform system (`transforms/v2/core/ContainerTraits.hpp`) uses these traits to:
- Map element types to container types
- Determine appropriate output containers for transforms
- Validate type compatibility

```cpp
// ContainerTraits now queries DataTraits instead of defining its own
template<typename T>
inline constexpr bool is_ragged_v = WhiskerToolbox::TypeTraits::is_ragged_v<T>;
```

## Future Extensions

Potential additional trait properties:
- `supports_interpolation`: Can values be interpolated between time points?
- `has_provenance`: Does container track transform provenance?
- `storage_strategy`: Eager, lazy, or on-demand computation?
- `supports_parallel_iteration`: Can be safely iterated in parallel?

## Related Documentation

- Transform System V2: `transforms/v2/DESIGN.md`
- Container Traits: `transforms/v2/core/ContainerTraits.hpp`
- Type Index Mapping: `transforms/v2/core/ContainerTraits.cpp`
