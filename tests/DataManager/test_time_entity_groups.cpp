#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <unordered_set>
#include <vector>

// ========================================================================
// Phase 2 tests: Time entity grouping integration
// ========================================================================

TEST_CASE("Time entity groups - create group and add time entities", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("baseline");

    // Register time entities and add them to the group
    auto ids = dm.ensureTimeEntityIds(TimeKey("time"), TimeFrameIndex(100), TimeFrameIndex(104));
    REQUIRE(ids.size() == 5);

    auto added = groups->addEntitiesToGroup(gid, ids);
    REQUIRE(added == 5);

    REQUIRE(groups->getGroupSize(gid) == 5);
}

TEST_CASE("Time entity groups - getTimeIndicesInGroup single clock", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("stimulus");

    auto ids = dm.ensureTimeEntityIds(TimeKey("time"), TimeFrameIndex(50), TimeFrameIndex(54));
    groups->addEntitiesToGroup(gid, ids);

    auto indices = dm.getTimeIndicesInGroup(gid, TimeKey("time"));
    REQUIRE(indices.size() == 5);

    // Sort for deterministic comparison
    std::sort(indices.begin(), indices.end());
    for (int i = 0; i < 5; ++i) {
        REQUIRE(indices[static_cast<std::size_t>(i)] == TimeFrameIndex(50 + i));
    }
}

TEST_CASE("Time entity groups - getTimeIndicesInGroup filters by clock", "[datamanager][time][entity][group]") {
    DataManager dm;

    // Register a second TimeFrame
    auto camera_tf = std::make_shared<TimeFrame>(std::vector<int>{0, 33, 66, 100, 133});
    dm.setTime(TimeKey("camera"), camera_tf);

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("mixed_clocks");

    // Add time entities from both clocks
    auto time_ids = dm.ensureTimeEntityIds(TimeKey("time"), TimeFrameIndex(10), TimeFrameIndex(12));
    auto cam_ids = dm.ensureTimeEntityIds(TimeKey("camera"), TimeFrameIndex(0), TimeFrameIndex(2));

    groups->addEntitiesToGroup(gid, time_ids);
    groups->addEntitiesToGroup(gid, cam_ids);

    REQUIRE(groups->getGroupSize(gid) == 6);

    // Filter by "time" clock - should only get 3
    auto time_indices = dm.getTimeIndicesInGroup(gid, TimeKey("time"));
    REQUIRE(time_indices.size() == 3);
    std::sort(time_indices.begin(), time_indices.end());
    REQUIRE(time_indices[0] == TimeFrameIndex(10));
    REQUIRE(time_indices[1] == TimeFrameIndex(11));
    REQUIRE(time_indices[2] == TimeFrameIndex(12));

    // Filter by "camera" clock - should only get 3
    auto cam_indices = dm.getTimeIndicesInGroup(gid, TimeKey("camera"));
    REQUIRE(cam_indices.size() == 3);
    std::sort(cam_indices.begin(), cam_indices.end());
    REQUIRE(cam_indices[0] == TimeFrameIndex(0));
    REQUIRE(cam_indices[1] == TimeFrameIndex(1));
    REQUIRE(cam_indices[2] == TimeFrameIndex(2));
}

TEST_CASE("Time entity groups - getTimeIndicesInGroup skips non-TimeEntity members", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("mixed_kinds");

    // Add a time entity
    EntityId time_eid = dm.ensureTimeEntityId(TimeKey("time"), TimeFrameIndex(42));
    groups->addEntityToGroup(gid, time_eid);

    // Add a non-time entity (e.g., a PointEntity) directly through the registry
    EntityId point_eid = dm.getEntityRegistry()->ensureId(
        "some_points", EntityKind::PointEntity, TimeFrameIndex(42), 0);
    groups->addEntityToGroup(gid, point_eid);

    REQUIRE(groups->getGroupSize(gid) == 2);

    // getTimeIndicesInGroup should only return the time entity
    auto indices = dm.getTimeIndicesInGroup(gid, TimeKey("time"));
    REQUIRE(indices.size() == 1);
    REQUIRE(indices[0] == TimeFrameIndex(42));
}

TEST_CASE("Time entity groups - getTimeIndicesInGroup returns empty for non-existent group", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto indices = dm.getTimeIndicesInGroup(GroupId{999}, TimeKey("time"));
    REQUIRE(indices.empty());
}

TEST_CASE("Time entity groups - getTimeIndicesInGroupConverted same clock", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("same_clock");

    auto ids = dm.ensureTimeEntityIds(TimeKey("time"), TimeFrameIndex(10), TimeFrameIndex(14));
    groups->addEntitiesToGroup(gid, ids);

    // Convert to same clock → indices unchanged
    auto indices = dm.getTimeIndicesInGroupConverted(gid, TimeKey("time"));
    REQUIRE(indices.size() == 5);

    std::sort(indices.begin(), indices.end());
    for (int i = 0; i < 5; ++i) {
        REQUIRE(indices[static_cast<std::size_t>(i)] == TimeFrameIndex(10 + i));
    }
}

