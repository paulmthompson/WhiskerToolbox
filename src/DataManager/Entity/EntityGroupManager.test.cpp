#include "Entity/EntityGroupManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <unordered_set>

TEST_CASE("EntityGroupManager - Group Management", "[entity][group][manager][crud]") {
    EntityGroupManager manager;

    SECTION("Create and retrieve groups") {
        GroupId group1 = manager.createGroup("Test Group 1", "First test group");
        GroupId group2 = manager.createGroup("Test Group 2", "Second test group");
        
        REQUIRE(group1 != group2);
        REQUIRE(group1 > 0);
        REQUIRE(group2 > 0);
        
        REQUIRE(manager.hasGroup(group1));
        REQUIRE(manager.hasGroup(group2));
        REQUIRE_FALSE(manager.hasGroup(999));
        
        auto desc1 = manager.getGroupDescriptor(group1);
        REQUIRE(desc1.has_value());
        REQUIRE(desc1->id == group1);
        REQUIRE(desc1->name == "Test Group 1");
        REQUIRE(desc1->description == "First test group");
        REQUIRE(desc1->entity_count == 0);
        
        auto desc2 = manager.getGroupDescriptor(group2);
        REQUIRE(desc2.has_value());
        REQUIRE(desc2->name == "Test Group 2");
        REQUIRE(desc2->description == "Second test group");
        
        auto invalid_desc = manager.getGroupDescriptor(999);
        REQUIRE_FALSE(invalid_desc.has_value());
    }
    
    SECTION("Update group metadata") {
        GroupId group = manager.createGroup("Original Name", "Original Description");
        
        bool updated = manager.updateGroup(group, "Updated Name", "Updated Description");
        REQUIRE(updated);
        
        auto desc = manager.getGroupDescriptor(group);
        REQUIRE(desc.has_value());
        REQUIRE(desc->name == "Updated Name");
        REQUIRE(desc->description == "Updated Description");
        
        // Try updating non-existent group
        bool failed_update = manager.updateGroup(999, "Should Fail");
        REQUIRE_FALSE(failed_update);
    }
    
    SECTION("Delete groups") {
        GroupId group = manager.createGroup("Test Group");
        REQUIRE(manager.hasGroup(group));
        
        bool deleted = manager.deleteGroup(group);
        REQUIRE(deleted);
        REQUIRE_FALSE(manager.hasGroup(group));
        
        // Try deleting non-existent group
        bool failed_delete = manager.deleteGroup(999);
        REQUIRE_FALSE(failed_delete);
    }
    
    SECTION("Get all groups") {
        REQUIRE(manager.getAllGroupIds().empty());
        REQUIRE(manager.getAllGroupDescriptors().empty());
        
        GroupId group1 = manager.createGroup("Group 1", "Description 1");
        GroupId group2 = manager.createGroup("Group 2", "Description 2");
        GroupId group3 = manager.createGroup("Group 3", "Description 3");
        
        auto all_ids = manager.getAllGroupIds();
        REQUIRE(all_ids.size() == 3);
        REQUIRE(std::find(all_ids.begin(), all_ids.end(), group1) != all_ids.end());
        REQUIRE(std::find(all_ids.begin(), all_ids.end(), group2) != all_ids.end());
        REQUIRE(std::find(all_ids.begin(), all_ids.end(), group3) != all_ids.end());
        
        auto all_descriptors = manager.getAllGroupDescriptors();
        REQUIRE(all_descriptors.size() == 3);
        
        // Verify descriptors contain expected data
        std::unordered_set<std::string> names;
        for (auto const & desc : all_descriptors) {
            names.insert(desc.name);
        }
        REQUIRE(names.count("Group 1") == 1);
        REQUIRE(names.count("Group 2") == 1);
        REQUIRE(names.count("Group 3") == 1);
    }
}

