#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <unordered_set>
#include <vector>

// ========================================================================
// Phase 1 tests: DataManager time-entity helper methods
// ========================================================================

TEST_CASE("DataManager::ensureTimeEntityId - basic registration", "[datamanager][time][entity]") {
    DataManager dm;

    // The default "time" TimeKey always exists
    TimeKey const key("time");
    TimeFrameIndex const idx(10);

    EntityId id = dm.ensureTimeEntityId(key, idx);
    REQUIRE(id != EntityId(0)); // Not the sentinel value

    // Same call returns same ID (idempotent)
    EntityId id2 = dm.ensureTimeEntityId(key, idx);
    REQUIRE(id == id2);
}

TEST_CASE("DataManager::ensureTimeEntityId - different clocks", "[datamanager][time][entity]") {
    DataManager dm;

    // Register a second TimeFrame
    auto camera_tf = std::make_shared<TimeFrame>(std::vector<int>{0, 33, 66, 100});
    dm.setTime(TimeKey("camera"), camera_tf);

    TimeFrameIndex const idx(2);
    EntityId id_time   = dm.ensureTimeEntityId(TimeKey("time"),   idx);
    EntityId id_camera = dm.ensureTimeEntityId(TimeKey("camera"), idx);

    REQUIRE(id_time != id_camera);
}

TEST_CASE("DataManager::ensureTimeEntityIds - contiguous range", "[datamanager][time][entity]") {
    DataManager dm;

    TimeKey const key("time");
    auto ids = dm.ensureTimeEntityIds(key, TimeFrameIndex(5), TimeFrameIndex(9));

    REQUIRE(ids.size() == 5); // indices 5,6,7,8,9

    // All IDs should be unique
    std::unordered_set<EntityId> unique_ids(ids.begin(), ids.end());
    REQUIRE(unique_ids.size() == 5);

    // Re-requesting should return the same IDs in the same order
    auto ids2 = dm.ensureTimeEntityIds(key, TimeFrameIndex(5), TimeFrameIndex(9));
    REQUIRE(ids == ids2);
}

TEST_CASE("DataManager::ensureTimeEntityIds - single element range", "[datamanager][time][entity]") {
    DataManager dm;

    auto ids = dm.ensureTimeEntityIds(TimeKey("time"), TimeFrameIndex(42), TimeFrameIndex(42));
    REQUIRE(ids.size() == 1);
}

TEST_CASE("DataManager::ensureTimeEntityIds - empty range returns empty", "[datamanager][time][entity]") {
    DataManager dm;

    // end < start → empty
    auto ids = dm.ensureTimeEntityIds(TimeKey("time"), TimeFrameIndex(10), TimeFrameIndex(5));
    REQUIRE(ids.empty());
}

TEST_CASE("DataManager::resolveTimeEntity - round-trip", "[datamanager][time][entity]") {
    DataManager dm;

    TimeKey const key("time");
    TimeFrameIndex const idx(123);

    EntityId id = dm.ensureTimeEntityId(key, idx);

    auto result = dm.resolveTimeEntity(id);
    REQUIRE(result.has_value());
    REQUIRE(result->first == key);
    REQUIRE(result->second == idx);
}

TEST_CASE("DataManager::resolveTimeEntity - non-existent entity returns nullopt", "[datamanager][time][entity]") {
    DataManager dm;

    auto result = dm.resolveTimeEntity(EntityId(99999));
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DataManager::resolveTimeEntity - non-TimeEntity returns nullopt", "[datamanager][time][entity]") {
    DataManager dm;

    // Register a PointEntity directly through the registry
    EntityId point_id = dm.getEntityRegistry()->ensureId(
        "some_data", EntityKind::PointEntity, TimeFrameIndex(50), 0);

    auto result = dm.resolveTimeEntity(point_id);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("DataManager::resolveTimeEntity - batch round-trip", "[datamanager][time][entity]") {
    DataManager dm;

    TimeKey const key("time");
    auto ids = dm.ensureTimeEntityIds(key, TimeFrameIndex(100), TimeFrameIndex(104));
    REQUIRE(ids.size() == 5);

    for (std::size_t i = 0; i < ids.size(); ++i) {
        auto result = dm.resolveTimeEntity(ids[i]);
        REQUIRE(result.has_value());
        REQUIRE(result->first == key);
        REQUIRE(result->second == TimeFrameIndex(static_cast<int64_t>(100 + i)));
    }
}

TEST_CASE("DataManager time entities - coexist with data entities", "[datamanager][time][entity]") {
    DataManager dm;

    // Create a time entity
    EntityId time_id = dm.ensureTimeEntityId(TimeKey("time"), TimeFrameIndex(50));

    // Create a data entity through the registry
    EntityId data_id = dm.getEntityRegistry()->ensureId(
        "lines", EntityKind::LineEntity, TimeFrameIndex(50), 0);

    // They should be different entities
    REQUIRE(time_id != data_id);

    // Time entity resolves correctly
    auto time_result = dm.resolveTimeEntity(time_id);
    REQUIRE(time_result.has_value());
    REQUIRE(time_result->first == TimeKey("time"));

    // Data entity does not resolve as time entity
    auto data_result = dm.resolveTimeEntity(data_id);
    REQUIRE_FALSE(data_result.has_value());
}