TEST_CASE("Time entity groups - getTimeIndicesInGroupConverted cross-clock", "[datamanager][time][entity][group]") {
    DataManager dm;

    // Default "time" clock: 0,1,2,...,99 (identity mapping: index i → absolute time i)
    std::vector<int> default_times(100);
    for (int i = 0; i < 100; ++i) {
        default_times[static_cast<size_t>(i)] = i;
    }
    dm.setTime(TimeKey("time"), std::make_shared<TimeFrame>(default_times), /*overwrite=*/true);

    // Camera clock: absolute times 0, 10, 20, 30, ..., 90 (10x slower)
    std::vector<int> camera_times;
    for (int i = 0; i < 10; ++i) {
        camera_times.push_back(i * 10);
    }
    dm.setTime(TimeKey("camera"), std::make_shared<TimeFrame>(camera_times));

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("cross_clock");

    // Register camera frame indices 2 and 3 (absolute times 20, 30)
    auto ids = dm.ensureTimeEntityIds(TimeKey("camera"), TimeFrameIndex(2), TimeFrameIndex(3));
    groups->addEntitiesToGroup(gid, ids);

    // Convert to "time" clock → should get indices 20 and 30
    auto indices = dm.getTimeIndicesInGroupConverted(gid, TimeKey("time"));
    REQUIRE(indices.size() == 2);

    std::sort(indices.begin(), indices.end());
    REQUIRE(indices[0] == TimeFrameIndex(20));
    REQUIRE(indices[1] == TimeFrameIndex(30));
}

TEST_CASE("Time entity groups - getTimeIndicesInGroupConverted multi-clock group", "[datamanager][time][entity][group]") {
    DataManager dm;

    // "time" clock: identity mapping 0..49
    std::vector<int> default_times(50);
    for (int i = 0; i < 50; ++i) {
        default_times[static_cast<size_t>(i)] = i;
    }
    dm.setTime(TimeKey("time"), std::make_shared<TimeFrame>(default_times), /*overwrite=*/true);

    // "camera" clock: 0, 5, 10, 15, ..., 45 (5x slower, 10 frames)
    std::vector<int> camera_times;
    for (int i = 0; i < 10; ++i) {
        camera_times.push_back(i * 5);
    }
    dm.setTime(TimeKey("camera"), std::make_shared<TimeFrame>(camera_times));

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("multi_clock");

    // Add "time" index 7 (absolute time 7)
    EntityId t_id = dm.ensureTimeEntityId(TimeKey("time"), TimeFrameIndex(7));
    groups->addEntityToGroup(gid, t_id);

    // Add "camera" index 3 (absolute time 15)
    EntityId c_id = dm.ensureTimeEntityId(TimeKey("camera"), TimeFrameIndex(3));
    groups->addEntityToGroup(gid, c_id);

    // Convert all to "time" clock
    auto indices = dm.getTimeIndicesInGroupConverted(gid, TimeKey("time"));
    REQUIRE(indices.size() == 2);

    std::sort(indices.begin(), indices.end());
    REQUIRE(indices[0] == TimeFrameIndex(7));  // already on "time" clock
    REQUIRE(indices[1] == TimeFrameIndex(15)); // camera idx 3 → absolute 15 → time idx 15
}

TEST_CASE("Time entity groups - getTimeIndicesInGroupConverted skips missing source clock", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("orphan_clock");

    // Register an entity for a clock that exists
    EntityId id = dm.ensureTimeEntityId(TimeKey("time"), TimeFrameIndex(5));
    groups->addEntityToGroup(gid, id);

    // Also register an entity for a clock key that is NOT in _times
    // (create it directly through the registry to bypass DataManager)
    EntityId orphan_id = dm.getEntityRegistry()->ensureId(
        "ghost_clock", EntityKind::TimeEntity, TimeFrameIndex(0), 0);
    groups->addEntityToGroup(gid, orphan_id);

    // Convert to "time" — should get index 5, and skip the orphan
    auto indices = dm.getTimeIndicesInGroupConverted(gid, TimeKey("time"));
    REQUIRE(indices.size() == 1);
    REQUIRE(indices[0] == TimeFrameIndex(5));
}

TEST_CASE("Time entity groups - getTimeIndicesInGroupConverted returns empty for unregistered target", "[datamanager][time][entity][group]") {
    DataManager dm;

    auto * groups = dm.getEntityGroupManager();
    GroupId gid = groups->createGroup("no_target");

    EntityId id = dm.ensureTimeEntityId(TimeKey("time"), TimeFrameIndex(0));
    groups->addEntityToGroup(gid, id);

    // Target clock does not exist
    auto indices = dm.getTimeIndicesInGroupConverted(gid, TimeKey("nonexistent"));
    REQUIRE(indices.empty());
}