TEST_CASE("EntityGroupManager - Entity Management", "[entity][group][manager][entities]") {
    EntityGroupManager manager;
    
    GroupId group1 = manager.createGroup("Group 1");
    GroupId group2 = manager.createGroup("Group 2");
    
    EntityId entity1 = 100;
    EntityId entity2 = 200;
    EntityId entity3 = 300;
    
    SECTION("Add single entities to groups") {
        REQUIRE(manager.addEntityToGroup(group1, entity1));
        REQUIRE(manager.addEntityToGroup(group1, entity2));
        REQUIRE(manager.addEntityToGroup(group2, entity3));
        
        // Try adding duplicate
        REQUIRE_FALSE(manager.addEntityToGroup(group1, entity1));
        
        // Try adding to non-existent group
        REQUIRE_FALSE(manager.addEntityToGroup(999, entity1));
        
        REQUIRE(manager.isEntityInGroup(group1, entity1));
        REQUIRE(manager.isEntityInGroup(group1, entity2));
        REQUIRE_FALSE(manager.isEntityInGroup(group1, entity3));
        REQUIRE(manager.isEntityInGroup(group2, entity3));
        
        REQUIRE(manager.getGroupSize(group1) == 2);
        REQUIRE(manager.getGroupSize(group2) == 1);
        REQUIRE(manager.getGroupSize(999) == 0);
    }
    
    SECTION("Add multiple entities to groups") {
        std::vector<EntityId> entities = {entity1, entity2, entity3};
        
        size_t added = manager.addEntitiesToGroup(group1, entities);
        REQUIRE(added == 3);
        
        // Try adding same entities again (should add 0)
        size_t added_again = manager.addEntitiesToGroup(group1, entities);
        REQUIRE(added_again == 0);
        
        // Try adding to non-existent group
        size_t failed_add = manager.addEntitiesToGroup(999, entities);
        REQUIRE(failed_add == 0);
        
        REQUIRE(manager.getGroupSize(group1) == 3);
    }
    
    SECTION("Remove entities from groups") {
        // Setup
        manager.addEntityToGroup(group1, entity1);
        manager.addEntityToGroup(group1, entity2);
        manager.addEntityToGroup(group1, entity3);
        
        REQUIRE(manager.removeEntityFromGroup(group1, entity2));
        REQUIRE_FALSE(manager.isEntityInGroup(group1, entity2));
        REQUIRE(manager.getGroupSize(group1) == 2);
        
        // Try removing non-existent entity
        REQUIRE_FALSE(manager.removeEntityFromGroup(group1, entity2));
        REQUIRE_FALSE(manager.removeEntityFromGroup(group1, 999));
        
        // Try removing from non-existent group
        REQUIRE_FALSE(manager.removeEntityFromGroup(999, entity1));
    }
    
    SECTION("Remove multiple entities from groups") {
        // Setup
        std::vector<EntityId> entities = {entity1, entity2, entity3};
        manager.addEntitiesToGroup(group1, entities);
        
        std::vector<EntityId> to_remove = {entity1, entity3, 999}; // Include non-existent entity
        size_t removed = manager.removeEntitiesFromGroup(group1, to_remove);
        REQUIRE(removed == 2); // Only entity1 and entity3 should be removed
        
        REQUIRE_FALSE(manager.isEntityInGroup(group1, entity1));
        REQUIRE(manager.isEntityInGroup(group1, entity2));
        REQUIRE_FALSE(manager.isEntityInGroup(group1, entity3));
        REQUIRE(manager.getGroupSize(group1) == 1);
        
        // Try removing from non-existent group
        size_t failed_remove = manager.removeEntitiesFromGroup(999, to_remove);
        REQUIRE(failed_remove == 0);
    }
    
    SECTION("Get entities in group") {
        std::vector<EntityId> entities = {entity1, entity2, entity3};
        manager.addEntitiesToGroup(group1, entities);
        
        auto retrieved = manager.getEntitiesInGroup(group1);
        REQUIRE(retrieved.size() == 3);
        
        std::sort(retrieved.begin(), retrieved.end());
        std::sort(entities.begin(), entities.end());
        REQUIRE(retrieved == entities);
        
        // Empty group
        auto empty = manager.getEntitiesInGroup(group2);
        REQUIRE(empty.empty());
        
        // Non-existent group
        auto invalid = manager.getEntitiesInGroup(999);
        REQUIRE(invalid.empty());
    }
    
    SECTION("Clear group") {
        std::vector<EntityId> entities = {entity1, entity2, entity3};
        manager.addEntitiesToGroup(group1, entities);
        
        REQUIRE(manager.clearGroup(group1));
        REQUIRE(manager.getGroupSize(group1) == 0);
        REQUIRE(manager.hasGroup(group1)); // Group should still exist, just empty
        
        // Try clearing non-existent group
        REQUIRE_FALSE(manager.clearGroup(999));
    }
}

