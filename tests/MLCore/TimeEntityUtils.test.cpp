
#include <catch2/catch_test_macros.hpp>

#include "entity_utils/TimeEntityUtils.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================

namespace {

/**
 * @brief Create a simple identity TimeFrame (index i → time i)
 */
std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    for (int i = 0; i < count; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a DataManager with a TimeFrame registered under given key
 */
std::shared_ptr<DataManager> makeDM(std::string const & time_key, int frame_count) {
    auto dm = std::make_shared<DataManager>();
    dm->setTime(TimeKey(time_key), makeTimeFrame(frame_count));
    return dm;
}

/**
 * @brief Build a vector of TimeFrameIndex from int values
 */
std::vector<TimeFrameIndex> makeFrames(std::vector<int64_t> const & values) {
    std::vector<TimeFrameIndex> frames;
    frames.reserve(values.size());
    for (auto v : values) {
        frames.emplace_back(v);
    }
    return frames;
}

/**
 * @brief Create a DigitalIntervalSeries with given interval bounds
 */
std::shared_ptr<DigitalIntervalSeries> makeIntervalSeries(
    std::vector<std::pair<int64_t, int64_t>> const & intervals_vec)
{
    auto series = std::make_shared<DigitalIntervalSeries>();
    for (auto const & [s, e] : intervals_vec) {
        series->addEvent(TimeFrameIndex(s), TimeFrameIndex(e));
    }
    return series;
}

/**
 * @brief Collect all intervals from a DigitalIntervalSeries into a vector
 */
std::vector<std::pair<int64_t, int64_t>> collectIntervals(
    DigitalIntervalSeries const & series)
{
    std::vector<std::pair<int64_t, int64_t>> result;
    for (auto iv : series.view()) {
        result.emplace_back(iv.interval.start, iv.interval.end);
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// registerTimeEntities
// ============================================================================

TEST_CASE("registerTimeEntities - basic registration",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto frames = makeFrames({5, 10, 15, 20});

    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    REQUIRE(ids.size() == 4);
    // All IDs should be distinct
    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t j = i + 1; j < ids.size(); ++j) {
            REQUIRE(ids[i] != ids[j]);
        }
    }
}

TEST_CASE("registerTimeEntities - idempotent re-registration",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto frames = makeFrames({10, 20, 30});

    auto ids1 = MLCore::registerTimeEntities(*dm, "camera", frames);
    auto ids2 = MLCore::registerTimeEntities(*dm, "camera", frames);

    REQUIRE(ids1.size() == ids2.size());
    for (std::size_t i = 0; i < ids1.size(); ++i) {
        REQUIRE(ids1[i] == ids2[i]);
    }
}

TEST_CASE("registerTimeEntities - empty span returns empty",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    std::vector<TimeFrameIndex> empty;

    auto ids = MLCore::registerTimeEntities(*dm, "camera", empty);

    REQUIRE(ids.empty());
}

TEST_CASE("registerTimeEntities - preserves input order",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    // Deliberately non-sorted input
    auto frames = makeFrames({50, 10, 90, 30});

    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    // Re-registering in a different order should return same IDs for same values
    auto frames_sorted = makeFrames({10, 30, 50, 90});
    auto ids_sorted = MLCore::registerTimeEntities(*dm, "camera", frames_sorted);

    // ids[0] was for frame 50, ids_sorted[2] is also for frame 50
    REQUIRE(ids[0] == ids_sorted[2]);
    REQUIRE(ids[1] == ids_sorted[0]);
    REQUIRE(ids[2] == ids_sorted[3]);
    REQUIRE(ids[3] == ids_sorted[1]);
}

TEST_CASE("registerTimeEntities - different time keys give different IDs",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    dm->setTime(TimeKey("neural"), makeTimeFrame(100));

    auto frames = makeFrames({10, 20});

    auto camera_ids = MLCore::registerTimeEntities(*dm, "camera", frames);
    auto neural_ids = MLCore::registerTimeEntities(*dm, "neural", frames);

    // Same time indices on different clocks should produce different entity IDs
    REQUIRE(camera_ids[0] != neural_ids[0]);
    REQUIRE(camera_ids[1] != neural_ids[1]);
}

TEST_CASE("registerTimeEntities - descriptors are correct",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto frames = makeFrames({42});

    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);
    REQUIRE(ids.size() == 1);

    auto desc = dm->getEntityRegistry()->get(ids[0]);
    REQUIRE(desc.has_value());
    REQUIRE(desc->data_key == "camera");
    REQUIRE(desc->kind == EntityKind::TimeEntity);
    REQUIRE(desc->time_value == 42);
    REQUIRE(desc->local_index == 0);
}

// ============================================================================
// intervalsToTimeEntities
// ============================================================================

TEST_CASE("intervalsToTimeEntities - single interval",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto series = makeIntervalSeries({{10, 14}});

    auto ids = MLCore::intervalsToTimeEntities(*dm, *series, "camera");

    // Interval [10, 14] → 5 entities (10, 11, 12, 13, 14)
    REQUIRE(ids.size() == 5);

    // IDs should be sorted by time value (ascending)
    for (std::size_t i = 0; i < ids.size(); ++i) {
        auto resolved = dm->resolveTimeEntity(ids[i]);
        REQUIRE(resolved.has_value());
        REQUIRE(resolved->second.getValue() == static_cast<int64_t>(10 + i));
    }
}

TEST_CASE("intervalsToTimeEntities - multiple disjoint intervals",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto series = makeIntervalSeries({{5, 7}, {20, 22}});

    auto ids = MLCore::intervalsToTimeEntities(*dm, *series, "camera");

    // [5,7] = 3 frames + [20,22] = 3 frames = 6 total
    REQUIRE(ids.size() == 6);

    // Sorted by time
    std::vector<int64_t> time_values;
    for (auto const & id : ids) {
        auto resolved = dm->resolveTimeEntity(id);
        REQUIRE(resolved.has_value());
        time_values.push_back(resolved->second.getValue());
    }
    REQUIRE(time_values == std::vector<int64_t>{5, 6, 7, 20, 21, 22});
}

TEST_CASE("intervalsToTimeEntities - overlapping intervals are deduplicated",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    // Intervals [10, 15] and [13, 18] overlap on frames 13, 14, 15
    auto series = makeIntervalSeries({{10, 15}, {13, 18}});

    auto ids = MLCore::intervalsToTimeEntities(*dm, *series, "camera");

    // Deduplicated: frames 10..18 = 9 unique values
    REQUIRE(ids.size() == 9);

    std::vector<int64_t> time_values;
    for (auto const & id : ids) {
        auto resolved = dm->resolveTimeEntity(id);
        REQUIRE(resolved.has_value());
        time_values.push_back(resolved->second.getValue());
    }
    REQUIRE(time_values == std::vector<int64_t>{10, 11, 12, 13, 14, 15, 16, 17, 18});
}

TEST_CASE("intervalsToTimeEntities - empty series returns empty",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto series = makeIntervalSeries({});

    auto ids = MLCore::intervalsToTimeEntities(*dm, *series, "camera");

    REQUIRE(ids.empty());
}

TEST_CASE("intervalsToTimeEntities - single-frame interval",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto series = makeIntervalSeries({{42, 42}});

    auto ids = MLCore::intervalsToTimeEntities(*dm, *series, "camera");

    REQUIRE(ids.size() == 1);
    auto resolved = dm->resolveTimeEntity(ids[0]);
    REQUIRE(resolved.has_value());
    REQUIRE(resolved->second.getValue() == 42);
}

TEST_CASE("intervalsToTimeEntities - idempotent with repeated calls",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto series = makeIntervalSeries({{10, 12}});

    auto ids1 = MLCore::intervalsToTimeEntities(*dm, *series, "camera");
    auto ids2 = MLCore::intervalsToTimeEntities(*dm, *series, "camera");

    REQUIRE(ids1.size() == ids2.size());
    for (std::size_t i = 0; i < ids1.size(); ++i) {
        REQUIRE(ids1[i] == ids2[i]);
    }
}

// ============================================================================
// timeEntitiesToIntervals
// ============================================================================

TEST_CASE("timeEntitiesToIntervals - contiguous frames produce single interval",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    // Register entities and put them in a group
    auto frames = makeFrames({10, 11, 12, 13, 14});
    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    GroupId gid = groups->createGroup("test_group");
    groups->addEntitiesToGroup(gid, ids);

    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result != nullptr);

