#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>
#include <algorithm>

/**
 * @brief Integration test fixture for EntityGroupManager with DataManager and LineData
 * 
 * This fixture tests the complete workflow of:
 * 1. Creating LineData in DataManager
 * 2. Retrieving EntityIds from LineData
 * 3. Creating groups in EntityGroupManager
 * 4. Adding EntityIds to groups
 * 5. Verifying group contents and reverse lookups
 */
class EntityGroupManagerIntegrationFixture {
protected:
    EntityGroupManagerIntegrationFixture() {
        // Initialize DataManager
        data_manager = std::make_unique<DataManager>();
        
        // Create and populate LineData with test data
        setupLineData();
    }
    
    void setupLineData() {
        // Create LineData
        auto line_data = std::make_shared<LineData>();
        
        // Create test lines at different time frames
        std::vector<Point2D<float>> line1_t1 = {
            {10.0f, 15.0f},
            {20.0f, 25.0f},
            {30.0f, 35.0f}
        };
        
        std::vector<Point2D<float>> line2_t1 = {
            {40.0f, 45.0f},
            {50.0f, 55.0f},
            {60.0f, 65.0f}
        };
        
        std::vector<Point2D<float>> line1_t2 = {
            {70.0f, 75.0f},
            {80.0f, 85.0f},
            {90.0f, 95.0f},
            {100.0f, 105.0f}
        };
        
        std::vector<Point2D<float>> line2_t2 = {
            {110.0f, 115.0f},
            {120.0f, 125.0f}
        };
        
        std::vector<Point2D<float>> line1_t3 = {
            {130.0f, 135.0f},
            {140.0f, 145.0f},
            {150.0f, 155.0f}
        };
        
        // Add lines to LineData (this will NOT generate EntityIds yet since context isn't set)
        line_data->addAtTime(TimeFrameIndex(10), line1_t1);
        line_data->addAtTime(TimeFrameIndex(10), line2_t1);
        line_data->addAtTime(TimeFrameIndex(20), line1_t2);
        line_data->addAtTime(TimeFrameIndex(20), line2_t2);
        line_data->addAtTime(TimeFrameIndex(30), line1_t3);
        
        // Set image size
        line_data->setImageSize({800, 600});
        
        // Add to DataManager
        data_manager->setData<LineData>("test_lines", line_data, TimeKey("time"));
        
        // Debug: Check if EntityRegistry is available
        auto* entity_registry = data_manager->getEntityRegistry();
        REQUIRE(entity_registry != nullptr);
        
        // Debug: Test EntityRegistry directly
        EntityId test_id = entity_registry->ensureId("test", EntityKind::LineEntity, TimeFrameIndex(10), 0);
        INFO("Direct EntityRegistry test ID: " << test_id);
        REQUIRE(test_id != 0);
        
        // NOW set up identity context and rebuild EntityIds
        line_data->setIdentityContext("test_lines", entity_registry);
        
        // Debug: Check what's in the entity_ids_by_time before rebuild
        auto ids_t10_before = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_t20_before = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto ids_t30_before = line_data->getEntityIdsAtTime(TimeFrameIndex(30));
        
        INFO("EntityIds at time 10 before rebuild: " << ids_t10_before.size());
        INFO("EntityIds at time 20 before rebuild: " << ids_t20_before.size());
        INFO("EntityIds at time 30 before rebuild: " << ids_t30_before.size());
        
        for (size_t i = 0; i < ids_t10_before.size(); ++i) {
            INFO("T10[" << i << "] = " << ids_t10_before[i]);
        }
        
        // Debug: Verify identity context was set correctly
        auto before_rebuild = line_data->getAllEntityIds();
        INFO("EntityIds before rebuild: " << before_rebuild.size());
        REQUIRE(before_rebuild.size() == 5); // Should have placeholder 0s
        
        line_data->rebuildAllEntityIds();
        
        // Debug: Check if EntityIds were actually generated
        auto debug_ids = line_data->getAllEntityIds();
        INFO("Total EntityIds after rebuild: " << debug_ids.size());
        for (size_t i = 0; i < debug_ids.size(); ++i) {
            INFO("EntityId[" << i << "] = " << debug_ids[i]);
        }
        
        // Store reference for tests
        this->line_data = line_data;
    }
    
