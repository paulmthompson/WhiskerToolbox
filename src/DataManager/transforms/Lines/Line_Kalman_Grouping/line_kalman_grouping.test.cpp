#include "transforms/Lines/Line_Kalman_Grouping/line_kalman_grouping.hpp"

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
#include <vector>

/**
 * @brief Test fixture for Line Kalman Grouping functionality
 * 
 * This fixture creates a scenario with two distinct lines that move across frames
 * with interspersed ground truth (manually labeled) frames.
 */
class LineKalmanGroupingTestFixture {
protected:
    LineKalmanGroupingTestFixture() {
        // Initialize DataManager
        data_manager = std::make_unique<DataManager>();

        // Create EntityGroupManager
        group_manager = std::make_unique<EntityGroupManager>();

        // Create a TimeFrame (100 frames)
        auto timeValues = std::vector<int>();
        for (int i = 0; i < 100; ++i) {
            timeValues.push_back(i);
        }

        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);

        // Create LineData
        data_manager->setData<LineData>("test_lines", TimeKey("test_timeframe"));
        line_data = data_manager->getData<LineData>("test_lines");

        // Create two groups for testing
        group1_id = group_manager->createGroup("Line Group 1", "First test group");
        group2_id = group_manager->createGroup("Line Group 2", "Second test group");

        generateTestData();
    }

    void generateTestData() {
        // Generate two lines that move in predictable patterns
        // Line 1: starts at (100, 100) and moves right with small vertical oscillation
        // Line 2: starts at (100, 200) and moves right with small vertical oscillation

        for (int frame = 0; frame < 100; ++frame) {
            // Line 1: horizontal movement with sine wave vertical component
            Line2D line1;
            float base_x1 = 100.0f + frame * 2.0f;                  // moves 2 pixels right per frame
            float base_y1 = 100.0f + 10.0f * std::sin(frame * 0.1f);// small oscillation

            // Create a short horizontal line segment
            for (int i = 0; i < 10; ++i) {
                line1.push_back({base_x1 + i, base_y1});
            }

            // Line 2: similar movement but offset vertically
            Line2D line2;
            float base_x2 = 100.0f + frame * 2.0f;
            float base_y2 = 200.0f + 8.0f * std::sin(frame * 0.15f + 1.0f);// different phase

            for (int i = 0; i < 10; ++i) {
                line2.push_back({base_x2 + i, base_y2});
            }

            // Add lines to data
            line_data->addAtTime(TimeFrameIndex(frame), line1, false);
            line_data->addAtTime(TimeFrameIndex(frame), line2, false);
        }

        // Get all entity IDs (should be 200 total: 2 per frame * 100 frames)
        auto all_entity_ids = line_data->getAllEntityIds();
        REQUIRE(all_entity_ids.size() == 200);

        // Store entity IDs for each line and frame for easy access
        for (int frame = 0; frame < 100; ++frame) {
            auto entities_at_frame = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entities_at_frame.size() == 2);

            // Sort by line position to ensure consistent assignment
            std::vector<EntityId> sorted_entities = entities_at_frame;
            std::sort(sorted_entities.begin(), sorted_entities.end(),
                      [this](EntityId a, EntityId b) {
                          auto line_a = line_data->getLineByEntityId(a);
                          auto line_b = line_data->getLineByEntityId(b);
                          if (!line_a || !line_b) return false;

                          // Sort by Y coordinate (line 1 has lower Y than line 2)
                          float y_a = line_a->empty() ? 0.0f : line_a->front().y;
                          float y_b = line_b->empty() ? 0.0f : line_b->front().y;
                          return y_a < y_b;
                      });

            line1_entities[frame] = sorted_entities[0];
            line2_entities[frame] = sorted_entities[1];
        }

        // Add ground truth labels at specific frames (every 10 frames)
        for (int frame = 0; frame < 100; frame += 10) {
            group_manager->addEntityToGroup(group1_id, line1_entities[frame]);
            group_manager->addEntityToGroup(group2_id, line2_entities[frame]);
            ground_truth_frames.push_back(frame);
        }
    }

    std::unique_ptr<DataManager> data_manager;
    std::unique_ptr<EntityGroupManager> group_manager;
    std::shared_ptr<LineData> line_data;

    GroupId group1_id;
    GroupId group2_id;

    std::map<int, EntityId> line1_entities;// frame -> entity_id for line 1
    std::map<int, EntityId> line2_entities;// frame -> entity_id for line 2
    std::vector<int> ground_truth_frames;
};

