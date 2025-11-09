#include "transforms/Lines/Line_Index_Grouping/line_index_grouping.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "fixtures/entity_id.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <vector>

/**
 * @brief Test fixture for Line Index Grouping functionality
 * 
 * This fixture creates a scenario with varying numbers of lines across frames
 * to test the index-based grouping algorithm.
 */
class LineIndexGroupingTestFixture {
protected:
    LineIndexGroupingTestFixture() {
        // Initialize DataManager
        data_manager = std::make_unique<DataManager>();

        // Create EntityGroupManager
        group_manager = std::make_unique<EntityGroupManager>();

        // Create a TimeFrame (20 frames - smaller than Kalman test for simplicity)
        auto timeValues = std::vector<int>();
        for (int i = 0; i < 20; ++i) {
            timeValues.push_back(i);
        }

        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);

        // Create LineData
        data_manager->setData<LineData>("test_lines", TimeKey("test_timeframe"));
        line_data = data_manager->getData<LineData>("test_lines");

        generateTestData();
    }

    void generateTestData() {
        // Generate test data with varying numbers of lines per frame
        // Frame 0-4: 3 lines each
        // Frame 5-9: 5 lines each (maximum)
        // Frame 10-14: 4 lines each
        // Frame 15-19: 2 lines each

        for (int frame = 0; frame < 20; ++frame) {
            int num_lines = 3; // default
            
            if (frame >= 5 && frame < 10) {
                num_lines = 5; // maximum
            } else if (frame >= 10 && frame < 15) {
                num_lines = 4;
            } else if (frame >= 15) {
                num_lines = 2;
            }

            for (int line_idx = 0; line_idx < num_lines; ++line_idx) {
                Line2D line;
                
                // Create a simple horizontal line
                // Each line at a different y-position based on its index
                float y_pos = 100.0f + line_idx * 50.0f;
                float x_start = 10.0f + frame * 5.0f;
                
                for (int i = 0; i < 10; ++i) {
                    line.push_back({x_start + i * 2.0f, y_pos});
                }
                
                line_data->addAtTime(TimeFrameIndex(frame), line, false);
            }
        }

        // Store entity IDs for verification
        for (int frame = 0; frame < 20; ++frame) {
            auto entities_at_frame = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            for (auto entity_id : entities_at_frame) {
                frame_entities[frame].push_back(entity_id);
            }
        }
    }

    std::unique_ptr<DataManager> data_manager;
    std::unique_ptr<EntityGroupManager> group_manager;
    std::shared_ptr<LineData> line_data;

    std::map<int, std::vector<EntityId>> frame_entities; // frame -> entity_ids
};

TEST_CASE_METHOD(LineIndexGroupingTestFixture, "Data Transform: LineIndexGrouping - Basic Functionality", "[LineIndexGrouping]") {

    SECTION("Test fixture setup is correct") {
        // Verify we have the expected data structure
        REQUIRE(line_data != nullptr);
        REQUIRE(group_manager != nullptr);

        // Check frame 0-4: should have 3 lines each
        for (int frame = 0; frame < 5; ++frame) {
            auto entities = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entities.size() == 3);
        }

        // Check frame 5-9: should have 5 lines each (maximum)
        for (int frame = 5; frame < 10; ++frame) {
            auto entities = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entities.size() == 5);
        }

        // Check frame 10-14: should have 4 lines each
        for (int frame = 10; frame < 15; ++frame) {
            auto entities = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entities.size() == 4);
        }

        // Check frame 15-19: should have 2 lines each
        for (int frame = 15; frame < 20; ++frame) {
            auto entities = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entities.size() == 2);
        }

        // Total entities should be: 5*3 + 5*5 + 5*4 + 5*2 = 15 + 25 + 20 + 10 = 70
        auto all_entities = get_all_entity_ids(*line_data);
        REQUIRE(all_entities.size() == 70);
    }
}