    std::unique_ptr<DataManager> data_manager;
    std::shared_ptr<LineData> line_data;
};

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture, 
                 "EntityGroupManager Integration - Basic Group Operations", 
                 "[integration][entity][group][datamanager][linedata]") {
    
    SECTION("DataManager has EntityGroupManager accessible") {
        REQUIRE(data_manager->getEntityGroupManager() != nullptr);
        
        auto* group_manager = data_manager->getEntityGroupManager();
        REQUIRE(group_manager->getGroupCount() == 0);
        REQUIRE(group_manager->getTotalEntityCount() == 0);
    }
    
    SECTION("LineData generates EntityIds when added to DataManager") {
        // Verify LineData has EntityIds at each time frame
        auto entity_ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto entity_ids_t30 = line_data->getEntityIdsAtTime(TimeFrameIndex(30));
        
        REQUIRE(entity_ids_t10.size() == 2);  // 2 lines at time 10
        REQUIRE(entity_ids_t20.size() == 2);  // 2 lines at time 20
        REQUIRE(entity_ids_t30.size() == 1);  // 1 line at time 30
        
        // Verify all EntityIds are unique and non-zero
        auto all_entity_ids = line_data->getAllEntityIds();
        REQUIRE(all_entity_ids.size() == 5);  // Total 5 lines
        
        // Debug output to see what EntityIds we're getting
        for (size_t i = 0; i < all_entity_ids.size(); ++i) {
            INFO("EntityId[" << i << "] = " << all_entity_ids[i]);
        }
        
        for (EntityId id : all_entity_ids) {
            REQUIRE(id != 0);  // Should be valid EntityIds
        }
        
        // Verify no duplicate EntityIds
        std::sort(all_entity_ids.begin(), all_entity_ids.end());
        auto unique_end = std::unique(all_entity_ids.begin(), all_entity_ids.end());
        REQUIRE(unique_end == all_entity_ids.end());  // No duplicates
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - Group Creation and Management",
                 "[integration][entity][group][datamanager][linedata]") {
    
    auto* group_manager = data_manager->getEntityGroupManager();
    
    SECTION("Create groups and add LineData EntityIds") {
        // Get EntityIds from different time frames
        auto entity_ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto entity_ids_t30 = line_data->getEntityIdsAtTime(TimeFrameIndex(30));
        
        // Create groups
        GroupId early_group = group_manager->createGroup("Early Lines", "Lines from time 10");
        GroupId middle_group = group_manager->createGroup("Middle Lines", "Lines from time 20");
        GroupId mixed_group = group_manager->createGroup("Mixed Selection", "Mixed lines from different times");
        
        // Add EntityIds to groups
        size_t added_early = group_manager->addEntitiesToGroup(early_group, entity_ids_t10);
        size_t added_middle = group_manager->addEntitiesToGroup(middle_group, entity_ids_t20);
        
        REQUIRE(added_early == 2);
        REQUIRE(added_middle == 2);
        
        // Add mixed selection (one from each time frame)
        std::vector<EntityId> mixed_entities = {
            entity_ids_t10[0],  // First line from time 10
            entity_ids_t20[1],  // Second line from time 20
            entity_ids_t30[0]   // Only line from time 30
        };
        size_t added_mixed = group_manager->addEntitiesToGroup(mixed_group, mixed_entities);
        REQUIRE(added_mixed == 3);
        
        // Verify group sizes
        REQUIRE(group_manager->getGroupSize(early_group) == 2);
        REQUIRE(group_manager->getGroupSize(middle_group) == 2);
        REQUIRE(group_manager->getGroupSize(mixed_group) == 3);
        REQUIRE(group_manager->getGroupCount() == 3);
        
        // Verify group contents
        auto early_entities = group_manager->getEntitiesInGroup(early_group);
        auto middle_entities = group_manager->getEntitiesInGroup(middle_group);
        auto mixed_retrieved = group_manager->getEntitiesInGroup(mixed_group);
        
        std::sort(early_entities.begin(), early_entities.end());
        std::sort(entity_ids_t10.begin(), entity_ids_t10.end());
        REQUIRE(early_entities == entity_ids_t10);
        
        std::sort(middle_entities.begin(), middle_entities.end());
        std::sort(entity_ids_t20.begin(), entity_ids_t20.end());
        REQUIRE(middle_entities == entity_ids_t20);
        
        std::sort(mixed_retrieved.begin(), mixed_retrieved.end());
        std::sort(mixed_entities.begin(), mixed_entities.end());
        REQUIRE(mixed_retrieved == mixed_entities);
    }
    
    SECTION("Verify reverse lookups work correctly") {
        auto entity_ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        
        GroupId group1 = group_manager->createGroup("Group 1");
        GroupId group2 = group_manager->createGroup("Group 2");
        
        // Add first entity to both groups
        group_manager->addEntityToGroup(group1, entity_ids_t10[0]);
        group_manager->addEntityToGroup(group2, entity_ids_t10[0]);
        
        // Add second entity to only group1
        group_manager->addEntityToGroup(group1, entity_ids_t10[1]);
        
        // Verify reverse lookups
        auto groups_for_first_entity = group_manager->getGroupsContainingEntity(entity_ids_t10[0]);
        auto groups_for_second_entity = group_manager->getGroupsContainingEntity(entity_ids_t10[1]);
        
        REQUIRE(groups_for_first_entity.size() == 2);
        REQUIRE(std::find(groups_for_first_entity.begin(), groups_for_first_entity.end(), group1) != groups_for_first_entity.end());
        REQUIRE(std::find(groups_for_first_entity.begin(), groups_for_first_entity.end(), group2) != groups_for_first_entity.end());
        
        REQUIRE(groups_for_second_entity.size() == 1);
        REQUIRE(groups_for_second_entity[0] == group1);
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - LineData Entity Lookup",
                 "[integration][entity][group][datamanager][linedata][lookup]") {
    
    auto* group_manager = data_manager->getEntityGroupManager();
    
    SECTION("Retrieve original LineData from EntityIds via groups") {
        // Get EntityIds from time frame 10
        auto entity_ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_t10.size() == 2);
        
        // Create a group and add these entities
        GroupId test_group = group_manager->createGroup("Test Group", "For lookup testing");
        group_manager->addEntitiesToGroup(test_group, entity_ids_t10);
        
        // Retrieve EntityIds from the group
        auto group_entities = group_manager->getEntitiesInGroup(test_group);
        REQUIRE(group_entities.size() == 2);
        
        // Use LineData lookup methods to get original data
        auto lines_with_ids = line_data->getLinesByEntityIds(group_entities);
        auto time_infos = line_data->getTimeInfoByEntityIds(group_entities);
        
        REQUIRE(lines_with_ids.size() == 2);
        REQUIRE(time_infos.size() == 2);
        
        // Verify the lookup results
        for (auto const& [entity_id, line] : lines_with_ids) {
            REQUIRE(line.size() == 3);  // All test lines have 3 points
            
            // Verify this EntityId was in our original list
            REQUIRE(std::find(entity_ids_t10.begin(), entity_ids_t10.end(), entity_id) != entity_ids_t10.end());
        }
        
        for (auto const& [entity_id, time, local_index] : time_infos) {
            REQUIRE(time == TimeFrameIndex(10));  // All should be from time 10
            REQUIRE(local_index >= 0);
            REQUIRE(local_index < 2);  // Should be 0 or 1 (two lines at this time)
            
            // Verify this EntityId was in our original list
            REQUIRE(std::find(entity_ids_t10.begin(), entity_ids_t10.end(), entity_id) != entity_ids_t10.end());
        }
    }
    
    SECTION("Complete workflow: Group -> EntityIds -> LineData") {
        // This demonstrates the full workflow for visualization linking
        
        // Step 1: Create a mixed group representing user selection
        auto entity_ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        
        std::vector<EntityId> user_selection = {
            entity_ids_t10[0],  // First line from time 10
            entity_ids_t20[1],  // Second line from time 20
        };
        
        GroupId selection_group = group_manager->createGroup("User Selection", "Selected from scatter plot");
        group_manager->addEntitiesToGroup(selection_group, user_selection);
        
        // Step 2: Later, retrieve selection from group (simulating UI callback)
        auto selected_entities = group_manager->getEntitiesInGroup(selection_group);
        REQUIRE(selected_entities.size() == 2);
        
        // Step 3: Get original line data for visualization
        auto selected_lines = line_data->getLinesByEntityIds(selected_entities);
        auto selected_time_info = line_data->getTimeInfoByEntityIds(selected_entities);
        
        REQUIRE(selected_lines.size() == 2);
        REQUIRE(selected_time_info.size() == 2);
        
        // Step 4: Verify we can identify the temporal spread
        std::vector<TimeFrameIndex> times_found;
        for (auto const& [entity_id, time, local_index] : selected_time_info) {
            times_found.push_back(time);
        }
        
        std::sort(times_found.begin(), times_found.end());
        REQUIRE(times_found[0] == TimeFrameIndex(10));
        REQUIRE(times_found[1] == TimeFrameIndex(20));
        
        // Step 5: Verify line data integrity
        for (auto const& [entity_id, line] : selected_lines) {
            REQUIRE_FALSE(line.empty());
            REQUIRE(line.size() >= 2);  // All our test lines have at least 2 points
            
            // Verify the line data is geometrically valid
            for (auto const& point : line) {
                REQUIRE(point.x >= 0.0f);
                REQUIRE(point.y >= 0.0f);
            }
        }
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - DataManager Reset Behavior",
                 "[integration][entity][group][datamanager][reset]") {
    
    auto* group_manager = data_manager->getEntityGroupManager();
    
    SECTION("Groups persist until DataManager reset") {
        // Create groups and add entities
        auto entity_ids = line_data->getAllEntityIds();
        REQUIRE_FALSE(entity_ids.empty());
        
        GroupId test_group = group_manager->createGroup("Test Group");
        group_manager->addEntitiesToGroup(test_group, entity_ids);
        
        REQUIRE(group_manager->getGroupCount() == 1);
        REQUIRE(group_manager->getGroupSize(test_group) == entity_ids.size());
        
        // Reset DataManager
        data_manager->reset();
        
        // Verify groups are cleared
        REQUIRE(group_manager->getGroupCount() == 0);
        REQUIRE(group_manager->getTotalEntityCount() == 0);
        REQUIRE_FALSE(group_manager->hasGroup(test_group));
    }
}
