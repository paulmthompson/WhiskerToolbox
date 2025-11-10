#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "transforms/Lines/Line_Proximity_Grouping/line_proximity_grouping.hpp"
#include "fixtures/data_manager_test_fixtures.hpp"
#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"

TEST_CASE_METHOD(DataManagerTestFixture, "Data Transform: LineProximityGrouping - Basic functionality", "[LineProximityGrouping]") {

    auto& dm = getDataManager();
    auto* group_manager = dm.getEntityGroupManager();
    
    SECTION("LineProximityGroupingOperation - getName and canApply") {
        LineProximityGroupingOperation operation;
        
        REQUIRE(operation.getName() == "Group Lines by Proximity");
        REQUIRE(operation.getTargetInputTypeIndex() == std::type_index(typeid(std::shared_ptr<LineData>)));
        
        // Test canApply with valid LineData
        auto line_data = std::make_shared<LineData>();
        DataTypeVariant valid_variant = line_data;
        REQUIRE(operation.canApply(valid_variant));
        
        // Test canApply with invalid data
        auto analog_data = std::make_shared<AnalogTimeSeries>();
        DataTypeVariant invalid_variant = analog_data;
        REQUIRE_FALSE(operation.canApply(invalid_variant));
    }
    
    SECTION("LineProximityGroupingParameters - Basic construction") {
        LineProximityGroupingParameters params(group_manager);
        
        REQUIRE(params.getGroupManager() == group_manager);
        REQUIRE(params.distance_threshold == 50.0f);
        REQUIRE(params.position_along_line == 0.5f);
        REQUIRE(params.create_new_group_for_outliers == true);
        REQUIRE(params.new_group_name == "Ungrouped Lines");
    }
    
    SECTION("Basic grouping with no existing groups") {
        // Create test LineData
        auto line_data = std::make_shared<LineData>();
        
        // Add some test lines
        std::vector<Point2D<float>> line1 = {{0.0f, 0.0f}, {10.0f, 0.0f}};
        std::vector<Point2D<float>> line2 = {{5.0f, 5.0f}, {15.0f, 5.0f}};
        std::vector<Point2D<float>> line3 = {{100.0f, 100.0f}, {110.0f, 100.0f}}; // Far away
        
        line_data->addAtTime(TimeFrameIndex(1), line1, NotifyObservers::No);
        line_data->addAtTime(TimeFrameIndex(1), line2, NotifyObservers::No);
        line_data->addAtTime(TimeFrameIndex(1), line3, NotifyObservers::No);
        
        // Set up entity context
        line_data->setIdentityContext("test_lines", dm.getEntityRegistry());
        line_data->rebuildAllEntityIds();
        
        LineProximityGroupingParameters params(group_manager);
        params.distance_threshold = 10.0f; // Close lines should group together
        
        // Execute grouping
        auto result = lineProximityGrouping(line_data, &params);
        
        // Should return the same shared_ptr
        REQUIRE(result == line_data);
        
        // Should have created a group for ungrouped lines
        REQUIRE(group_manager->getGroupCount() == 1);
        
        auto group_ids = group_manager->getAllGroupIds();
        auto group_entities = group_manager->getEntitiesInGroup(group_ids[0]);
        
        // All entities should be in the group
        REQUIRE(group_entities.size() == 3);
    }
}

TEST_CASE("Data Transform: LineProximityGrouping - Distance calculation", "[LineProximityGrouping]") {
    
    SECTION("calculateLineDistance - Basic functionality") {
        Line2D line1 = {{0.0f, 0.0f}, {10.0f, 0.0f}};
        Line2D line2 = {{0.0f, 5.0f}, {10.0f, 5.0f}};
        
        // At position 0.5 (middle), both lines have points at (5, 0) and (5, 5)
        float distance = calculateLineDistance(line1, line2, 0.5f);
        REQUIRE_THAT(distance, Catch::Matchers::WithinAbs(5.0f, 0.01f));
        
        // At position 0.0 (start), distance should still be 5
        distance = calculateLineDistance(line1, line2, 0.0f);
        REQUIRE_THAT(distance, Catch::Matchers::WithinAbs(5.0f, 0.01f));
    }
    
    SECTION("calculateLineDistance - Invalid lines") {
        Line2D empty_line;
        Line2D valid_line = {{0.0f, 0.0f}, {10.0f, 0.0f}};
        
        // Distance with empty line should be max float
        float distance = calculateLineDistance(empty_line, valid_line, 0.5f);
        REQUIRE(distance == std::numeric_limits<float>::max());
    }
}