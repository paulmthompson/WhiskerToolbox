#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

// TableView and LineSamplingMultiComputer includes
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/TableRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture, 
                 "EntityGroupManager Integration - TableView Round-trip", 
                 "[integration][entity][group][tableview][linesampling]") {
    
    SECTION("Create TableView from LineData, group entities, query back to LineData") {
        // Get required components
        auto* group_manager = data_manager->getEntityGroupManager();
        REQUIRE(group_manager != nullptr);
        
        // Create TimeFrame for the table
        std::vector<int> timeValues = {10, 20, 30};
        auto table_timeframe = std::make_shared<TimeFrame>(timeValues);
        
        // Create DataManagerExtension for TableView integration
        auto dme = std::make_shared<DataManagerExtension>(*data_manager);
        
        // Create LineDataAdapter from our test data
        auto line_adapter = std::make_shared<LineDataAdapter>(line_data, table_timeframe, "test_lines");
        
        // Create LineSamplingMultiComputer with 2 segments (3 sample points: 0.0, 0.5, 1.0)
        auto multi_computer = std::make_unique<LineSamplingMultiComputer>(
            std::static_pointer_cast<ILineSource>(line_adapter),
            "test_lines",
            table_timeframe,
            2  // 2 segments = 3 sample points
        );
        
        // Create row selector for our time frames
        std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto row_selector = std::make_unique<TimestampSelector>(timestamps, table_timeframe);
        
        // Build TableView using TableViewBuilder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));
        builder.addColumns<double>("Line", std::move(multi_computer));
        
        auto table = builder.build();
        
        // Verify table structure
        REQUIRE(table.getRowCount() == 5);  // t10:2 + t20:2 + t30:1 = 5 rows (entity expansion)
        REQUIRE(table.getColumnCount() == 6);  // 3 sample points * 2 coordinates = 6 columns
        
        // Verify column names
        REQUIRE(table.hasColumn("Line.x@0.000"));
        REQUIRE(table.hasColumn("Line.y@0.000"));
        REQUIRE(table.hasColumn("Line.x@0.500"));
        REQUIRE(table.hasColumn("Line.y@0.500"));
        REQUIRE(table.hasColumn("Line.x@1.000"));
        REQUIRE(table.hasColumn("Line.y@1.000"));
        
        // Add table to TableRegistry
        auto* table_registry = data_manager->getTableRegistry();
        std::string table_key = "line_sampling_test_table";
        
        // First create table entry in registry
        bool created = table_registry->createTable(table_key, "Line Sampling Test Table", "Test table for EntityGroupManager integration");
        REQUIRE(created);
        
        // Then store the built table
        bool stored = table_registry->storeBuiltTable(table_key, std::make_unique<TableView>(std::move(table)));
        REQUIRE(stored);
        
        // Get the table back from registry
        auto stored_table = table_registry->getBuiltTable(table_key);
        REQUIRE(stored_table != nullptr);
        
        // Get sample data from specific rows
        auto x_start = stored_table->getColumnValues<double>("Line.x@0.000");
        auto y_start = stored_table->getColumnValues<double>("Line.y@0.000");
        auto x_mid = stored_table->getColumnValues<double>("Line.x@0.500");
        auto y_mid = stored_table->getColumnValues<double>("Line.y@0.500");
        auto x_end = stored_table->getColumnValues<double>("Line.x@1.000");
        auto y_end = stored_table->getColumnValues<double>("Line.y@1.000");
        
        REQUIRE(x_start.size() == 5);
        REQUIRE(y_start.size() == 5);
        REQUIRE(x_mid.size() == 5);
        REQUIRE(y_mid.size() == 5);
        REQUIRE(x_end.size() == 5);
        REQUIRE(y_end.size() == 5);
        
        // Now we need to get EntityIDs for specific table rows
        // This requires TableView to expose EntityIDs - let's check if this functionality exists
        
        // Try to get EntityIDs from the table (this might not be implemented yet)
        // We'll select rows 1, 2, and 4 for our group
        std::vector<size_t> selected_row_indices = {1, 2, 4};
        std::vector<EntityId> entity_ids_from_table;
        
        // This is the functionality that might be missing - getting EntityIDs from TableView rows
        // For now, let's try to get them through the LineData directly using time/index information
        
        // Get EntityIDs from LineData that correspond to our table rows
        // Row 0: t=10, entity 0 -> EntityIDs at TimeFrameIndex(10)[0]
        // Row 1: t=10, entity 1 -> EntityIDs at TimeFrameIndex(10)[1]  
        // Row 2: t=20, entity 0 -> EntityIDs at TimeFrameIndex(20)[0]
        // Row 3: t=20, entity 1 -> EntityIDs at TimeFrameIndex(20)[1]
        // Row 4: t=30, entity 0 -> EntityIDs at TimeFrameIndex(30)[0]
        
        auto ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto ids_t30 = line_data->getEntityIdsAtTime(TimeFrameIndex(30));
        
        REQUIRE(ids_t10.size() == 2);
        REQUIRE(ids_t20.size() == 2);
        REQUIRE(ids_t30.size() == 1);
        
        // Select EntityIDs for rows 1, 2, 4 (0-indexed)
        // Row 1 = t10[1], Row 2 = t20[0], Row 4 = t30[0]
        std::vector<EntityId> selected_entity_ids = {
            ids_t10[1],  // Row 1
            ids_t20[0],  // Row 2  
            ids_t30[0]   // Row 4
        };
        
        // Verify all EntityIDs are valid
        for (EntityId id : selected_entity_ids) {
            REQUIRE(id != 0);
            INFO("Selected EntityID: " << id);
        }
        
        // Create a group in EntityGroupManager with these EntityIDs
        GroupId test_group = group_manager->createGroup("TableView Selection", "Entities from selected table rows");
        size_t added = group_manager->addEntitiesToGroup(test_group, selected_entity_ids);
        REQUIRE(added == selected_entity_ids.size());
        
        // Verify the group was created correctly
        REQUIRE(group_manager->hasGroup(test_group));
        REQUIRE(group_manager->getGroupSize(test_group) == selected_entity_ids.size());
        
        auto group_entities = group_manager->getEntitiesInGroup(test_group);
        REQUIRE(group_entities.size() == selected_entity_ids.size());
        
        // Now query LineData using the grouped EntityIDs to get the original line data
        auto lines_from_group = line_data->getLinesByEntityIds(group_entities);
        REQUIRE(lines_from_group.size() == selected_entity_ids.size());
        
        // Verify that the lines we get back match the data in the corresponding table rows
        // We'll compare the start and end points from LineSamplingMultiComputer with actual line data
        
        for (size_t i = 0; i < lines_from_group.size(); ++i) {
            EntityId entity_id = lines_from_group[i].first;
            Line2D const& original_line = lines_from_group[i].second;
            
            // Find which row this EntityID corresponds to
            size_t table_row_index = 0;
            if (entity_id == ids_t10[1]) {
                table_row_index = 1;  // Row 1: t=10, entity 1
            } else if (entity_id == ids_t20[0]) {
                table_row_index = 2;  // Row 2: t=20, entity 0
            } else if (entity_id == ids_t30[0]) {
                table_row_index = 4;  // Row 4: t=30, entity 0
            } else {
                FAIL("Unexpected EntityID in group: " << entity_id);
            }
            
            // Get the sampled points from the table for this row
            float table_x_start = static_cast<float>(x_start[table_row_index]);
            float table_y_start = static_cast<float>(y_start[table_row_index]);
            float table_x_end = static_cast<float>(x_end[table_row_index]);
            float table_y_end = static_cast<float>(y_end[table_row_index]);
            
            // Get actual start and end points from the original line
            REQUIRE(original_line.size() >= 2);
            Point2D<float> actual_start = original_line.front();
            Point2D<float> actual_end = original_line.back();
            
            // Verify that the table data matches the original line data
            INFO("Checking EntityID " << entity_id << " at table row " << table_row_index);
            INFO("Table start: (" << table_x_start << ", " << table_y_start << ")");
            INFO("Actual start: (" << actual_start.x << ", " << actual_start.y << ")");
            INFO("Table end: (" << table_x_end << ", " << table_y_end << ")");
            INFO("Actual end: (" << actual_end.x << ", " << actual_end.y << ")");
            
            REQUIRE(table_x_start == Catch::Approx(actual_start.x).epsilon(0.001f));
            REQUIRE(table_y_start == Catch::Approx(actual_start.y).epsilon(0.001f));
            REQUIRE(table_x_end == Catch::Approx(actual_end.x).epsilon(0.001f));
            REQUIRE(table_y_end == Catch::Approx(actual_end.y).epsilon(0.001f));
        }
        
        INFO("Successfully verified round-trip: LineData -> TableView -> EntityGroupManager -> LineData");
    }
}