    auto intervals = collectIntervals(*result);
    REQUIRE(intervals.size() == 1);
    REQUIRE(intervals[0] == std::make_pair(int64_t{10}, int64_t{14}));
}

TEST_CASE("timeEntitiesToIntervals - disjoint frames produce multiple intervals",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    // Frames with gaps: [5,6,7] and [20,21]
    auto frames = makeFrames({5, 6, 7, 20, 21});
    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    GroupId gid = groups->createGroup("test_group");
    groups->addEntitiesToGroup(gid, ids);

    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result != nullptr);

    auto intervals = collectIntervals(*result);
    REQUIRE(intervals.size() == 2);
    REQUIRE(intervals[0] == std::make_pair(int64_t{5}, int64_t{7}));
    REQUIRE(intervals[1] == std::make_pair(int64_t{20}, int64_t{21}));
}

TEST_CASE("timeEntitiesToIntervals - single frame produces single-frame interval",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    auto frames = makeFrames({42});
    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    GroupId gid = groups->createGroup("test_group");
    groups->addEntitiesToGroup(gid, ids);

    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result != nullptr);

    auto intervals = collectIntervals(*result);
    REQUIRE(intervals.size() == 1);
    REQUIRE(intervals[0] == std::make_pair(int64_t{42}, int64_t{42}));
}

