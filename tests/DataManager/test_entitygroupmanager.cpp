#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/adapters/DigitalIntervalDataAdapter.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "utils/TableView/computers/IntervalOverlapComputer.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/transforms/PCATransform.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

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

        // Create a TimeFrame
        auto timeValues = std::vector<int>();
        for (int i = 0; i < 100; ++i) {
            timeValues.push_back(i);
        }
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_time"), timeFrame);

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
                {30.0f, 35.0f}};

        std::vector<Point2D<float>> line2_t1 = {
                {40.0f, 45.0f},
                {50.0f, 55.0f},
                {60.0f, 65.0f}};

        std::vector<Point2D<float>> line1_t2 = {
                {70.0f, 75.0f},
                {80.0f, 85.0f},
                {90.0f, 95.0f},
                {100.0f, 105.0f}};

        std::vector<Point2D<float>> line2_t2 = {
                {110.0f, 115.0f},
                {120.0f, 125.0f}};

        std::vector<Point2D<float>> line1_t3 = {
                {130.0f, 135.0f},
                {140.0f, 145.0f},
                {150.0f, 155.0f}};

        // Add lines to LineData (this will NOT generate EntityIds yet since context isn't set)
        line_data->addAtTime(TimeFrameIndex(10), line1_t1);
        line_data->addAtTime(TimeFrameIndex(10), line2_t1);
        line_data->addAtTime(TimeFrameIndex(20), line1_t2);
        line_data->addAtTime(TimeFrameIndex(20), line2_t2);
        line_data->addAtTime(TimeFrameIndex(30), line1_t3);

        // Set image size
        line_data->setImageSize({800, 600});

        // Add to DataManager
        data_manager->setData<LineData>("test_lines", line_data, TimeKey("test_time"));

        // Debug: Check if EntityRegistry is available
        auto * entity_registry = data_manager->getEntityRegistry();
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
        REQUIRE(before_rebuild.size() == 5);// Should have placeholder 0s

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

        auto * group_manager = data_manager->getEntityGroupManager();
        REQUIRE(group_manager->getGroupCount() == 0);
        REQUIRE(group_manager->getTotalEntityCount() == 0);
    }

    SECTION("LineData generates EntityIds when added to DataManager") {
        // Verify LineData has EntityIds at each time frame
        auto entity_ids_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto entity_ids_t30 = line_data->getEntityIdsAtTime(TimeFrameIndex(30));

        REQUIRE(entity_ids_t10.size() == 2);// 2 lines at time 10
        REQUIRE(entity_ids_t20.size() == 2);// 2 lines at time 20
        REQUIRE(entity_ids_t30.size() == 1);// 1 line at time 30

        // Verify all EntityIds are unique and non-zero
        auto all_entity_ids = line_data->getAllEntityIds();
        REQUIRE(all_entity_ids.size() == 5);// Total 5 lines

        // Debug output to see what EntityIds we're getting
        for (size_t i = 0; i < all_entity_ids.size(); ++i) {
            INFO("EntityId[" << i << "] = " << all_entity_ids[i]);
        }

        for (EntityId id: all_entity_ids) {
            REQUIRE(id != 0);// Should be valid EntityIds
        }

        // Verify no duplicate EntityIds
        std::sort(all_entity_ids.begin(), all_entity_ids.end());
        auto unique_end = std::unique(all_entity_ids.begin(), all_entity_ids.end());
        REQUIRE(unique_end == all_entity_ids.end());// No duplicates
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - Group Creation and Management",
                 "[integration][entity][group][datamanager][linedata]") {

    auto * group_manager = data_manager->getEntityGroupManager();

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
                entity_ids_t10[0],// First line from time 10
                entity_ids_t20[1],// Second line from time 20
                entity_ids_t30[0] // Only line from time 30
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

    auto * group_manager = data_manager->getEntityGroupManager();

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
        auto lines_with_ids = line_data->getDataByEntityIds(group_entities);

        REQUIRE(lines_with_ids.size() == 2);

        // Verify the lookup results
        for (auto const & [entity_id, line]: lines_with_ids) {
            REQUIRE(line.size() == 3);// All test lines have 3 points

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
                entity_ids_t10[0],// First line from time 10
                entity_ids_t20[1],// Second line from time 20
        };

        GroupId selection_group = group_manager->createGroup("User Selection", "Selected from scatter plot");
        group_manager->addEntitiesToGroup(selection_group, user_selection);

        // Step 2: Later, retrieve selection from group (simulating UI callback)
        auto selected_entities = group_manager->getEntitiesInGroup(selection_group);
        REQUIRE(selected_entities.size() == 2);

        // Step 3: Get original line data for visualization
        auto selected_lines = line_data->getDataByEntityIds(selected_entities);

        REQUIRE(selected_lines.size() == 2);

        // Step 4: Verify we can identify the temporal spread
        // Step 5: Verify line data integrity
        for (auto const & [entity_id, line]: selected_lines) {
            REQUIRE_FALSE(line.empty());
            REQUIRE(line.size() >= 2);// All our test lines have at least 2 points

            // Verify the line data is geometrically valid
            for (auto const & point: line) {
                REQUIRE(point.x >= 0.0f);
                REQUIRE(point.y >= 0.0f);
            }
        }
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - DataManager Reset Behavior",
                 "[integration][entity][group][datamanager][reset]") {

    auto * group_manager = data_manager->getEntityGroupManager();

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
                 "EntityGroupManager Integration - MediaWindow-like Entity Group Checking",
                 "[integration][entity][group][mediawindow][linedata]") {

    SECTION("Check if entities at specific time are in groups (MediaWindow use case)") {
        // Simulate MediaWindow behavior: get line data at specific time and check group membership
        auto * group_manager = data_manager->getEntityGroupManager();

        // Create test groups with some entities
        GroupId selected_group = group_manager->createGroup("Selected Lines");
        GroupId highlighted_group = group_manager->createGroup("Highlighted Lines");

        // Get all entity IDs from line data (simulating what MediaWindow would do)
        auto all_entity_ids = line_data->getAllEntityIds();
        REQUIRE(all_entity_ids.size() >= 5);

        // Add some entities to groups (simulating user selection/highlighting)
        // Add first 2 entities to selected group
        std::vector<EntityId> selected_entities = {all_entity_ids[0], all_entity_ids[1]};
        group_manager->addEntitiesToGroup(selected_group, selected_entities);

        // Add last entity to highlighted group
        std::vector<EntityId> highlighted_entities = {all_entity_ids.back()};
        group_manager->addEntitiesToGroup(highlighted_group, highlighted_entities);

        // Simulate MediaWindow getAtTime behavior for each time frame
        std::vector<TimeFrameIndex> test_times = {TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};

        for (auto time: test_times) {
            INFO("Testing time frame: " << time.getValue());

            // Get line data at this time (what MediaWindow does)
            auto lines_at_time = line_data->getAtTime(time);
            auto entity_ids_at_time = line_data->getEntityIdsAtTime(time);

            REQUIRE(lines_at_time.size() == entity_ids_at_time.size());

            // For each line at this time, check group membership (what MediaWindow would do for rendering)
            for (size_t i = 0; i < lines_at_time.size(); ++i) {
                EntityId entity_id = entity_ids_at_time[i];
                Line2D const & line = lines_at_time[i];

                INFO("Checking entity " << entity_id << " at time " << time.getValue() << ", line index " << i);

                // Check if this entity is in the selected group
                bool is_selected = group_manager->isEntityInGroup(selected_group, entity_id);

                // Check if this entity is in the highlighted group
                bool is_highlighted = group_manager->isEntityInGroup(highlighted_group, entity_id);

                // Get all groups containing this entity (useful for complex highlighting logic)
                auto groups_containing_entity = group_manager->getGroupsContainingEntity(entity_id);

                if (is_selected) {
                    INFO("Entity " << entity_id << " is SELECTED (should render differently)");
                    REQUIRE(std::find(groups_containing_entity.begin(), groups_containing_entity.end(), selected_group) != groups_containing_entity.end());
                }

                if (is_highlighted) {
                    INFO("Entity " << entity_id << " is HIGHLIGHTED (should render differently)");
                    REQUIRE(std::find(groups_containing_entity.begin(), groups_containing_entity.end(), highlighted_group) != groups_containing_entity.end());
                }

                if (!is_selected && !is_highlighted) {
                    INFO("Entity " << entity_id << " is NORMAL (default rendering)");
                    REQUIRE(groups_containing_entity.empty());
                }

                // Verify line data is accessible for rendering
                REQUIRE(line.size() >= 2);// Should have at least 2 points
            }
        }

        // Test bulk group membership checking (useful for performance in MediaWindow)
        auto entities_at_t10 = line_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entities_at_t20 = line_data->getEntityIdsAtTime(TimeFrameIndex(20));

        // Check which entities from time 10 are in selected group
        auto selected_entities_t10 = group_manager->getEntitiesInGroup(selected_group);
        std::vector<EntityId> selected_at_t10;
        std::set_intersection(entities_at_t10.begin(), entities_at_t10.end(),
                              selected_entities_t10.begin(), selected_entities_t10.end(),
                              std::back_inserter(selected_at_t10));

        INFO("At time 10: " << selected_at_t10.size() << " entities are selected");

        // Verify that we have expected selections
        if (entities_at_t10.size() >= 2) {
            REQUIRE(selected_at_t10.size() >= 1);// Should have at least one selected entity at t10
        }

        INFO("Successfully simulated MediaWindow group membership checking workflow");
    }

    SECTION("Dynamic group membership changes during MediaWindow navigation") {
        auto * group_manager = data_manager->getEntityGroupManager();

        // Create a dynamic selection group
        GroupId dynamic_selection = group_manager->createGroup("Dynamic Selection");

        // Simulate user selecting different entities as they navigate through time
        TimeFrameIndex current_time(10);
        auto entities_t10 = line_data->getEntityIdsAtTime(current_time);

        if (!entities_t10.empty()) {
            // Select first entity at time 10
            group_manager->addEntityToGroup(dynamic_selection, entities_t10[0]);

            // Verify selection
            auto lines_t10 = line_data->getAtTime(current_time);
            for (size_t i = 0; i < entities_t10.size(); ++i) {
                bool should_be_selected = (i == 0);
                bool is_selected = group_manager->isEntityInGroup(dynamic_selection, entities_t10[i]);
                REQUIRE(is_selected == should_be_selected);
            }
        }

        // Navigate to time 20 and change selection
        current_time = TimeFrameIndex(20);
        auto entities_t20 = line_data->getEntityIdsAtTime(current_time);

        if (!entities_t20.empty()) {
            // Clear previous selection and select different entity
            group_manager->clearGroup(dynamic_selection);
            group_manager->addEntityToGroup(dynamic_selection, entities_t20.back());

            // Verify old selection is cleared
            if (!entities_t10.empty()) {
                REQUIRE_FALSE(group_manager->isEntityInGroup(dynamic_selection, entities_t10[0]));
            }

            // Verify new selection
            auto lines_t20 = line_data->getAtTime(current_time);
            for (size_t i = 0; i < entities_t20.size(); ++i) {
                bool should_be_selected = (i == entities_t20.size() - 1);// Last entity
                bool is_selected = group_manager->isEntityInGroup(dynamic_selection, entities_t20[i]);
                REQUIRE(is_selected == should_be_selected);
            }
        }

        INFO("Successfully simulated dynamic group membership changes during navigation");
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - PointData Entity Lookup",
                 "[integration][entity][group][datamanager][pointdata]") {

    SECTION("PointData entity lookup and group membership checking") {
        // Create and populate PointData with test data
        auto point_data = std::make_shared<PointData>();

        // Create test points at different time frames
        std::vector<Point2D<float>> points_t10 = {
                {10.0f, 15.0f},
                {20.0f, 25.0f},
                {30.0f, 35.0f}};

        std::vector<Point2D<float>> points_t20 = {
                {40.0f, 45.0f},
                {50.0f, 55.0f},
                {60.0f, 65.0f},
                {70.0f, 75.0f}};

        std::vector<Point2D<float>> points_t30 = {
                {80.0f, 85.0f},
                {90.0f, 95.0f}};

        // Add points to PointData
        for (auto const & point: points_t10) {
            point_data->addAtTime(TimeFrameIndex(10), point);
        }
        for (auto const & point: points_t20) {
            point_data->addAtTime(TimeFrameIndex(20), point);
        }
        for (auto const & point: points_t30) {
            point_data->addAtTime(TimeFrameIndex(30), point);
        }

        // Set image size
        point_data->setImageSize({800, 600});

        // Set up identity context BEFORE adding to DataManager
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());
        point_data->rebuildAllEntityIds();

        // Add to DataManager
        data_manager->setData<PointData>("test_points", point_data, TimeKey("test_time"));

        // Verify EntityIds were generated
        auto all_entity_ids = point_data->getAllEntityIds();
        REQUIRE(all_entity_ids.size() == 9);// 3 + 4 + 2 = 9 total points

        // Verify all EntityIds are non-zero
        for (EntityId id: all_entity_ids) {
            REQUIRE(id != 0);
        }

        // Test entity lookup methods
        auto entities_t10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entities_t20 = point_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto entities_t30 = point_data->getEntityIdsAtTime(TimeFrameIndex(30));

        REQUIRE(entities_t10.size() == 3);
        REQUIRE(entities_t20.size() == 4);
        REQUIRE(entities_t30.size() == 2);

        // Test reverse lookup - get point by EntityId
        EntityId first_entity = entities_t10[0];
        auto point_lookup = point_data->getPointByEntityId(first_entity);
        REQUIRE(point_lookup.has_value());
        REQUIRE(point_lookup->x == Catch::Approx(10.0f));
        REQUIRE(point_lookup->y == Catch::Approx(15.0f));

        // Test time and index lookup
        auto time_info = point_data->getTimeAndIndexByEntityId(first_entity);
        REQUIRE(time_info.has_value());
        REQUIRE(time_info->first == TimeFrameIndex(10));
        REQUIRE(time_info->second == 0);// First point at this time

        // Create groups in EntityGroupManager
        auto * group_manager = data_manager->getEntityGroupManager();
        GroupId highlighted_group = group_manager->createGroup("highlighted_points");
        GroupId selected_group = group_manager->createGroup("selected_points");

        // Add some entities to groups
        group_manager->addEntityToGroup(highlighted_group, entities_t10[1]);// Second point at t10
        group_manager->addEntityToGroup(highlighted_group, entities_t20[0]);// First point at t20
        group_manager->addEntityToGroup(selected_group, entities_t20[2]);   // Third point at t20

        // Simulate MediaWindow behavior: get points at specific time and check group membership
        TimeFrameIndex current_time(20);
        auto points_at_time = point_data->getAtTime(current_time);
        auto entities_at_time = point_data->getEntityIdsAtTime(current_time);

        REQUIRE(points_at_time.size() == entities_at_time.size());
        REQUIRE(points_at_time.size() == 4);

        INFO("Simulating MediaWindow point rendering with group awareness:");

        for (size_t i = 0; i < points_at_time.size(); ++i) {
            Point2D<float> const & point = points_at_time[i];
            EntityId entity_id = entities_at_time[i];

            // Check group membership
            bool is_highlighted = group_manager->isEntityInGroup(highlighted_group, entity_id);
            bool is_selected = group_manager->isEntityInGroup(selected_group, entity_id);

            // Verify the relationship between entity and point data
            auto looked_up_point = point_data->getPointByEntityId(entity_id);
            REQUIRE(looked_up_point.has_value());
            REQUIRE(looked_up_point->x == Catch::Approx(point.x));
            REQUIRE(looked_up_point->y == Catch::Approx(point.y));

            // Log rendering decision
            if (is_selected) {
                INFO("Point (" << point.x << ", " << point.y << ") EntityId:" << entity_id
                               << " is SELECTED (special selection rendering)");
                REQUIRE(i == 2);// Should be the third point (index 2)
            } else if (is_highlighted) {
                INFO("Point (" << point.x << ", " << point.y << ") EntityId:" << entity_id
                               << " is HIGHLIGHTED (special highlight rendering)");
                REQUIRE(i == 0);// Should be the first point (index 0)
            } else {
                INFO("Point (" << point.x << ", " << point.y << ") EntityId:" << entity_id
                               << " is NORMAL (default rendering)");
                REQUIRE_FALSE(is_selected);
                REQUIRE_FALSE(is_highlighted);
            }
        }

        // Test batch operations
        std::vector<EntityId> batch_entities = {entities_t10[0], entities_t20[1], entities_t30[0]};
        auto batch_points = point_data->getPointsByEntityIds(batch_entities);
        auto batch_time_info = point_data->getTimeInfoByEntityIds(batch_entities);

        REQUIRE(batch_points.size() == 3);
        REQUIRE(batch_time_info.size() == 3);

        // Verify batch results
        REQUIRE(batch_points[0].first == entities_t10[0]);
        REQUIRE(batch_points[0].second.x == Catch::Approx(10.0f));
        REQUIRE(batch_points[0].second.y == Catch::Approx(15.0f));

        REQUIRE(std::get<0>(batch_time_info[0]) == entities_t10[0]);
        REQUIRE(std::get<1>(batch_time_info[0]) == TimeFrameIndex(10));
        REQUIRE(std::get<2>(batch_time_info[0]) == 0);

        INFO("Successfully tested PointData entity lookup and group membership integration");
    }
}

