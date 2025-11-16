#include "Entity/EntityRelationshipManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"

/*
 * Example: Integrating EntityRelationshipManager with existing Entity system
 * 
 * This example demonstrates how to use EntityRelationshipManager alongside
 * EntityGroupManager and EntityRegistry to track relationships between entities.
 */

// Example scenario: Processing mask data to calculate areas
void example_mask_area_processing() {
    // Central entity registry (session-scoped)
    EntityRegistry registry;
    
    // Group manager for organizing entities
    EntityGroupManager groups;
    
    // Relationship manager for tracking parent-child relationships
    EntityRelationshipManager relationships;
    
    // Create groups for raw masks and calculated areas
    GroupId mask_group = groups.createGroup("Masks", "Original mask time series");
    GroupId area_group = groups.createGroup("Areas", "Calculated mask areas");
    
    // Simulate processing masks at different time points
    for (int time_idx = 0; time_idx < 10; ++time_idx) {
        // Get entity ID for mask at this time point
        EntityId mask_entity = registry.ensureId(
            "mask_data",                    // data key
            EntityKind::MaskEntity,         // entity kind
            TimeFrameIndex(time_idx),       // time index
            0                               // local index within time
        );
        
        // Add mask entity to mask group
        groups.addEntityToGroup(mask_group, mask_entity);
        
        // Calculate area (simulated)
        // In real code: double area = calculateMaskArea(mask_entity);
        
        // Create entity ID for the calculated area
        EntityId area_entity = registry.ensureId(
            "mask_area_data",               // different data key
            EntityKind::EventEntity,        // areas are stored as events
            TimeFrameIndex(time_idx),       // same time index
            0                               // local index
        );
        
        // Add area entity to area group
        groups.addEntityToGroup(area_group, area_entity);
        
        // Establish parent-child relationship: mask -> area
        relationships.addRelationship(
            mask_entity,
            area_entity,
            RelationshipType::ParentChild,
            "Mask area calculation"
        );
    }
    
    // User interaction: User selects a mask entity in the UI
    EntityId selected_mask = registry.ensureId(
        "mask_data",
        EntityKind::MaskEntity,
        TimeFrameIndex(5),  // Time point 5
        0
    );
    
    // Find the corresponding area value
    auto children = relationships.getChildren(selected_mask);
    if (!children.empty()) {
        EntityId area_entity = children[0];
        
        // Display or highlight the area value
        // In real code: displayAreaValue(area_entity);
        
        // Verify the area entity is in the correct group
        bool in_area_group = groups.isEntityInGroup(area_group, area_entity);
        // in_area_group should be true
    }
    
    // Reverse navigation: User selects an area entity
    EntityId selected_area = registry.ensureId(
        "mask_area_data",
        EntityKind::EventEntity,
        TimeFrameIndex(7),
        0
    );
    
    // Find the parent mask
    auto parents = relationships.getParents(selected_area);
    if (!parents.empty()) {
        EntityId mask_entity = parents[0];
        
        // Display or highlight the corresponding mask
        // In real code: displayMask(mask_entity);
    }
    
    // Get detailed information about relationships for an entity
    auto details = relationships.getRelationshipDetails(selected_mask);
    for (auto const & rel : details) {
        // rel.from_entity = selected_mask
        // rel.to_entity = area_entity
        // rel.type = RelationshipType::ParentChild
        // rel.label = "Mask area calculation"
    }
    
    // Statistics
    // Total number of relationships
    size_t rel_count = relationships.getRelationshipCount();  // 10
    
    // Number of unique entities with relationships
    size_t entity_count = relationships.getEntityCount();     // 20 (10 masks + 10 areas)
    
    // Number of entities in each group
    size_t mask_count = groups.getGroupSize(mask_group);      // 10
    size_t area_count = groups.getGroupSize(area_group);      // 10
}

// Example: Multi-stage processing pipeline
void example_processing_pipeline() {
    EntityRegistry registry;
    EntityRelationshipManager relationships;
    
    // Stage 1: Raw data
    EntityId raw_data = registry.ensureId(
        "raw_analog",
        EntityKind::EventEntity,
        TimeFrameIndex(0),
        0
    );
    
    // Stage 2: Filtered data
    EntityId filtered_data = registry.ensureId(
        "filtered_analog",
        EntityKind::EventEntity,
        TimeFrameIndex(0),
        0
    );
    
    relationships.addRelationship(
        raw_data,
        filtered_data,
        RelationshipType::Derived,
        "Low-pass filter (cutoff=10Hz)"
    );
    
    // Stage 3: Feature extraction
    EntityId features = registry.ensureId(
        "features",
        EntityKind::EventEntity,
        TimeFrameIndex(0),
        0
    );
    
    relationships.addRelationship(
        filtered_data,
        features,
        RelationshipType::Derived,
        "Peak detection"
    );
    
    // Navigate the processing chain
    auto stage2_outputs = relationships.getChildren(raw_data);
    // Returns: [filtered_data]
    
    auto stage3_outputs = relationships.getChildren(filtered_data);
    // Returns: [features]
    
    // Trace back to original data
    auto stage2_inputs = relationships.getParents(filtered_data);
    // Returns: [raw_data]
    
    auto stage1_inputs = relationships.getParents(raw_data);
    // Returns: [] (no parents)
    
    // Get all relationships for filtered_data
    auto all_rels = relationships.getRelationshipDetails(filtered_data, true);
    // Returns: [
    //   {raw_data -> filtered_data, "Low-pass filter (cutoff=10Hz)"},
    //   {filtered_data -> features, "Peak detection"}
    // ]
}

// Example: Cleanup and session reset
void example_cleanup() {
    EntityRelationshipManager relationships;
    
    // ... add relationships during session ...
    
    // Remove specific relationship
    EntityId entity1 = EntityId(100);
    EntityId entity2 = EntityId(200);
    relationships.removeRelationship(entity1, entity2, RelationshipType::ParentChild);
    
    // Remove all relationships involving an entity
    relationships.removeAllRelationships(entity1);
    
    // Clear all relationships for new session
    relationships.clear();
}