TEST_CASE("timeEntitiesToIntervals - empty group returns nullptr",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    GroupId gid = groups->createGroup("empty_group");

    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result == nullptr);
}

TEST_CASE("timeEntitiesToIntervals - nonexistent group returns nullptr",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);

    // GroupId 999 does not exist
    auto result = MLCore::timeEntitiesToIntervals(*dm, 999, "camera");
    REQUIRE(result == nullptr);
}

TEST_CASE("timeEntitiesToIntervals - filters by time key",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    dm->setTime(TimeKey("neural"), makeTimeFrame(100));
    auto * groups = dm->getEntityGroupManager();

    // Register entities on two different clocks
    auto camera_frames = makeFrames({10, 11, 12});
    auto neural_frames = makeFrames({50, 51});
    auto camera_ids = MLCore::registerTimeEntities(*dm, "camera", camera_frames);
    auto neural_ids = MLCore::registerTimeEntities(*dm, "neural", neural_frames);

    // Put all entities in the same group
    GroupId gid = groups->createGroup("mixed_group");
    groups->addEntitiesToGroup(gid, camera_ids);
    groups->addEntitiesToGroup(gid, neural_ids);

    // Query for "camera" only
    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result != nullptr);
    auto intervals = collectIntervals(*result);
    REQUIRE(intervals.size() == 1);
    REQUIRE(intervals[0] == std::make_pair(int64_t{10}, int64_t{12}));

    // Query for "neural" only
    auto result2 = MLCore::timeEntitiesToIntervals(*dm, gid, "neural");
    REQUIRE(result2 != nullptr);
    auto intervals2 = collectIntervals(*result2);
    REQUIRE(intervals2.size() == 1);
    REQUIRE(intervals2[0] == std::make_pair(int64_t{50}, int64_t{51}));
}

TEST_CASE("timeEntitiesToIntervals - group with only non-matching TimeEntities returns nullptr",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    dm->setTime(TimeKey("neural"), makeTimeFrame(100));
    auto * groups = dm->getEntityGroupManager();

    auto neural_frames = makeFrames({10, 11});
    auto neural_ids = MLCore::registerTimeEntities(*dm, "neural", neural_frames);

    GroupId gid = groups->createGroup("neural_only");
    groups->addEntitiesToGroup(gid, neural_ids);

    // Query for "camera" — no matching entities
    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result == nullptr);
}

