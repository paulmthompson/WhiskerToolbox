#include "transforms/Lines/Line_Group_To_Intervals/line_group_to_intervals.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "fixtures/entity_id.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <vector>

/**
 * @brief Test fixture for Line Group to Intervals transformation
 * 
 * This fixture creates a scenario with multiple groups of lines across frames
 * to test the group-to-interval conversion algorithm.
 */
class LineGroupToIntervalsTestFixture {
protected:
    LineGroupToIntervalsTestFixture() {
        // Create DataManager and initialize components
        data_manager = std::make_unique<DataManager>();
        
        // Create EntityGroupManager
        group_manager = std::make_unique<EntityGroupManager>();

        // Create LineData with entity tracking enabled
        line_data = std::make_shared<LineData>();
        line_data->setIdentityContext("test_lines", data_manager->getEntityRegistry());

        // Create three groups for testing
        // Group 1: "Whisker A" - will be present in frames 0-4, 10-14
        // Group 2: "Whisker B" - will be present in frames 5-9, 15-19
        // Group 3: "Whisker C" - will be present in frames 0-9, 20-24
        group_a_id = group_manager->createGroup("Whisker A", "First whisker group");
        group_b_id = group_manager->createGroup("Whisker B", "Second whisker group");
        group_c_id = group_manager->createGroup("Whisker C", "Third whisker group");

        // Helper to create a simple line
        auto create_line = [](float base_y) -> Line2D {
            Line2D line;
            line.push_back(Point2D<float>{10.0f, base_y});
            line.push_back(Point2D<float>{20.0f, base_y + 5.0f});
            line.push_back(Point2D<float>{30.0f, base_y + 10.0f});
            return line;
        };

        // Create test data
        // Frames 0-4: Group A only
        for (int frame = 0; frame < 5; ++frame) {
            line_data->addAtTime(TimeFrameIndex(frame), create_line(10.0f), NotifyObservers::No);
        }

        // Frames 5-9: Group B and Group C
        for (int frame = 5; frame < 10; ++frame) {
            line_data->addAtTime(TimeFrameIndex(frame), create_line(20.0f), NotifyObservers::No); // Group B
            line_data->addAtTime(TimeFrameIndex(frame), create_line(30.0f), NotifyObservers::No); // Group C
        }

        // Frames 10-14: Group A only
        for (int frame = 10; frame < 15; ++frame) {
            line_data->addAtTime(TimeFrameIndex(frame), create_line(10.0f), NotifyObservers::No);
        }

        // Frames 15-19: Group B only
        for (int frame = 15; frame < 20; ++frame) {
            line_data->addAtTime(TimeFrameIndex(frame), create_line(20.0f), NotifyObservers::No);
        }

        // Frames 20-24: Group C only
        for (int frame = 20; frame < 25; ++frame) {
            line_data->addAtTime(TimeFrameIndex(frame), create_line(30.0f), NotifyObservers::No);
        }

        // Now assign entities to groups
        // Group A: frames 0-4 (indices 0) and 10-14 (indices 0)
        for (int frame = 0; frame < 5; ++frame) {
            auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entity_ids.size() == 1);
            group_manager->addEntityToGroup(group_a_id, entity_ids[0]);
        }
        for (int frame = 10; frame < 15; ++frame) {
            auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entity_ids.size() == 1);
            group_manager->addEntityToGroup(group_a_id, entity_ids[0]);
        }

        // Group B: frames 5-9 (indices 0) and 15-19 (indices 0)
        for (int frame = 5; frame < 10; ++frame) {
            auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entity_ids.size() == 2);
            group_manager->addEntityToGroup(group_b_id, entity_ids[0]);
        }
        for (int frame = 15; frame < 20; ++frame) {
            auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entity_ids.size() == 1);
            group_manager->addEntityToGroup(group_b_id, entity_ids[0]);
        }

        // Group C: frames 5-9 (indices 1) and 20-24 (indices 0)
        for (int frame = 5; frame < 10; ++frame) {
            auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entity_ids.size() == 2);
            group_manager->addEntityToGroup(group_c_id, entity_ids[1]);
        }
        for (int frame = 20; frame < 25; ++frame) {
            auto entity_ids = line_data->getEntityIdsAtTime(TimeFrameIndex(frame));
            REQUIRE(entity_ids.size() == 1);
            group_manager->addEntityToGroup(group_c_id, entity_ids[0]);
        }
    }

    std::unique_ptr<DataManager> data_manager;
    std::unique_ptr<EntityGroupManager> group_manager;
    std::shared_ptr<LineData> line_data;

    GroupId group_a_id;
    GroupId group_b_id;
    GroupId group_c_id;
};

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Basic Functionality", 
                 "[LineGroupToIntervals]") {

    SECTION("Test fixture setup is correct") {
        // Verify we have the expected data structure
        REQUIRE(line_data != nullptr);
        REQUIRE(group_manager != nullptr);

        // Verify groups exist
        REQUIRE(group_manager->hasGroup(group_a_id));
        REQUIRE(group_manager->hasGroup(group_b_id));
        REQUIRE(group_manager->hasGroup(group_c_id));

        // Verify group sizes
        REQUIRE(group_manager->getGroupSize(group_a_id) == 10); // frames 0-4, 10-14
        REQUIRE(group_manager->getGroupSize(group_b_id) == 10); // frames 5-9, 15-19
        REQUIRE(group_manager->getGroupSize(group_c_id) == 10); // frames 5-9, 20-24

        // Verify total entities
        auto all_entities = get_all_entity_ids(*line_data);
        REQUIRE(all_entities.size() == 30); // 5*1 + 5*2 + 5*1 + 5*1 + 5*1
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Track Presence", 
                 "[LineGroupToIntervals]") {

    SECTION("Track presence of Group A (discontinuous)") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id;
        params.track_presence = true;

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two separate intervals

        // First interval: frames 0-4
        REQUIRE(intervals[0].start == 0);
        REQUIRE(intervals[0].end == 4);

        // Second interval: frames 10-14
        REQUIRE(intervals[1].start == 10);
        REQUIRE(intervals[1].end == 14);
    }

    SECTION("Track presence of Group B (discontinuous)") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_b_id;
        params.track_presence = true;

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two separate intervals

        // First interval: frames 5-9
        REQUIRE(intervals[0].start == 5);
        REQUIRE(intervals[0].end == 9);

        // Second interval: frames 15-19
        REQUIRE(intervals[1].start == 15);
        REQUIRE(intervals[1].end == 19);
    }

    SECTION("Track presence of Group C (discontinuous)") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_c_id;
        params.track_presence = true;

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two separate intervals

        // First interval: frames 5-9
        REQUIRE(intervals[0].start == 5);
        REQUIRE(intervals[0].end == 9);

        // Second interval: frames 20-24
        REQUIRE(intervals[1].start == 20);
        REQUIRE(intervals[1].end == 24);
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Track Absence", 
                 "[LineGroupToIntervals]") {

    SECTION("Track absence of Group A") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id;
        params.track_presence = false; // Track absence

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 2); // Two gaps

        // First gap: frames 5-9
        REQUIRE(intervals[0].start == 5);
        REQUIRE(intervals[0].end == 9);

        // Second gap: frames 15-24
        REQUIRE(intervals[1].start == 15);
        REQUIRE(intervals[1].end == 24);
    }

    SECTION("Track absence of Group B") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_b_id;
        params.track_presence = false; // Track absence

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 3); // Three gaps

        // First gap: frames 0-4
        REQUIRE(intervals[0].start == 0);
        REQUIRE(intervals[0].end == 4);

        // Second gap: frames 10-14
        REQUIRE(intervals[1].start == 10);
        REQUIRE(intervals[1].end == 14);

        // Third gap: frames 20-24
        REQUIRE(intervals[2].start == 20);
        REQUIRE(intervals[2].end == 24);
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Minimum Interval Length", 
                 "[LineGroupToIntervals]") {

    SECTION("Filter short intervals with min_interval_length = 6") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id; // Has intervals of length 5
        params.track_presence = true;
        params.min_interval_length = 6; // Filter out intervals shorter than 6

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.empty()); // Both intervals are length 5, should be filtered out
    }

    SECTION("Allow intervals with min_interval_length = 5") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id;
        params.track_presence = true;
        params.min_interval_length = 5; // Exact match

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 2); // Both intervals should pass
    }

    SECTION("Filter with min_interval_length on absence tracking") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_b_id;
        params.track_presence = false; // Track absence
        params.min_interval_length = 6; // Filter intervals < 6

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        // Group B absent in: frames 0-4 (5 frames), 10-14 (5 frames), 20-24 (5 frames)
        // All are length 5, so all should be filtered out
        REQUIRE(intervals.empty());
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Merge Gap Threshold", 
                 "[LineGroupToIntervals]") {

    SECTION("Merge intervals with gap_threshold = 6") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id; // Present in 0-4 and 10-14 (gap of 5)
        params.track_presence = true;
        params.merge_gap_threshold = 6; // Merge if gap <= 6

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 1); // Should be merged into one

        // Merged interval: frames 0-14
        REQUIRE(intervals[0].start == 0);
        REQUIRE(intervals[0].end == 14);
    }

    SECTION("Do not merge intervals with gap_threshold = 4") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id; // Gap of 5 between intervals
        params.track_presence = true;
        params.merge_gap_threshold = 4; // Don't merge if gap > 4

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 2); // Should remain separate
    }

    SECTION("Merge multiple gaps with high threshold") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_b_id; // Present in 5-9 and 15-19 (gap of 5)
        params.track_presence = true;
        params.merge_gap_threshold = 10; // Large threshold

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.size() == 1); // Should be merged

        // Merged interval: frames 5-19
        REQUIRE(intervals[0].start == 5);
        REQUIRE(intervals[0].end == 19);
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Combined Filters", 
                 "[LineGroupToIntervals]") {

    SECTION("Merge then filter") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_c_id; // Present in 5-9 and 20-24
        params.track_presence = true;
        params.merge_gap_threshold = 15; // Merge (gap is 10)
        params.min_interval_length = 20; // Then filter

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        // After merge: 5-24 (length 20)
        // After filter: length 20 passes
        REQUIRE(intervals.size() == 1);
        REQUIRE(intervals[0].start == 5);
        REQUIRE(intervals[0].end == 24);
    }

    SECTION("Merge then filter out") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_c_id;
        params.track_presence = true;
        params.merge_gap_threshold = 15; // Merge
        params.min_interval_length = 21; // Filter out (length is 20)

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        
        auto intervals = result->getDigitalIntervalSeries();
        REQUIRE(intervals.empty()); // Should be filtered out
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Edge Cases", 
                 "[LineGroupToIntervals]") {

    SECTION("Empty line data") {
        auto empty_line_data = std::make_shared<LineData>();
        empty_line_data->setIdentityContext("empty", data_manager->getEntityRegistry());

        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id;

        auto result = lineGroupToIntervals(empty_line_data, &params);

        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }

    SECTION("Null line data") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id;

        auto result = lineGroupToIntervals(nullptr, &params);

        REQUIRE(result == nullptr);
    }

    SECTION("Null group manager") {
        LineGroupToIntervalsParameters params;
        params.group_manager = nullptr;
        params.target_group_id = group_a_id;

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result == nullptr);
    }

    SECTION("Invalid group ID") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = 9999; // Non-existent group

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result == nullptr);
    }

    SECTION("Empty group") {
        auto empty_group = group_manager->createGroup("Empty Group", "No entities");
        
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = empty_group;

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().empty());
    }

    SECTION("Zero target_group_id") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = 0;

        auto result = lineGroupToIntervals(line_data, &params);

        REQUIRE(result == nullptr);
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Operation Interface", 
                 "[LineGroupToIntervals]") {

    SECTION("Operation getName") {
        LineGroupToIntervalsOperation op;
        REQUIRE(op.getName() == "Line Group to Digital Intervals");
    }

    SECTION("Operation canApply with LineData") {
        LineGroupToIntervalsOperation op;
        DataTypeVariant variant = line_data;
        REQUIRE(op.canApply(variant));
    }

    SECTION("Operation canApply with wrong type") {
        LineGroupToIntervalsOperation op;
        auto point_data = std::make_shared<PointData>();
        DataTypeVariant variant = point_data;
        REQUIRE_FALSE(op.canApply(variant));
    }

    SECTION("Operation getDefaultParameters") {
        LineGroupToIntervalsOperation op;
        auto params = op.getDefaultParameters();
        REQUIRE(params != nullptr);
        
        auto* typed_params = dynamic_cast<LineGroupToIntervalsParameters*>(params.get());
        REQUIRE(typed_params != nullptr);
        REQUIRE(typed_params->group_manager == nullptr);
        REQUIRE(typed_params->target_group_id == 0);
        REQUIRE(typed_params->track_presence == true);
    }

    SECTION("Operation execute") {
        LineGroupToIntervalsOperation op;
        
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id;
        params.track_presence = true;

        DataTypeVariant input_variant = line_data;
        auto result_variant = op.execute(input_variant, &params);

        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result_variant));
        
        auto result = std::get<std::shared_ptr<DigitalIntervalSeries>>(result_variant);
        REQUIRE(result != nullptr);
        REQUIRE(result->getDigitalIntervalSeries().size() == 2);
    }

    SECTION("Operation execute with progress callback") {
        LineGroupToIntervalsOperation op;
        
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_b_id;
        params.track_presence = true;

        int last_progress = -1;
        auto progress_callback = [&last_progress](int progress) {
            REQUIRE(progress >= 0);
            REQUIRE(progress <= 100);
            REQUIRE(progress >= last_progress); // Progress should be monotonic
            last_progress = progress;
        };

        DataTypeVariant input_variant = line_data;
        auto result_variant = op.execute(input_variant, &params, progress_callback);

        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(result_variant));
        REQUIRE(last_progress == 100); // Should reach 100%
    }
}