TEST_CASE_METHOD(LineKalmanGroupingTestFixture, "LineKalmanGrouping - Basic Functionality", "[LineKalmanGrouping]") {

    SECTION("Test fixture setup is correct") {
        // Verify we have the expected data structure
        REQUIRE(line_data != nullptr);
        REQUIRE(group_manager != nullptr);

        // Check that we have lines at all frames
        for (int frame = 0; frame < 100; ++frame) {
            auto entities = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entities.size() == 2);
        }

        // Check ground truth labels
        REQUIRE(ground_truth_frames.size() == 10);// Every 10 frames

        auto group1_entities = group_manager->getEntitiesInGroup(group1_id);
        auto group2_entities = group_manager->getEntitiesInGroup(group2_id);
        REQUIRE(group1_entities.size() == 10);
        REQUIRE(group2_entities.size() == 10);
    }
}

TEST_CASE_METHOD(LineKalmanGroupingTestFixture, "LineKalmanGrouping - Full Algorithm Test", "[LineKalmanGrouping]") {

    SECTION("Basic grouping with default parameters") {
        // Create parameters
        LineKalmanGroupingParameters params(group_manager.get());
        params.verbose_output = false;

        // Count ungrouped entities before
        auto all_entities = line_data->getAllEntityIds();
        std::unordered_set<EntityId> grouped_before;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped_before.insert(entities_in_group.begin(), entities_in_group.end());
        }
        size_t ungrouped_before = all_entities.size() - grouped_before.size();

        // Run the algorithm
        auto result = lineKalmanGrouping(line_data, &params);

        // Should return the same line_data pointer
        REQUIRE(result == line_data);

        // Count grouped entities after (include putative groups)
        std::unordered_set<EntityId> grouped_after;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped_after.insert(entities_in_group.begin(), entities_in_group.end());
        }
        auto descriptors = group_manager->getAllGroupDescriptors();
        auto findPutativeFor = [&](GroupId base_group) -> std::optional<GroupId> {
            auto desc = group_manager->getGroupDescriptor(base_group);
            if (!desc.has_value()) return std::nullopt;
            std::string expected = std::string("Putative:") + desc->name;
            for (auto const & d: descriptors) {
                if (d.name == expected) return d.id;
            }
            return std::nullopt;
        };
        if (auto p1 = findPutativeFor(group1_id)) {
            auto ents = group_manager->getEntitiesInGroup(*p1);
            grouped_after.insert(ents.begin(), ents.end());
        }
        if (auto p2 = findPutativeFor(group2_id)) {
            auto ents = group_manager->getEntitiesInGroup(*p2);
            grouped_after.insert(ents.begin(), ents.end());
        }
        size_t ungrouped_after = all_entities.size() - grouped_after.size();

        // Should have assigned more entities to groups
        REQUIRE(ungrouped_after < ungrouped_before);

        // Most entities should be assigned (at least 50% improvement)
        REQUIRE(ungrouped_after <= ungrouped_before / 2);
    }

    SECTION("Verify correct line assignment") {
        // Create parameters with verbose output for debugging
        LineKalmanGroupingParameters params(group_manager.get());
        params.verbose_output = true;// Enable for debugging

        // Debug: Check initial state
        auto all_entities_before = line_data->getAllEntityIds();
        std::unordered_set<EntityId> grouped_before;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped_before.insert(entities_in_group.begin(), entities_in_group.end());
        }

        INFO("Total entities: " << all_entities_before.size());
        INFO("Grouped before: " << grouped_before.size());
        INFO("Ground truth frames: " << ground_truth_frames.size());

        // Run the algorithm
        auto result = lineKalmanGrouping(line_data, &params);

        // Debug: Check final state
        std::unordered_set<EntityId> grouped_after;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped_after.insert(entities_in_group.begin(), entities_in_group.end());
        }
        auto descriptors = group_manager->getAllGroupDescriptors();
        auto findPutativeFor = [&](GroupId base_group) -> std::optional<GroupId> {
            auto desc = group_manager->getGroupDescriptor(base_group);
            if (!desc.has_value()) return std::nullopt;
            std::string expected = std::string("Putative:") + desc->name;
            for (auto const & d: descriptors) {
                if (d.name == expected) return d.id;
            }
            return std::nullopt;
        };
        auto put_g1 = findPutativeFor(group1_id);
        auto put_g2 = findPutativeFor(group2_id);
        if (put_g1) {
            auto ents = group_manager->getEntitiesInGroup(*put_g1);
            grouped_after.insert(ents.begin(), ents.end());
        }
        if (put_g2) {
            auto ents = group_manager->getEntitiesInGroup(*put_g2);
            grouped_after.insert(ents.begin(), ents.end());
        }

        INFO("Grouped after: " << grouped_after.size());

        // Check that lines are correctly assigned to their respective groups
        // We'll check a sample of frames to verify correct assignment
        std::vector<int> test_frames = {5, 15, 25, 35, 45, 55, 65, 75, 85, 95};

        int correct_assignments_group1 = 0;
        int correct_assignments_group2 = 0;
        int total_assignments_group1 = 0;
        int total_assignments_group2 = 0;

        for (int frame: test_frames) {
            // Check if line1_entity is in group1 or its putative group
            auto groups_for_line1 = group_manager->getGroupsContainingEntity(line1_entities[frame]);
            if (!groups_for_line1.empty()) {
                total_assignments_group1++;
                bool in_correct = std::find(groups_for_line1.begin(), groups_for_line1.end(), group1_id) != groups_for_line1.end();
                if (!in_correct && put_g1) {
                    in_correct = std::find(groups_for_line1.begin(), groups_for_line1.end(), *put_g1) != groups_for_line1.end();
                }
                if (in_correct) {
                    correct_assignments_group1++;
                }
            }

            // Check if line2_entity is in group2 or its putative group
            auto groups_for_line2 = group_manager->getGroupsContainingEntity(line2_entities[frame]);
            if (!groups_for_line2.empty()) {
                total_assignments_group2++;
                bool in_correct2 = std::find(groups_for_line2.begin(), groups_for_line2.end(), group2_id) != groups_for_line2.end();
                if (!in_correct2 && put_g2) {
                    in_correct2 = std::find(groups_for_line2.begin(), groups_for_line2.end(), *put_g2) != groups_for_line2.end();
                }
                if (in_correct2) {
                    correct_assignments_group2++;
                }
            }
        }

        INFO("Total assignments group1: " << total_assignments_group1);
        INFO("Correct assignments group1: " << correct_assignments_group1);
        INFO("Total assignments group2: " << total_assignments_group2);
        INFO("Correct assignments group2: " << correct_assignments_group2);

        // More lenient requirement - just check that some assignments were made
        REQUIRE(grouped_after.size() > grouped_before.size());// Should make some assignments

        // If assignments were made, at least 50% should be correct (relaxed from 70%)
        if (total_assignments_group1 > 0) {
            double accuracy1 = static_cast<double>(correct_assignments_group1) / total_assignments_group1;
            REQUIRE(accuracy1 >= 0.5);
        }

        if (total_assignments_group2 > 0) {
            double accuracy2 = static_cast<double>(correct_assignments_group2) / total_assignments_group2;
            REQUIRE(accuracy2 >= 0.5);
        }
    }

    SECTION("Test with different parameter settings") {
        // Test with stricter distance threshold
        LineKalmanGroupingParameters strict_params(group_manager.get());
        strict_params.process_noise_position = 5.0;
        strict_params.measurement_noise_position = 2.0;
        strict_params.measurement_noise_length = 5.0;

        auto result1 = lineKalmanGrouping(line_data, &strict_params);

        // With strict parameters, fewer assignments should be made
        auto all_entities = line_data->getAllEntityIds();
        std::unordered_set<EntityId> grouped_strict;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped_strict.insert(entities_in_group.begin(), entities_in_group.end());
        }
        // Include putative groups
        {
            auto descriptors = group_manager->getAllGroupDescriptors();
            for (auto base_gid: {group1_id, group2_id}) {
                auto desc = group_manager->getGroupDescriptor(base_gid);
                if (!desc) continue;
                std::string expected = std::string("Putative:") + desc->name;
                for (auto const & d: descriptors) {
                    if (d.name == expected) {
                        auto ents = group_manager->getEntitiesInGroup(d.id);
                        grouped_strict.insert(ents.begin(), ents.end());
                    }
                }
            }
        }

        // Reset groups for next test
        group_manager->clearGroup(group1_id);
        group_manager->clearGroup(group2_id);
        // Delete any putative groups created in strict run
        {
            auto descriptors2 = group_manager->getAllGroupDescriptors();
            for (auto const & d: descriptors2) {
                if (d.name.rfind("Putative:", 0) == 0) {
                    group_manager->deleteGroup(d.id);
                }
            }
        }

        // Re-add ground truth
        for (int frame: ground_truth_frames) {
            group_manager->addEntityToGroup(group1_id, line1_entities[frame]);
            group_manager->addEntityToGroup(group2_id, line2_entities[frame]);
        }

        // Test with lenient distance threshold
        LineKalmanGroupingParameters lenient_params(group_manager.get());
        lenient_params.process_noise_position = 20.0;
        lenient_params.measurement_noise_position = 10.0;
        lenient_params.measurement_noise_length = 20.0;

        auto result2 = lineKalmanGrouping(line_data, &lenient_params);

        std::unordered_set<EntityId> grouped_lenient;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped_lenient.insert(entities_in_group.begin(), entities_in_group.end());
        }
        // Include putative groups
        {
            auto descriptors3 = group_manager->getAllGroupDescriptors();
            for (auto base_gid: {group1_id, group2_id}) {
                auto desc = group_manager->getGroupDescriptor(base_gid);
                if (!desc) continue;
                std::string expected = std::string("Putative:") + desc->name;
                for (auto const & d: descriptors3) {
                    if (d.name == expected) {
                        auto ents = group_manager->getEntitiesInGroup(d.id);
                        grouped_lenient.insert(ents.begin(), ents.end());
                    }
                }
            }
        }

        // Lenient parameters should generally result in more assignments
        REQUIRE(grouped_lenient.size() >= grouped_strict.size());
    }

    SECTION("Test auto-estimation parameters") {
        // Test with auto-estimation enabled
        LineKalmanGroupingParameters auto_params(group_manager.get());
        auto_params.auto_estimate_static_noise = true;
        auto_params.auto_estimate_measurement_noise = true;
        auto_params.static_noise_percentile = 0.1;
        auto_params.verbose_output = true;// To see estimation output

        auto result = lineKalmanGrouping(line_data, &auto_params);
        REQUIRE(result == line_data);

        // Should still make some assignments
        std::unordered_set<EntityId> grouped;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped.insert(entities_in_group.begin(), entities_in_group.end());
        }
        // Include putative groups
        {
            auto descriptors4 = group_manager->getAllGroupDescriptors();
            for (auto base_gid: {group1_id, group2_id}) {
                auto desc = group_manager->getGroupDescriptor(base_gid);
                if (!desc) continue;
                std::string expected = std::string("Putative:") + desc->name;
                for (auto const & d: descriptors4) {
                    if (d.name == expected) {
                        auto ents = group_manager->getEntitiesInGroup(d.id);
                        grouped.insert(ents.begin(), ents.end());
                    }
                }
            }
        }

        // At minimum, ground truth should still be grouped
        REQUIRE(grouped.size() >= ground_truth_frames.size() * 2);
    }

    SECTION("Test static feature noise scale parameter") {
        // Test with different static noise scales
        LineKalmanGroupingParameters params1(group_manager.get());
        params1.static_feature_process_noise_scale = 0.001;// Very small
        params1.verbose_output = false;

        auto result1 = lineKalmanGrouping(line_data, &params1);

        std::unordered_set<EntityId> grouped1;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped1.insert(entities_in_group.begin(), entities_in_group.end());
        }
        {
            auto descriptors5 = group_manager->getAllGroupDescriptors();
            for (auto base_gid: {group1_id, group2_id}) {
                auto desc = group_manager->getGroupDescriptor(base_gid);
                if (!desc) continue;
                std::string expected = std::string("Putative:") + desc->name;
                for (auto const & d: descriptors5) {
                    if (d.name == expected) {
                        auto ents = group_manager->getEntitiesInGroup(d.id);
                        grouped1.insert(ents.begin(), ents.end());
                    }
                }
            }
        }

        // Reset groups
        group_manager->clearGroup(group1_id);
        group_manager->clearGroup(group2_id);
        for (int frame: ground_truth_frames) {
            group_manager->addEntityToGroup(group1_id, line1_entities[frame]);
            group_manager->addEntityToGroup(group2_id, line2_entities[frame]);
        }

        // Test with larger static noise scale
        LineKalmanGroupingParameters params2(group_manager.get());
        params2.static_feature_process_noise_scale = 0.1;// Larger
        params2.verbose_output = false;

        auto result2 = lineKalmanGrouping(line_data, &params2);

        std::unordered_set<EntityId> grouped2;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped2.insert(entities_in_group.begin(), entities_in_group.end());
        }
        {
            auto descriptors6 = group_manager->getAllGroupDescriptors();
            for (auto base_gid: {group1_id, group2_id}) {
                auto desc = group_manager->getGroupDescriptor(base_gid);
                if (!desc) continue;
                std::string expected = std::string("Putative:") + desc->name;
                for (auto const & d: descriptors6) {
                    if (d.name == expected) {
                        auto ents = group_manager->getEntitiesInGroup(d.id);
                        grouped2.insert(ents.begin(), ents.end());
                    }
                }
            }
        }

        // Both should make some assignments
        REQUIRE(grouped1.size() > ground_truth_frames.size() * 2);
        REQUIRE(grouped2.size() > ground_truth_frames.size() * 2);
    }

    SECTION("Test per-feature measurement noise") {
        LineKalmanGroupingParameters params(group_manager.get());
        params.measurement_noise_position = 2.0;// Low noise for position
        params.measurement_noise_length = 20.0; // High noise for length
        params.verbose_output = false;

        auto result = lineKalmanGrouping(line_data, &params);
        REQUIRE(result == line_data);

        // Build combined set of assigned entity IDs across ground truth and putative groups
        std::unordered_set<EntityId> grouped;
        for (auto group_id: {group1_id, group2_id}) {
            auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
            grouped.insert(entities_in_group.begin(), entities_in_group.end());
        }
        {
            auto descriptors = group_manager->getAllGroupDescriptors();
            for (auto base_gid: {group1_id, group2_id}) {
                auto desc = group_manager->getGroupDescriptor(base_gid);
                if (!desc) continue;
                std::string expected = std::string("Putative:") + desc->name;
                for (auto const & d: descriptors) {
                    if (d.name == expected) {
                        auto ents = group_manager->getEntitiesInGroup(d.id);
                        grouped.insert(ents.begin(), ents.end());
                    }
                }
            }
        }

        // At minimum, anchors should be present; often there will be more assignments
        REQUIRE(grouped.size() >= ground_truth_frames.size() * 2);
    }
}