TEST_CASE("timeEntitiesToIntervals - unsorted entities are handled correctly",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    // Register entities in non-sorted order
    auto frames = makeFrames({30, 10, 20, 11, 12, 31});
    auto ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    GroupId gid = groups->createGroup("unsorted_group");
    groups->addEntitiesToGroup(gid, ids);

    auto result = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(result != nullptr);

    auto intervals = collectIntervals(*result);
    // Expect: [10,12], [20,20], [30,31]
    REQUIRE(intervals.size() == 3);
    REQUIRE(intervals[0] == std::make_pair(int64_t{10}, int64_t{12}));
    REQUIRE(intervals[1] == std::make_pair(int64_t{20}, int64_t{20}));
    REQUIRE(intervals[2] == std::make_pair(int64_t{30}, int64_t{31}));
}

// ============================================================================
// Round-trip tests
// ============================================================================

TEST_CASE("Round-trip: intervals → entities → intervals",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    // Start with intervals
    auto original = makeIntervalSeries({{5, 9}, {20, 24}});

    // Convert to entities
    auto ids = MLCore::intervalsToTimeEntities(*dm, *original, "camera");
    REQUIRE(ids.size() == 10); // 5 + 5

    // Put in a group
    GroupId gid = groups->createGroup("round_trip");
    groups->addEntitiesToGroup(gid, ids);

    // Convert back to intervals
    auto recovered = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(recovered != nullptr);

    auto intervals = collectIntervals(*recovered);
    REQUIRE(intervals.size() == 2);
    REQUIRE(intervals[0] == std::make_pair(int64_t{5}, int64_t{9}));
    REQUIRE(intervals[1] == std::make_pair(int64_t{20}, int64_t{24}));
}

TEST_CASE("Round-trip: entities → intervals → entities",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    // Start with registered entities
    auto frames = makeFrames({10, 11, 12, 20, 21, 22});
    auto original_ids = MLCore::registerTimeEntities(*dm, "camera", frames);

    // Put in a group
    GroupId gid = groups->createGroup("round_trip");
    groups->addEntitiesToGroup(gid, original_ids);

    // Convert to intervals
    auto intervals = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(intervals != nullptr);

    // Convert back to entities
    auto recovered_ids = MLCore::intervalsToTimeEntities(*dm, *intervals, "camera");

    // Should get same entity IDs (possibly in sorted order)
    REQUIRE(recovered_ids.size() == original_ids.size());

    // Sort both for comparison
    auto sorted_original = original_ids;
    std::sort(sorted_original.begin(), sorted_original.end());
    auto sorted_recovered = recovered_ids;
    std::sort(sorted_recovered.begin(), sorted_recovered.end());

    REQUIRE(sorted_original == sorted_recovered);
}

TEST_CASE("Round-trip: single-frame interval preserves identity",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    auto original = makeIntervalSeries({{42, 42}});
    auto ids = MLCore::intervalsToTimeEntities(*dm, *original, "camera");
    REQUIRE(ids.size() == 1);

    GroupId gid = groups->createGroup("single");
    groups->addEntitiesToGroup(gid, ids);

    auto recovered = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(recovered != nullptr);

    auto intervals = collectIntervals(*recovered);
    REQUIRE(intervals.size() == 1);
    REQUIRE(intervals[0] == std::make_pair(int64_t{42}, int64_t{42}));
}

TEST_CASE("Round-trip: overlapping intervals merge after entity dedup",
          "[mlcore][entity_utils][time_entity]")
{
    auto dm = makeDM("camera", 100);
    auto * groups = dm->getEntityGroupManager();

    // Overlapping intervals: [10,15] and [13,18]
    auto original = makeIntervalSeries({{10, 15}, {13, 18}});

    auto ids = MLCore::intervalsToTimeEntities(*dm, *original, "camera");
    // Should be deduplicated: 10,11,12,13,14,15,16,17,18 = 9
    REQUIRE(ids.size() == 9);

    GroupId gid = groups->createGroup("merged");
    groups->addEntitiesToGroup(gid, ids);

    auto recovered = MLCore::timeEntitiesToIntervals(*dm, gid, "camera");
    REQUIRE(recovered != nullptr);

    auto intervals = collectIntervals(*recovered);
    // Overlapping intervals should merge into one: [10, 18]
    REQUIRE(intervals.size() == 1);
    REQUIRE(intervals[0] == std::make_pair(int64_t{10}, int64_t{18}));
}