TEST_CASE("EntityGroupManager Integration - DigitalIntervalSeries Entity Lookup",
          "[integration][entity][group][datamanager][digitalintervalseries]") {

    SECTION("DigitalIntervalSeries entity lookup and group membership") {
        DataManager data_manager;

        // Create and populate DigitalIntervalSeries with test intervals
        auto interval_data = std::make_shared<DigitalIntervalSeries>();

        // Add test intervals at different times
        interval_data->addEvent(Interval{100, 200});  // Index 0: 100ms duration from t=100 to t=200
        interval_data->addEvent(Interval{300, 350});  // Index 1: 50ms duration from t=300 to t=350
        interval_data->addEvent(Interval{500, 800});  // Index 2: 300ms duration from t=500 to t=800
        interval_data->addEvent(Interval{1000, 1100});// Index 3: 100ms duration from t=1000 to t=1100
        interval_data->addEvent(Interval{1200, 1300});// Index 4: 100ms duration from t=1200 to t=1300

        // Set up identity context before adding to DataManager
        interval_data->setIdentityContext("test_intervals", data_manager.getEntityRegistry());
        interval_data->rebuildAllEntityIds();

        // Add to DataManager
        data_manager.setData<DigitalIntervalSeries>("test_intervals", interval_data, TimeKey("test_time"));

        // Verify EntityIds were generated correctly
        auto all_entity_ids = interval_data->getEntityIds();
        REQUIRE(all_entity_ids.size() == 5);

        for (EntityId entity_id: all_entity_ids) {
            REQUIRE(entity_id != 0);// All should be valid, non-zero EntityIds
        }

        // Test entity lookup functionality
        EntityId first_entity = all_entity_ids[0];
        EntityId third_entity = all_entity_ids[2];
        EntityId last_entity = all_entity_ids[4];

        // Test getIntervalByEntityId
        auto first_interval = interval_data->getIntervalByEntityId(first_entity);
        auto third_interval = interval_data->getIntervalByEntityId(third_entity);
        auto last_interval = interval_data->getIntervalByEntityId(last_entity);

        REQUIRE(first_interval.has_value());
        REQUIRE(third_interval.has_value());
        REQUIRE(last_interval.has_value());

        REQUIRE(first_interval->start == 100);
        REQUIRE(first_interval->end == 200);
        REQUIRE(third_interval->start == 500);
        REQUIRE(third_interval->end == 800);
        REQUIRE(last_interval->start == 1200);
        REQUIRE(last_interval->end == 1300);

        // Test getIndexByEntityId
        auto first_index = interval_data->getIndexByEntityId(first_entity);
        auto third_index = interval_data->getIndexByEntityId(third_entity);
        auto last_index = interval_data->getIndexByEntityId(last_entity);

        REQUIRE(first_index.has_value());
        REQUIRE(third_index.has_value());
        REQUIRE(last_index.has_value());

        REQUIRE(*first_index == 0);
        REQUIRE(*third_index == 2);
        REQUIRE(*last_index == 4);

        // Create groups using EntityGroupManager
        auto * group_manager = data_manager.getEntityGroupManager();

        GroupId short_intervals = group_manager->createGroup("Short Duration Intervals");
        GroupId long_intervals = group_manager->createGroup("Long Duration Intervals");

        // Classify intervals by duration (short: <= 100ms, long: > 100ms)
        std::vector<EntityId> short_entities;
        std::vector<EntityId> long_entities;

        for (size_t i = 0; i < all_entity_ids.size(); ++i) {
            auto interval = interval_data->getIntervalByEntityId(all_entity_ids[i]);
            REQUIRE(interval.has_value());

            int64_t duration = interval->end - interval->start;
            if (duration <= 100) {
                short_entities.push_back(all_entity_ids[i]);
            } else {
                long_entities.push_back(all_entity_ids[i]);
            }
        }

        REQUIRE(short_entities.size() == 4);// Intervals 0, 1, 3, 4 have <= 100ms duration
        REQUIRE(long_entities.size() == 1); // Interval 2 has > 100ms duration

        // Add entities to groups
        group_manager->addEntitiesToGroup(short_intervals, short_entities);
        group_manager->addEntitiesToGroup(long_intervals, long_entities);

        // Test group membership checking
        for (EntityId entity_id: short_entities) {
            auto groups = group_manager->getGroupsContainingEntity(entity_id);
            REQUIRE(groups.size() == 1);
            REQUIRE(groups[0] == short_intervals);

            // Verify we can lookup the interval data using the entity
            auto interval = interval_data->getIntervalByEntityId(entity_id);
            REQUIRE(interval.has_value());

            int64_t duration = interval->end - interval->start;
            REQUIRE(duration <= 100);
        }

        for (EntityId entity_id: long_entities) {
            auto groups = group_manager->getGroupsContainingEntity(entity_id);
            REQUIRE(groups.size() == 1);
            REQUIRE(groups[0] == long_intervals);

            // Verify we can lookup the interval data using the entity
            auto interval = interval_data->getIntervalByEntityId(entity_id);
            REQUIRE(interval.has_value());

            int64_t duration = interval->end - interval->start;
            REQUIRE(duration > 100);
        }

        // Test batch operations
        std::vector<EntityId> batch_entities = {all_entity_ids[0], all_entity_ids[2], all_entity_ids[4]};
        auto batch_intervals = interval_data->getIntervalsByEntityIds(batch_entities);
        auto batch_index_info = interval_data->getIndexInfoByEntityIds(batch_entities);

        REQUIRE(batch_intervals.size() == 3);
        REQUIRE(batch_index_info.size() == 3);

        // Verify batch results
        REQUIRE(batch_intervals[0].first == all_entity_ids[0]);
        REQUIRE(batch_intervals[0].second.start == 100);
        REQUIRE(batch_intervals[0].second.end == 200);

        REQUIRE(batch_index_info[0].first == all_entity_ids[0]);
        REQUIRE(batch_index_info[0].second == 0);

        REQUIRE(batch_intervals[1].first == all_entity_ids[2]);
        REQUIRE(batch_intervals[1].second.start == 500);
        REQUIRE(batch_intervals[1].second.end == 800);

        REQUIRE(batch_index_info[1].first == all_entity_ids[2]);
        REQUIRE(batch_index_info[1].second == 2);

        INFO("Successfully tested DigitalIntervalSeries entity lookup and group membership integration");
    }
}

