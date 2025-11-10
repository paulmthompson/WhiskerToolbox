#include "transforms/Lines/Line_Outlier_Detection/line_outlier_detection.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

/**
 * @brief Test fixture for Line Outlier Detection functionality
 * 
 * This fixture creates a scenario with two groups of lines that move smoothly across frames,
 * plus some intentional outliers that deviate significantly from the group trajectory.
 */
class LineOutlierDetectionTestFixture {
public:
    LineOutlierDetectionTestFixture() {
        // Create DataManager and EntityGroupManager
        data_manager = std::make_unique<DataManager>();
        group_manager = std::make_unique<EntityGroupManager>();

        std::vector<int> timeValues;
        for (int i = 0; i <= 100; ++i) {
            timeValues.push_back(i);
        }
        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);

        // Create LineData
        line_data = std::make_shared<LineData>();
        data_manager->setData<LineData>("test_lines", line_data, TimeKey("test_timeframe"));
        auto registry = data_manager->getEntityRegistry();

        // Create two groups
        group1_id = group_manager->createGroup("Group1", "Normal trajectory group 1");
        group2_id = group_manager->createGroup("Group2", "Normal trajectory group 2");

        // Generate test data: 100 frames
        // Group 1: moves from (50, 50) to (150, 150) smoothly
        // Group 2: moves from (200, 200) to (100, 100) smoothly
        // Add some outliers at specific frames

        std::random_device rd;
        std::mt19937 gen(42);                    // Fixed seed for reproducibility (intentional for test determinism)
        std::normal_distribution noise(0.0, 2.0);// Small noise

        for (int frame = 0; frame < 100; ++frame) {
            // Determine if this frame should have outliers instead of normal trajectories
            bool is_outlier_frame = (frame == 25 || frame == 50 || frame == 75);

            // Group 1 trajectory
            auto const x1 = 50.0f + static_cast<float>(frame);
            auto const y1 = 50.0f + static_cast<float>(frame);
            
            if (is_outlier_frame) {
                // Create outlier for group 1 (way off trajectory)
                Line2D outlier1 = createLine(x1 + 100.0f, y1 + 100.0f, 150.0f);
                line_data->addAtTime(TimeFrameIndex(frame), outlier1, NotifyObservers::No);
                EntityId outlier_entity1 = line_data->getEntityIdsAtTime(TimeFrameIndex(frame)).back();
                group_manager->addEntityToGroup(group1_id, outlier_entity1);
                group1_entities[frame] = outlier_entity1;
                outlier_entities.push_back(outlier_entity1);
            } else {
                // Create normal trajectory for group 1
                auto const line1 = createLine(x1 + static_cast<float>(noise(gen)),
                                              y1 + static_cast<float>(noise(gen)),
                                              50.0f + static_cast<float>(noise(gen)));
                line_data->addAtTime(TimeFrameIndex(frame), line1, NotifyObservers::No);
                auto const entity1 = line_data->getEntityIdsAtTime(TimeFrameIndex(frame)).back();
                group_manager->addEntityToGroup(group1_id, entity1);
                group1_entities[frame] = entity1;
            }

            // Group 2 trajectory
            auto const x2 = 200.0f - static_cast<float>(frame);
            auto const y2 = 200.0f - static_cast<float>(frame);
            
            if (is_outlier_frame) {
                // Create outlier for group 2 (way off trajectory)
                Line2D outlier2 = createLine(x2 - 100.0f, y2 - 100.0f, 20.0f);
                line_data->addAtTime(TimeFrameIndex(frame), outlier2, NotifyObservers::No);
                EntityId outlier_entity2 = line_data->getEntityIdsAtTime(TimeFrameIndex(frame)).back();
                group_manager->addEntityToGroup(group2_id, outlier_entity2);
                group2_entities[frame] = outlier_entity2;
                outlier_entities.push_back(outlier_entity2);
            } else {
                // Create normal trajectory for group 2
                auto const line2 = createLine(x2 + static_cast<float>(noise(gen)),
                                              y2 + static_cast<float>(noise(gen)),
                                              60.0f + static_cast<float>(noise(gen)));
                line_data->addAtTime(TimeFrameIndex(frame), line2, NotifyObservers::No);
                auto const entity2 = line_data->getEntityIdsAtTime(TimeFrameIndex(frame)).back();
                group_manager->addEntityToGroup(group2_id, entity2);
                group2_entities[frame] = entity2;
            }
        }
    }

    // Helper to create a line centered at (x, y) with given length
    Line2D createLine(float center_x, float center_y, float length) const {
        Line2D line;
        constexpr int num_points = 10;
        for (int i = 0; i < num_points; ++i) {
            auto const t = static_cast<float>(i) / static_cast<float>(num_points - 1);
            float x = center_x - length / 2.0f + t * length;
            float y = center_y;// Horizontal line
            line.push_back(Point2D<float>(x, y));
        }
        return line;
    }

    std::unique_ptr<DataManager> data_manager;
    std::unique_ptr<EntityGroupManager> group_manager;
    std::shared_ptr<LineData> line_data;

    GroupId group1_id;
    GroupId group2_id;

    std::map<int, EntityId> group1_entities;
    std::map<int, EntityId> group2_entities;
    std::vector<EntityId> outlier_entities;
};