TEST_CASE("EntityGroupManager - Cross-references", "[entity][group][manager][reverse-lookup]") {
    EntityGroupManager manager;
    
    GroupId group1 = manager.createGroup("Group 1");
    GroupId group2 = manager.createGroup("Group 2");
    GroupId group3 = manager.createGroup("Group 3");
    
    EntityId entity1 = 100;
    EntityId entity2 = 200;
    EntityId entity3 = 300;
    
    SECTION("Entity to groups mapping") {
        // Add entity1 to multiple groups
        manager.addEntityToGroup(group1, entity1);
        manager.addEntityToGroup(group2, entity1);
        
        // Add entity2 to one group
        manager.addEntityToGroup(group1, entity2);
        
        // entity3 not in any group
        
        auto groups_for_entity1 = manager.getGroupsContainingEntity(entity1);
        REQUIRE(groups_for_entity1.size() == 2);
        REQUIRE(std::find(groups_for_entity1.begin(), groups_for_entity1.end(), group1) != groups_for_entity1.end());
        REQUIRE(std::find(groups_for_entity1.begin(), groups_for_entity1.end(), group2) != groups_for_entity1.end());
        
        auto groups_for_entity2 = manager.getGroupsContainingEntity(entity2);
        REQUIRE(groups_for_entity2.size() == 1);
        REQUIRE(groups_for_entity2[0] == group1);
        
        auto groups_for_entity3 = manager.getGroupsContainingEntity(entity3);
        REQUIRE(groups_for_entity3.empty());
    }
    
    SECTION("Cross-reference consistency after deletions") {
        // Setup
        manager.addEntityToGroup(group1, entity1);
        manager.addEntityToGroup(group2, entity1);
        manager.addEntityToGroup(group1, entity2);
        
        // Remove entity from one group
        manager.removeEntityFromGroup(group1, entity1);
        
        auto groups_for_entity1 = manager.getGroupsContainingEntity(entity1);
        REQUIRE(groups_for_entity1.size() == 1);
        REQUIRE(groups_for_entity1[0] == group2);
        
        // Delete entire group
        manager.deleteGroup(group2);
        
        groups_for_entity1 = manager.getGroupsContainingEntity(entity1);
        REQUIRE(groups_for_entity1.empty());
        
        // entity2 should still be in group1
        auto groups_for_entity2 = manager.getGroupsContainingEntity(entity2);
        REQUIRE(groups_for_entity2.size() == 1);
        REQUIRE(groups_for_entity2[0] == group1);
    }
}