TEST_CASE_METHOD(LineGroupToIntervalsTestFixture, 
                 "Data Transform: LineGroupToIntervals - Verify Interval Correctness", 
                 "[LineGroupToIntervals]") {

    SECTION("Check isEventAtTime for presence tracking") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id; // Present in 0-4, 10-14
        params.track_presence = true;

        auto result = lineGroupToIntervals(line_data, &params);
        REQUIRE(result != nullptr);

        // Check frames in first interval
        for (int i = 0; i <= 4; ++i) {
            REQUIRE(result->isEventAtTime(TimeFrameIndex(i)));
        }

        // Check frames in gap
        for (int i = 5; i <= 9; ++i) {
            REQUIRE_FALSE(result->isEventAtTime(TimeFrameIndex(i)));
        }

        // Check frames in second interval
        for (int i = 10; i <= 14; ++i) {
            REQUIRE(result->isEventAtTime(TimeFrameIndex(i)));
        }

        // Check frames after
        for (int i = 15; i <= 24; ++i) {
            REQUIRE_FALSE(result->isEventAtTime(TimeFrameIndex(i)));
        }
    }

    SECTION("Check isEventAtTime for absence tracking") {
        LineGroupToIntervalsParameters params;
        params.group_manager = group_manager.get();
        params.target_group_id = group_a_id; // Present in 0-4, 10-14
        params.track_presence = false; // Track absence

        auto result = lineGroupToIntervals(line_data, &params);
        REQUIRE(result != nullptr);

        // Check frames in first interval (should be absent)
        for (int i = 0; i <= 4; ++i) {
            REQUIRE_FALSE(result->isEventAtTime(TimeFrameIndex(i)));
        }

        // Check frames in first absence interval
        for (int i = 5; i <= 9; ++i) {
            REQUIRE(result->isEventAtTime(TimeFrameIndex(i)));
        }

        // Check frames in second interval (should be absent)
        for (int i = 10; i <= 14; ++i) {
            REQUIRE_FALSE(result->isEventAtTime(TimeFrameIndex(i)));
        }

        // Check frames in second absence interval
        for (int i = 15; i <= 24; ++i) {
            REQUIRE(result->isEventAtTime(TimeFrameIndex(i)));
        }
    }
}