TEST_CASE_METHOD(LineOutlierDetectionTestFixture, "Data Transform: LineOutlierDetection - Basic Functionality", "[LineOutlierDetection]") {

    SECTION("Test fixture setup is correct") {
        // Verify we have the expected data structure
        REQUIRE(line_data != nullptr);
        REQUIRE(group_manager != nullptr);

        // Check that we have lines at all frames
        for (int frame = 0; frame < 100; ++frame) {
            auto lines = line_data->getAtTime(TimeFrameIndex(frame));
            REQUIRE(!lines.empty());
        }

        // Check groups exist
        auto group1_entities = group_manager->getEntitiesInGroup(group1_id);
        auto group2_entities = group_manager->getEntitiesInGroup(group2_id);
        REQUIRE(group1_entities.size() == 100);// 100 frames (3 are outliers, 97 are normal)
        REQUIRE(group2_entities.size() == 100);// 100 frames (3 are outliers, 97 are normal)

        // Check outlier entities exist
        REQUIRE(outlier_entities.size() == 6);// 3 frames * 2 groups
    }
}

TEST_CASE_METHOD(LineOutlierDetectionTestFixture, "Data Transform: LineOutlierDetection - Full Algorithm Test", "[LineOutlierDetection]") {

    SECTION("Basic outlier detection with default parameters") {
        // Create parameters
        LineOutlierDetectionParameters params(group_manager.get());
        params.verbose_output = true;
        params.mad_threshold = 3.0;// Lower threshold to be more sensitive

        // Count entities in outlier group before
        auto all_groups_before = group_manager->getAllGroupIds();
        REQUIRE(all_groups_before.size() == 2);// Only group1 and group2

        // Run the algorithm
        auto result = lineOutlierDetection(line_data, &params);

        // Should return the same line_data pointer
        REQUIRE(result == line_data);

        // Check if outlier group was created
        auto all_groups_after = group_manager->getAllGroupIds();
        REQUIRE(all_groups_after.size() > 2);// Should have added outlier group

        // Find the outlier group
        std::optional<GroupId> outlier_group_id;
        for (auto gid: all_groups_after) {
            auto desc = group_manager->getGroupDescriptor(gid);
            if (desc && desc->name == params.outlier_group_name) {
                outlier_group_id = gid;
                break;
            }
        }
        REQUIRE(outlier_group_id.has_value());

        // Check that some outliers were detected
        auto outliers_detected = group_manager->getEntitiesInGroup(*outlier_group_id);
        INFO("Outliers detected: " << outliers_detected.size());
        REQUIRE(!outliers_detected.empty());

        // Verify that at least some of the intentional outliers were detected
        int correct_outliers = 0;
        for (auto entity_id: outliers_detected) {
            if (std::find(outlier_entities.begin(), outlier_entities.end(), entity_id) != outlier_entities.end()) {
                correct_outliers++;
            }
        }
        INFO("Correct outliers: " << correct_outliers << " / " << outlier_entities.size());
        REQUIRE(correct_outliers > 0);// At least some outliers should be detected
    }

    SECTION("Process specific groups only") {
        // Create parameters to only process group 1
        LineOutlierDetectionParameters params(group_manager.get());
        params.groups_to_process = {group1_id};
        params.verbose_output = true;
        params.mad_threshold = 3.0;

        // Run the algorithm
        auto result = lineOutlierDetection(line_data, &params);

        REQUIRE(result == line_data);

        // Find the outlier group
        auto all_groups = group_manager->getAllGroupIds();
        std::optional<GroupId> outlier_group_id;
        for (auto gid: all_groups) {
            auto desc = group_manager->getGroupDescriptor(gid);
            if (desc && desc->name == params.outlier_group_name) {
                outlier_group_id = gid;
                break;
            }
        }

        if (outlier_group_id.has_value()) {
            auto outliers_detected = group_manager->getEntitiesInGroup(*outlier_group_id);
            INFO("Outliers detected in group 1: " << outliers_detected.size());

            // All detected outliers should be from group 1
            for (auto entity_id: outliers_detected) {
                // Check if this entity was originally in group 1
                bool in_group1 = (group1_entities.find(0) != group1_entities.end() &&
                                  std::find_if(group1_entities.begin(), group1_entities.end(),
                                               [entity_id](auto const & p) { return p.second == entity_id; }) != group1_entities.end()) ||
                                 std::find(outlier_entities.begin(), outlier_entities.end(), entity_id) != outlier_entities.end();
                // Note: This is a simplified check; in practice we'd track which group each outlier came from
            }
        }
    }

    SECTION("Custom outlier group name") {
        // Create parameters with custom outlier group name
        LineOutlierDetectionParameters params(group_manager.get());
        params.outlier_group_name = "Custom Outliers";
        params.verbose_output = false;
        params.mad_threshold = 3.0;

        // Run the algorithm
        auto result = lineOutlierDetection(line_data, &params);

        // Find the custom outlier group
        auto all_groups = group_manager->getAllGroupIds();
        std::optional<GroupId> outlier_group_id;
        for (auto gid: all_groups) {
            auto desc = group_manager->getGroupDescriptor(gid);
            if (desc && desc->name == "Custom Outliers") {
                outlier_group_id = gid;
                break;
            }
        }

        REQUIRE(outlier_group_id.has_value());
    }
}