TEST_CASE_METHOD(EntityGroupManagerIntegrationFixture,
                 "EntityGroupManager Integration - PCA Transform Entity Preservation",
                 "[integration][entity][group][datamanager][pca][transform]") {

    std::cout << "CTEST_FULL_OUTPUT" << std::endl; // To show output in CTest logs

    SECTION("EntityIDs are preserved through PCA transformation") {
        // This test verifies that the new EntityID API correctly retrieves EntityIDs
        // from column computers rather than row selectors, and that these are properly
        // preserved through PCA transformation
        
        auto * table_registry = data_manager->getTableRegistry();
        std::string table_key = "pca_test_table";

        // Create table entry first
        REQUIRE(table_registry->createTable(table_key, "PCA Test Table"));

        // Create DataManagerExtension
        auto data_manager_extension = table_registry->getDataManagerExtension();
        REQUIRE(data_manager_extension != nullptr);

        auto timeFrame = data_manager->getTime(TimeKey("test_time"));
        // Create row selector with timestamps 
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto row_selector = std::make_unique<TimestampSelector>(timestamps, timeFrame);

        // Create LineDataAdapter
        auto line_adapter = std::make_shared<LineDataAdapter>(line_data, timeFrame, "test_lines");

        // Create LineSamplingMultiComputer - this will provide EntityIDs from LineData
        int segments = 3;// This creates 8 columns: x@0.000, y@0.000, x@0.333, y@0.333, x@0.667, y@0.667, x@1.000, y@1.000
        auto line_computer = std::make_unique<LineSamplingMultiComputer>(
                std::static_pointer_cast<ILineSource>(line_adapter),
                "test_lines",
                timeFrame,
                segments);

        // Build the original table
        TableViewBuilder builder(data_manager_extension);
        builder.setRowSelector(std::move(row_selector));
        builder.addColumns<double>("Line", std::move(line_computer));

        auto original_table = builder.build();

        //Get all values to make sure they are build and entity IDs are populated
        auto column_1 = original_table.getColumnValues<double>("Line.x@0.000");
        auto column_2 = original_table.getColumnValues<double>("Line.y@0.000");
        auto column_3 = original_table.getColumnValues<double>("Line.x@0.333");
        auto column_4 = original_table.getColumnValues<double>("Line.y@0.333");
        auto column_5 = original_table.getColumnValues<double>("Line.x@0.667");
        auto column_6 = original_table.getColumnValues<double>("Line.y@0.667");
        auto column_7 = original_table.getColumnValues<double>("Line.x@1.000");
        auto column_8 = original_table.getColumnValues<double>("Line.y@1.000");
        
        // Verify we have numerical columns for PCA
        auto column_names = original_table.getColumnNames();
        REQUIRE(column_names.size() == 8);         // 4 sample points * 2 coordinates = 8 columns
        REQUIRE(original_table.getRowCount() == 5);// 2 + 2 + 1 = 5 entities across 3 timestamps

        // Verify that EntityIDs are available from the original table
        // This tests the new API where EntityIDs come from column computers
        auto original_entity_ids_variant = original_table.getColumnEntityIds(column_names[0]);
        auto original_entity_ids = std::get<std::vector<EntityId>>(original_entity_ids_variant);
        
        REQUIRE(original_entity_ids.size() == 5);

        // Verify EntityIds are valid (non-zero) and come from LineData
        std::set<EntityId> expected_entity_ids;
        for (EntityId id : line_data->getAllEntityIds()) {
            expected_entity_ids.insert(id);
        }
        
        for (EntityId id : original_entity_ids) {
            REQUIRE(id != 0);
            REQUIRE(expected_entity_ids.count(id) > 0); // Should be from LineData
        }

        // Test per-row EntityID access
        for (size_t row = 0; row < original_table.getRowCount(); ++row) {
            auto row_entity_ids = original_table.getRowEntityIds(row);
            REQUIRE_FALSE(row_entity_ids.empty());
            // For LineSamplingMultiComputer, each row should have one EntityID (the line being sampled)
            REQUIRE(row_entity_ids.size() == 1);
            REQUIRE(row_entity_ids[0] == original_entity_ids[row]);
        }

        // Store original table in registry
        REQUIRE(table_registry->storeBuiltTable(table_key, std::make_unique<TableView>(std::move(original_table))));

        // Get the stored table for PCA transformation
        auto stored_original = table_registry->getBuiltTable(table_key);
        REQUIRE(stored_original != nullptr);

        // Configure PCA transformation
        PCAConfig pca_config;
        pca_config.center = true;
        pca_config.standardize = false;
        // Include all columns (they should all be numerical)
        for (auto const & col_name: column_names) {
            pca_config.include.push_back(col_name);
        }

        // Apply PCA transformation
        PCATransform pca_transform(pca_config);
        TableView transformed_table = pca_transform.apply(*stored_original);

        // Verify EntityIDs are preserved through transformation
        auto transformed_entity_ids = transformed_table.getEntityIds();

        // PCA may filter out rows with NaN/Inf values, so count might be different
        size_t transformed_row_count = transformed_table.getRowCount();
        REQUIRE(transformed_entity_ids.size() == transformed_row_count);
        REQUIRE(transformed_row_count <= original_entity_ids.size());


        // Test that we can still trace back to original LineData using preserved EntityIds
        auto group_manager = data_manager->getEntityGroupManager();
        GroupId pca_group = group_manager->createGroup("PCA Results");
        
        //Flatten transformed_entity_ids to unique
        std::set<EntityId> unique_transformed_ids;
        for (auto row_entity_ids : transformed_entity_ids) {
            for (auto id : row_entity_ids) {
                unique_transformed_ids.insert(id);
            }
        }
        std::vector<EntityId> unique_transformed_ids_vec(unique_transformed_ids.begin(), unique_transformed_ids.end());

        // Add all transformed EntityIDs to a group to test traceability
        group_manager->addEntitiesToGroup(pca_group, unique_transformed_ids_vec);

        // Verify we can trace back to original LineData using these EntityIds
        for (EntityId entity_id : unique_transformed_ids_vec) {
            auto original_line = line_data->getDataByEntityId(entity_id);
            REQUIRE(original_line.has_value());

        }

        // Verify transformed table has PCA columns
        auto transformed_column_names = transformed_table.getColumnNames();
        bool has_pc_columns = false;
        for (auto const & name: transformed_column_names) {
            if (name.find("PC") == 0) {// Starts with "PC"
                has_pc_columns = true;
                break;
            }
        }
        REQUIRE(has_pc_columns);

        INFO("Successfully preserved " << transformed_entity_ids.size() << " EntityIDs through PCA transformation");
        INFO("Original table: " << column_names.size() << " columns, " << original_entity_ids.size() << " rows");
        INFO("Transformed table: " << transformed_column_names.size() << " columns, " << transformed_row_count << " rows");
    }

}