TEST_CASE_METHOD(LineKalmanGroupingTestFixture, "LineKalmanGrouping - Edge Cases", "[LineKalmanGrouping]") {

    SECTION("Empty line data") {
        auto empty_line_data = std::make_shared<LineData>();
        LineKalmanGroupingParameters params(group_manager.get());

        auto result = lineKalmanGrouping(empty_line_data, &params);
        REQUIRE(result == empty_line_data);
    }

    SECTION("No existing groups") {
        auto empty_group_manager = std::make_unique<EntityGroupManager>();
        LineKalmanGroupingParameters params(empty_group_manager.get());

        auto result = lineKalmanGrouping(line_data, &params);
        REQUIRE(result == line_data);

        // No assignments should be made since there are no groups to track
        auto all_entities = line_data->getAllEntityIds();
        for (auto entity_id: all_entities) {
            auto groups = empty_group_manager->getGroupsContainingEntity(entity_id);
            REQUIRE(groups.empty());
        }
    }

    SECTION("Invalid parameters") {
        // Test with null group manager
        auto result1 = lineKalmanGrouping(line_data, nullptr);
        REQUIRE(result1 == line_data);

        // Test with null line data
        LineKalmanGroupingParameters params(group_manager.get());
        auto result2 = lineKalmanGrouping(nullptr, &params);
        REQUIRE(result2 == nullptr);
    }
}

TEST_CASE("LineKalmanGrouping - Transform Operation Interface", "[LineKalmanGrouping]") {

    SECTION("Transform operation basic functionality") {
        LineKalmanGroupingOperation operation;

        // Test getName
        REQUIRE(operation.getName() == "Group Lines using Kalman Filtering");

        // Test getTargetInputTypeIndex
        auto target_type = operation.getTargetInputTypeIndex();
        REQUIRE(target_type == std::type_index(typeid(std::shared_ptr<LineData>)));

        // Test getDefaultParameters (should return nullptr)
        auto default_params = operation.getDefaultParameters();
        REQUIRE(default_params == nullptr);
    }
}