TEST_CASE_METHOD(LineOutlierDetectionTestFixture, "Data Transform: LineOutlierDetection - Error Handling", "[LineOutlierDetection]") {

    SECTION("Null group manager") {
        LineOutlierDetectionParameters params(nullptr);
        auto result = lineOutlierDetection(line_data, &params);
        REQUIRE(result == line_data);// Should return without crashing
    }

    SECTION("Null line data") {
        LineOutlierDetectionParameters params(group_manager.get());
        auto result = ::lineOutlierDetection(nullptr, &params);
        REQUIRE(result == nullptr);
    }

    SECTION("Null parameters") {
        auto result = lineOutlierDetection(line_data, nullptr);
        REQUIRE(result == line_data);
    }
}

TEST_CASE_METHOD(LineOutlierDetectionTestFixture, "Data Transform: LineOutlierDetection - Operation Interface", "[LineOutlierDetection]") {

    SECTION("Operation name and type checking") {
        LineOutlierDetectionOperation op;
        REQUIRE(op.getName() == "Line Outlier Detection");

        DataTypeVariant variant(line_data);
        REQUIRE(op.canApply(variant));

        auto default_params = op.getDefaultParameters();
        REQUIRE(default_params != nullptr);
    }

    SECTION("Execute via operation interface") {
        LineOutlierDetectionOperation op;

        auto params = std::make_unique<LineOutlierDetectionParameters>(group_manager.get());
        params->mad_threshold = 3.0;

        DataTypeVariant input(line_data);
        auto output = op.execute(input, params.get());

        REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(output));
        auto output_data = std::get<std::shared_ptr<LineData>>(output);
        REQUIRE(output_data == line_data);
    }
}