TEST_CASE_METHOD(LineIndexGroupingTestFixture, "Data Transform: LineIndexGrouping - Algorithm Test", "[LineIndexGrouping]") {

    SECTION("Basic grouping with default parameters") {
        // Create parameters
        LineIndexGroupingParameters params(group_manager.get());
        params.group_name_prefix = "Line";

        // Run the algorithm
        auto result = lineIndexGrouping(line_data, &params);

        // Should return the same line_data pointer
        REQUIRE(result == line_data);

        // Should create 5 groups (based on maximum of 5 lines at frames 5-9)
        auto all_groups = group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 5);

        // Verify group names
        for (size_t i = 0; i < 5; ++i) {
            std::string expected_name = "Line " + std::to_string(i);
            bool found = false;
            for (auto group_id : all_groups) {
                auto descriptor = group_manager->getGroupDescriptor(group_id);
                if (descriptor && descriptor->name == expected_name) {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }

    SECTION("Verify correct entity assignment by index") {
        LineIndexGroupingParameters params(group_manager.get());
        params.group_name_prefix = "TestGroup";

        auto result = lineIndexGrouping(line_data, &params);

        auto all_groups = group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 5);

        // Find groups by name
        std::map<int, GroupId> index_to_group_id;
        for (int i = 0; i < 5; ++i) {
            std::string expected_name = "TestGroup " + std::to_string(i);
            for (auto group_id : all_groups) {
                auto descriptor = group_manager->getGroupDescriptor(group_id);
                if (descriptor && descriptor->name == expected_name) {
                    index_to_group_id[i] = group_id;
                    break;
                }
            }
        }
        REQUIRE(index_to_group_id.size() == 5);

        // Verify that entities at index 0 are in group 0, index 1 in group 1, etc.
        // Frame 5 has 5 lines, so all indices should be present
        auto entities_frame_5 = line_data->getEntityIdsAtTime(TimeFrameIndex(5));
        REQUIRE(entities_frame_5.size() == 5);

        for (size_t idx = 0; idx < 5; ++idx) {
            EntityId entity_at_idx = entities_frame_5[idx];
            auto groups = group_manager->getGroupsContainingEntity(entity_at_idx);
            
            // Should be in exactly one group
            REQUIRE(groups.size() == 1);
            
            // Should be in the group corresponding to its index
            REQUIRE(groups[0] == index_to_group_id[idx]);
        }

        // Verify group sizes
        // Group 0: should have entities from all 20 frames (every frame has at least 1 line)
        REQUIRE(group_manager->getGroupSize(index_to_group_id[0]) == 20);
        
        // Group 1: should have entities from all 20 frames (every frame has at least 2 lines)
        REQUIRE(group_manager->getGroupSize(index_to_group_id[1]) == 20);
        
        // Group 2: should have entities from frames 0-14 (15 frames with at least 3 lines)
        REQUIRE(group_manager->getGroupSize(index_to_group_id[2]) == 15);
        
        // Group 3: should have entities from frames 5-14 (10 frames with at least 4 lines)
        REQUIRE(group_manager->getGroupSize(index_to_group_id[3]) == 10);
        
        // Group 4: should have entities from frames 5-9 (5 frames with 5 lines)
        REQUIRE(group_manager->getGroupSize(index_to_group_id[4]) == 5);
    }

    SECTION("Custom group name prefix") {
        LineIndexGroupingParameters params(group_manager.get());
        params.group_name_prefix = "Whisker";

        auto result = lineIndexGrouping(line_data, &params);

        auto all_groups = group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 5);

        // Verify custom names
        for (size_t i = 0; i < 5; ++i) {
            std::string expected_name = "Whisker " + std::to_string(i);
            bool found = false;
            for (auto group_id : all_groups) {
                auto descriptor = group_manager->getGroupDescriptor(group_id);
                if (descriptor && descriptor->name == expected_name) {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }

    SECTION("Custom group description template") {
        LineIndexGroupingParameters params(group_manager.get());
        params.group_name_prefix = "Object";
        params.group_description_template = "Object number {} detected at index {}";

        auto result = lineIndexGrouping(line_data, &params);

        auto all_groups = group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 5);

        // Verify descriptions
        for (size_t i = 0; i < 5; ++i) {
            std::string expected_name = "Object " + std::to_string(i);
            std::string expected_desc = "Object number " + std::to_string(i) + " detected at index " + std::to_string(i);
            
            bool found = false;
            for (auto group_id : all_groups) {
                auto descriptor = group_manager->getGroupDescriptor(group_id);
                if (descriptor && descriptor->name == expected_name) {
                    REQUIRE(descriptor->description == expected_desc);
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }

    SECTION("Clear existing groups before grouping") {
        // Create some pre-existing groups
        auto pre_group1 = group_manager->createGroup("PreExisting1", "Should be deleted");
        auto pre_group2 = group_manager->createGroup("PreExisting2", "Should be deleted");
        
        REQUIRE(group_manager->getAllGroupIds().size() == 2);

        LineIndexGroupingParameters params(group_manager.get());
        params.clear_existing_groups = true;

        auto result = lineIndexGrouping(line_data, &params);

        // Should have exactly 5 groups (pre-existing ones should be gone)
        auto all_groups = group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 5);

        // Pre-existing groups should not exist
        REQUIRE(!group_manager->hasGroup(pre_group1));
        REQUIRE(!group_manager->hasGroup(pre_group2));
    }

    SECTION("Do not clear existing groups") {
        // Create some pre-existing groups
        auto pre_group1 = group_manager->createGroup("PreExisting1", "Should remain");
        auto pre_group2 = group_manager->createGroup("PreExisting2", "Should remain");
        
        REQUIRE(group_manager->getAllGroupIds().size() == 2);

        LineIndexGroupingParameters params(group_manager.get());
        params.clear_existing_groups = false; // default

        auto result = lineIndexGrouping(line_data, &params);

        // Should have 7 groups total (2 pre-existing + 5 new)
        auto all_groups = group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 7);

        // Pre-existing groups should still exist
        REQUIRE(group_manager->hasGroup(pre_group1));
        REQUIRE(group_manager->hasGroup(pre_group2));
    }
}

TEST_CASE_METHOD(LineIndexGroupingTestFixture, "Data Transform: LineIndexGrouping - Edge Cases", "[LineIndexGrouping]") {

    SECTION("Empty line data") {
        auto empty_line_data = std::make_shared<LineData>();
        LineIndexGroupingParameters params(group_manager.get());

        auto result = lineIndexGrouping(empty_line_data, &params);
        REQUIRE(result == empty_line_data);

        // No groups should be created
        REQUIRE(group_manager->getAllGroupIds().empty());
    }

    SECTION("Single line at single timestamp") {
        // Create DataManager for proper entity ID management
        auto data_manager = std::make_unique<DataManager>();
        
        // Create a TimeFrame
        std::vector<int> timeValues = {0};
        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);
        
        // Create LineData through DataManager
        data_manager->setData<LineData>("single_line", TimeKey("test_timeframe"));
        auto single_line_data = data_manager->getData<LineData>("single_line");
        
        Line2D line;
        line.push_back({0.0f, 0.0f});
        line.push_back({10.0f, 10.0f});
        single_line_data->addAtTime(TimeFrameIndex(0), line, false);

        auto single_group_manager = std::make_unique<EntityGroupManager>();
        LineIndexGroupingParameters params(single_group_manager.get());

        auto result = lineIndexGrouping(single_line_data, &params);
        REQUIRE(result == single_line_data);

        // Should create exactly 1 group
        auto all_groups = single_group_manager->getAllGroupIds();
        REQUIRE(all_groups.size() == 1);

        // The single entity should be in the group
        auto all_entities = get_all_entity_ids(*single_line_data);
        REQUIRE(all_entities.size() == 1);
        
        auto groups = single_group_manager->getGroupsContainingEntity(all_entities[0]);
        REQUIRE(groups.size() == 1);
        REQUIRE(groups[0] == all_groups[0]);
    }

    SECTION("Invalid parameters - null group manager") {
        LineIndexGroupingParameters params; // No group manager
        REQUIRE(!params.hasValidGroupManager());

        auto result = lineIndexGrouping(line_data, &params);
        REQUIRE(result == line_data);

        // No groups should be created (group_manager is not set)
    }

    SECTION("Invalid parameters - null line data") {
        LineIndexGroupingParameters params(group_manager.get());

        auto result = lineIndexGrouping(nullptr, &params);
        REQUIRE(result == nullptr);
    }

    SECTION("Invalid parameters - null params") {
        auto result = lineIndexGrouping(line_data, nullptr);
        REQUIRE(result == line_data);
    }
}

TEST_CASE("Data Transform: LineIndexGrouping - Transform Operation Interface", "[LineIndexGrouping]") {

    SECTION("Transform operation basic functionality") {
        LineIndexGroupingOperation operation;

        // Test getName
        REQUIRE(operation.getName() == "Group Lines by Index");

        // Test getTargetInputTypeIndex
        auto target_type = operation.getTargetInputTypeIndex();
        REQUIRE(target_type == std::type_index(typeid(std::shared_ptr<LineData>)));

        // Test canApply with correct type
        auto test_line_data = std::make_shared<LineData>();
        DataTypeVariant variant = test_line_data;
        REQUIRE(operation.canApply(variant));

        // Test canApply with incorrect type (use PointData instead of LineData)
        auto test_point_data = std::make_shared<PointData>();
        DataTypeVariant wrong_variant = test_point_data;
        REQUIRE(!operation.canApply(wrong_variant));

        // Test getDefaultParameters
        auto default_params = operation.getDefaultParameters();
        REQUIRE(default_params != nullptr);
        
        // Cast to the specific parameter type
        auto index_params = dynamic_cast<LineIndexGroupingParameters*>(default_params.get());
        REQUIRE(index_params != nullptr);
        REQUIRE(!index_params->hasValidGroupManager()); // Should have null group manager initially
        
        // Test that we can set the group manager
        auto mock_group_manager = std::make_unique<EntityGroupManager>();
        index_params->setGroupManager(mock_group_manager.get());
        REQUIRE(index_params->hasValidGroupManager());
        REQUIRE(index_params->getGroupManager() == mock_group_manager.get());
    }

    SECTION("Execute through operation interface") {
        LineIndexGroupingOperation operation;
        
        // Create DataManager for proper entity ID management
        auto data_manager = std::make_unique<DataManager>();
        
        // Create a TimeFrame
        std::vector<int> timeValues;
        for (int i = 0; i < 5; ++i) {
            timeValues.push_back(i);
        }
        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);
        
        // Create LineData through DataManager
        data_manager->setData<LineData>("operation_test_lines", TimeKey("test_timeframe"));
        auto test_line_data = data_manager->getData<LineData>("operation_test_lines");
        
        // Add test data
        for (int frame = 0; frame < 5; ++frame) {
            for (int line_idx = 0; line_idx < 3; ++line_idx) {
                Line2D line;
                for (int i = 0; i < 5; ++i) {
                    line.push_back({static_cast<float>(i * 10), static_cast<float>(line_idx * 20)});
                }
                test_line_data->addAtTime(TimeFrameIndex(frame), line, false);
            }
        }

        // Create group manager and parameters
        auto group_manager = std::make_unique<EntityGroupManager>();
        auto params = std::make_unique<LineIndexGroupingParameters>(group_manager.get());
        params->group_name_prefix = "OperationTest";

        // Execute through operation interface
        DataTypeVariant input_variant = test_line_data;
        DataTypeVariant result_variant = operation.execute(input_variant, params.get());

        // Result should be LineData
        REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
        auto result_data = std::get<std::shared_ptr<LineData>>(result_variant);
        REQUIRE(result_data == test_line_data);

        // Should have created 3 groups
        REQUIRE(group_manager->getAllGroupIds().size() == 3);
    }

    SECTION("Execute with invalid parameters type") {
        LineIndexGroupingOperation operation;
        
        auto test_line_data = std::make_shared<LineData>();
        DataTypeVariant input_variant = test_line_data;

        // Use wrong parameter type (use base class)
        auto wrong_params = std::make_unique<TransformParametersBase>();

        // Should handle gracefully
        DataTypeVariant result_variant = operation.execute(input_variant, wrong_params.get());
        REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));
    }
}

