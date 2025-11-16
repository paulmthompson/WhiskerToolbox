# Entity Relationship Manager

## Overview

The `EntityRelationshipManager` provides a mechanism to store and query sparse relationships between entities in the WhiskerToolbox data management system. This is particularly useful for tracking derived data relationships, parent-child hierarchies, and linked entities.

## Use Cases

### 1. Mask Area Calculation
When processing mask time series data to calculate the area of each mask over time, you can establish a relationship between the original mask entities and the calculated area values:

```cpp
EntityRelationshipManager relationships;

// After calculating area from mask data
EntityId mask_entity = EntityId(100);  // Original mask at time t
EntityId area_entity = EntityId(200);  // Calculated area value at time t

// Create a parent-child relationship
relationships.addRelationship(mask_entity, area_entity, 
                             RelationshipType::ParentChild, 
                             "Mask to Area");
```

### 2. Data Processing Pipeline
Track relationships through a multi-stage processing pipeline:

```cpp
// Raw data -> Filtered data -> Derived features
EntityId raw_data = EntityId(100);
EntityId filtered_data = EntityId(200);
EntityId features = EntityId(300);

relationships.addRelationship(raw_data, filtered_data, 
                             RelationshipType::Derived, 
                             "Low-pass filter");
relationships.addRelationship(filtered_data, features, 
                             RelationshipType::Derived, 
                             "Feature extraction");

// Query the processing chain
auto children = relationships.getChildren(raw_data);  // Returns filtered_data
auto grandchildren = relationships.getChildren(filtered_data);  // Returns features

// Navigate backwards
auto parents = relationships.getParents(features);  // Returns filtered_data
```

### 3. Time Series Analysis
Link entities across different time representations:

```cpp
// Link event entities to their corresponding interval entities
EntityId event_start = EntityId(100);
EntityId event_end = EntityId(101);
EntityId interval = EntityId(200);

relationships.addRelationship(event_start, interval, 
                             RelationshipType::Linked, 
                             "Interval start");
relationships.addRelationship(event_end, interval, 
                             RelationshipType::Linked, 
                             "Interval end");

// Query all related entities
auto related = relationships.getReverseRelatedEntities(interval);
// Returns both event_start and event_end
```

## API Reference

### Adding Relationships

```cpp
bool addRelationship(EntityId from_entity, 
                    EntityId to_entity, 
                    RelationshipType type,
                    std::string const & label = "");
```

Adds a directed relationship from one entity to another. Returns `true` if successful, `false` if the relationship already exists.

**Relationship Types:**
- `RelationshipType::ParentChild` - Parent-child hierarchy (e.g., mask → area)
- `RelationshipType::Derived` - Derived/processed data (e.g., raw → filtered)
- `RelationshipType::Linked` - General linkage (e.g., correlated entities)

### Querying Relationships

```cpp
// Get all children (entities this entity points to)
std::vector<EntityId> getChildren(EntityId entity_id) const;

// Get all parents (entities that point to this entity)
std::vector<EntityId> getParents(EntityId entity_id) const;

// Get all related entities (forward direction)
std::vector<EntityId> getRelatedEntities(
    EntityId entity_id, 
    std::optional<RelationshipType> type = std::nullopt) const;

// Get all reverse related entities (backward direction)
std::vector<EntityId> getReverseRelatedEntities(
    EntityId entity_id,
    std::optional<RelationshipType> type = std::nullopt) const;
```

### Detailed Information

```cpp
// Get complete relationship information including labels
std::vector<EntityRelationship> getRelationshipDetails(
    EntityId entity_id,
    bool include_reverse = false) const;
```

Returns a vector of `EntityRelationship` structures containing:
- `from_entity` - Source entity ID
- `to_entity` - Target entity ID  
- `type` - Relationship type
- `label` - Optional descriptive label

### Removing Relationships

```cpp
// Remove a specific relationship
bool removeRelationship(EntityId from_entity, 
                       EntityId to_entity, 
                       RelationshipType type);

// Remove all relationships involving an entity
std::size_t removeAllRelationships(EntityId entity_id);

// Clear all relationships (session reset)
void clear();
```

## Performance Characteristics

- **Add relationship**: O(1) average case
- **Query relationships**: O(k) where k is the number of relationships for an entity
- **Remove relationship**: O(1) average case
- **Memory**: O(n) where n is the total number of relationships

The data structure uses bidirectional hash maps to enable efficient lookups in both forward and reverse directions, making it suitable for sparse relationship graphs with hundreds of thousands of entities.

## Integration Example

Here's a complete example showing how to track mask processing:

```cpp
#include "Entity/EntityRelationshipManager.hpp"

void processMaskTimeSeries(/* ... */) {
    EntityRelationshipManager relationships;
    
    // Process each time point
    for (size_t t = 0; t < num_timepoints; ++t) {
        EntityId mask_entity = getMaskEntityAtTime(t);
        
        // Calculate area
        double area = calculateMaskArea(mask_entity);
        EntityId area_entity = createAreaEntity(area, t);
        
        // Establish relationship
        relationships.addRelationship(
            mask_entity, 
            area_entity,
            RelationshipType::ParentChild,
            "mask_area"
        );
    }
    
    // Later, when user selects a mask entity in the UI
    EntityId selected_mask = getSelectedEntity();
    
    // Find the corresponding area values
    auto children = relationships.getChildren(selected_mask);
    if (!children.empty()) {
        EntityId area_entity = children[0];
        displayAreaValue(area_entity);
    }
    
    // Or navigate from area back to mask
    EntityId selected_area = getSelectedAreaEntity();
    auto parents = relationships.getParents(selected_area);
    if (!parents.empty()) {
        EntityId mask_entity = parents[0];
        displayMask(mask_entity);
    }
}
```

## Design Patterns

The `EntityRelationshipManager` implements several design patterns observed in the codebase:

1. **Bidirectional Lookup** - Similar to `EntityGroupManager`, it maintains both forward and reverse indices for O(1) lookups in both directions.

2. **Sparse Storage** - Only stores relationships that exist, making it efficient for scenarios where only a subset of entities have relationships.

3. **Type Safety** - Uses strong typing for entity IDs and relationship types to prevent errors.

4. **Session Lifecycle** - Provides a `clear()` method for session resets, matching the pattern of other managers.

## Thread Safety

The current implementation is **not thread-safe**. If concurrent access is required, external synchronization should be used. For thread-safe access, consider wrapping operations in a mutex similar to `EntityRegistry`.