TEST_CASE("EntityGroupManager - Statistics and Utilities", "[entity][group][manager][stats]") {
    EntityGroupManager manager;
    
    SECTION("Initial state") {
        REQUIRE(manager.getGroupCount() == 0);
        REQUIRE(manager.getTotalEntityCount() == 0);
    }
    
    SECTION("Count after operations") {
        GroupId group1 = manager.createGroup("Group 1");
        GroupId group2 = manager.createGroup("Group 2");
        
        EntityId entity1 = 100;
        EntityId entity2 = 200;
        EntityId entity3 = 300;
        
        REQUIRE(manager.getGroupCount() == 2);
        REQUIRE(manager.getTotalEntityCount() == 0);
        
        manager.addEntityToGroup(group1, entity1);
        manager.addEntityToGroup(group1, entity2);
        manager.addEntityToGroup(group2, entity3);
        
        REQUIRE(manager.getGroupCount() == 2);
        REQUIRE(manager.getTotalEntityCount() == 3);
        
        // Add entity to multiple groups (shouldn't increase total entity count)
        manager.addEntityToGroup(group2, entity1);
        REQUIRE(manager.getTotalEntityCount() == 3);
        
        // Remove entity from one group (shouldn't decrease total if still in another)
        manager.removeEntityFromGroup(group2, entity1);
        REQUIRE(manager.getTotalEntityCount() == 3);
        
        // Remove entity from all groups (should decrease total)
        manager.removeEntityFromGroup(group1, entity1);
        REQUIRE(manager.getTotalEntityCount() == 2);
        
        // Delete group
        manager.deleteGroup(group1);
        REQUIRE(manager.getGroupCount() == 1);
        REQUIRE(manager.getTotalEntityCount() == 1); // Only entity3 in group2
    }
    
    SECTION("Clear all") {
        // Setup some data
        GroupId group1 = manager.createGroup("Group 1");
        GroupId group2 = manager.createGroup("Group 2");
        manager.addEntityToGroup(group1, 100);
        manager.addEntityToGroup(group2, 200);
        
        REQUIRE(manager.getGroupCount() == 2);
        REQUIRE(manager.getTotalEntityCount() == 2);
        
        manager.clear();
        
        REQUIRE(manager.getGroupCount() == 0);
        REQUIRE(manager.getTotalEntityCount() == 0);
        REQUIRE_FALSE(manager.hasGroup(group1));
        REQUIRE_FALSE(manager.hasGroup(group2));
        
        // Should be able to create new groups after clear
        GroupId new_group = manager.createGroup("New Group");
        REQUIRE(manager.hasGroup(new_group));
    }
}

TEST_CASE("EntityGroupManager - Performance Scenarios", "[entity][group][manager][performance]") {
    EntityGroupManager manager;
    
    SECTION("Large batch operations") {
        GroupId group = manager.createGroup("Large Group");
        
        // Create large vector of entity IDs
        std::vector<EntityId> entities;
        entities.reserve(10000);
        for (EntityId i = 1; i <= 10000; ++i) {
            entities.push_back(i);
        }
        
        // Batch add
        size_t added = manager.addEntitiesToGroup(group, entities);
        REQUIRE(added == 10000);
        REQUIRE(manager.getGroupSize(group) == 10000);
        
        // Batch remove half
        std::vector<EntityId> to_remove(entities.begin(), entities.begin() + 5000);
        size_t removed = manager.removeEntitiesFromGroup(group, to_remove);
        REQUIRE(removed == 5000);
        REQUIRE(manager.getGroupSize(group) == 5000);
        
        // Verify remaining entities
        auto remaining = manager.getEntitiesInGroup(group);
        REQUIRE(remaining.size() == 5000);
        
        // All remaining should be >= 5001
        for (EntityId entity : remaining) {
            REQUIRE(entity >= 5001);
        }
    }
    
    SECTION("Multiple groups with overlapping entities") {
        const size_t num_groups = 100;
        const size_t num_entities = 1000;
        
        std::vector<GroupId> groups;
        for (size_t i = 0; i < num_groups; ++i) {
            groups.push_back(manager.createGroup("Group " + std::to_string(i)));
        }
        
        // Add entities to multiple groups (each entity in ~10 groups on average)
        for (EntityId entity = 1; entity <= num_entities; ++entity) {
            for (size_t group_idx = 0; group_idx < num_groups; group_idx += 10) {
                if (group_idx + (entity % 10) < num_groups) {
                    manager.addEntityToGroup(groups[group_idx + (entity % 10)], entity);
                }
            }
        }
        
        REQUIRE(manager.getGroupCount() == num_groups);
        REQUIRE(manager.getTotalEntityCount() == num_entities);
        
        // Verify cross-references work efficiently
        EntityId test_entity = 500;
        auto groups_for_entity = manager.getGroupsContainingEntity(test_entity);
        REQUIRE_FALSE(groups_for_entity.empty());
        REQUIRE(groups_for_entity.size() <= 10); // Should be in ~10 groups
    }
}