TEST_CASE("Data Transform: LineIndexGrouping - Comprehensive Scenarios", "[LineIndexGrouping]") {

    SECTION("All frames have same number of lines") {
        // Create DataManager and EntityRegistry for proper entity ID management
        auto data_manager = std::make_unique<DataManager>();
        
        // Create a TimeFrame
        std::vector<int> timeValues;
        for (int i = 0; i < 10; ++i) {
            timeValues.push_back(i);
        }
        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);
        
        // Create LineData through DataManager
        data_manager->setData<LineData>("uniform_lines", TimeKey("test_timeframe"));
        auto uniform_data = data_manager->getData<LineData>("uniform_lines");
        
        // 10 frames, each with exactly 4 lines
        for (int frame = 0; frame < 10; ++frame) {
            for (int line_idx = 0; line_idx < 4; ++line_idx) {
                Line2D line;
                for (int i = 0; i < 5; ++i) {
                    line.push_back({static_cast<float>(i * 5), static_cast<float>(line_idx * 10)});
                }
                uniform_data->addAtTime(TimeFrameIndex(frame), line, false);
            }
        }

        auto group_manager = std::make_unique<EntityGroupManager>();
        LineIndexGroupingParameters params(group_manager.get());

        auto result = lineIndexGrouping(uniform_data, &params);

        // Should create 4 groups
        REQUIRE(group_manager->getAllGroupIds().size() == 4);

        // Each group should have exactly 10 entities (one per frame)
        for (auto group_id : group_manager->getAllGroupIds()) {
            REQUIRE(group_manager->getGroupSize(group_id) == 10);
        }
    }

    SECTION("Increasing line count over time") {
        // Create DataManager and EntityRegistry for proper entity ID management
        auto data_manager = std::make_unique<DataManager>();
        
        // Create a TimeFrame
        std::vector<int> timeValues;
        for (int i = 0; i < 5; ++i) {
            timeValues.push_back(i);
        }
        auto timeframe = std::make_shared<TimeFrame>(timeValues);
        data_manager->setTime(TimeKey("test_timeframe"), timeframe);
        
        // Create LineData through DataManager
        data_manager->setData<LineData>("increasing_lines", TimeKey("test_timeframe"));
        auto increasing_data = data_manager->getData<LineData>("increasing_lines");
        
        // Frame i has i+1 lines (1, 2, 3, 4, 5 lines)
        for (int frame = 0; frame < 5; ++frame) {
            for (int line_idx = 0; line_idx <= frame; ++line_idx) {
                Line2D line;
                for (int i = 0; i < 5; ++i) {
                    line.push_back({static_cast<float>(i), static_cast<float>(line_idx)});
                }
                increasing_data->addAtTime(TimeFrameIndex(frame), line, false);
            }
        }

        auto group_manager = std::make_unique<EntityGroupManager>();
        LineIndexGroupingParameters params(group_manager.get());

        auto result = lineIndexGrouping(increasing_data, &params);

        // Should create 5 groups (maximum from frame 4)
        REQUIRE(group_manager->getAllGroupIds().size() == 5);

        // Verify group sizes
        // Group 0: in all 5 frames
        // Group 1: in frames 1-4 (4 frames)
        // Group 2: in frames 2-4 (3 frames)
        // Group 3: in frames 3-4 (2 frames)
        // Group 4: in frame 4 only (1 frame)
        
        std::vector<GroupId> sorted_groups = group_manager->getAllGroupIds();
        std::sort(sorted_groups.begin(), sorted_groups.end(), 
                  [&](GroupId a, GroupId b) {
                      return group_manager->getGroupSize(a) > group_manager->getGroupSize(b);
                  });

        REQUIRE(group_manager->getGroupSize(sorted_groups[0]) == 5);
        REQUIRE(group_manager->getGroupSize(sorted_groups[1]) == 4);
        REQUIRE(group_manager->getGroupSize(sorted_groups[2]) == 3);
        REQUIRE(group_manager->getGroupSize(sorted_groups[3]) == 2);
        REQUIRE(group_manager->getGroupSize(sorted_groups[4]) == 1);
    }
}
