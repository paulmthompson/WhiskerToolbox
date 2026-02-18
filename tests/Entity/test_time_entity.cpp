#include <catch2/catch_test_macros.hpp>

#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <unordered_set>

// ========================================================================
// Phase 1 tests: TimeEntity as a first-class EntityKind in EntityRegistry
// ========================================================================

TEST_CASE("TimeEntity - ensureId returns same ID for same (TimeKey, index)", "[entity][time]") {
    EntityRegistry registry;

    TimeFrameIndex const idx(500);
    EntityId id1 = registry.ensureId("camera", EntityKind::TimeEntity, idx, 0);
    EntityId id2 = registry.ensureId("camera", EntityKind::TimeEntity, idx, 0);

    REQUIRE(id1 == id2);
}

TEST_CASE("TimeEntity - different TimeKeys produce different IDs for same index", "[entity][time]") {
    EntityRegistry registry;

    TimeFrameIndex const idx(100);
    EntityId id_camera = registry.ensureId("camera", EntityKind::TimeEntity, idx, 0);
    EntityId id_ephys  = registry.ensureId("ephys",  EntityKind::TimeEntity, idx, 0);

    REQUIRE(id_camera != id_ephys);
}

TEST_CASE("TimeEntity - different indices produce different IDs for same TimeKey", "[entity][time]") {
    EntityRegistry registry;

    EntityId id1 = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(0), 0);
    EntityId id2 = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(1), 0);
    EntityId id3 = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(999), 0);

    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(id1 != id3);
}

TEST_CASE("TimeEntity - descriptor fields are correct", "[entity][time]") {
    EntityRegistry registry;

    TimeFrameIndex const idx(750);
    EntityId id = registry.ensureId("camera", EntityKind::TimeEntity, idx, 0);

    auto desc = registry.get(id);
    REQUIRE(desc.has_value());
    REQUIRE(desc->data_key == "camera");
    REQUIRE(desc->kind == EntityKind::TimeEntity);
    REQUIRE(desc->time_value == 750);
    REQUIRE(desc->local_index == 0);
}

TEST_CASE("TimeEntity - does not collide with other EntityKinds at same key/time", "[entity][time]") {
    EntityRegistry registry;

    TimeFrameIndex const idx(200);
    EntityId time_id  = registry.ensureId("data1", EntityKind::TimeEntity,  idx, 0);
    EntityId point_id = registry.ensureId("data1", EntityKind::PointEntity, idx, 0);
    EntityId line_id  = registry.ensureId("data1", EntityKind::LineEntity,  idx, 0);

    REQUIRE(time_id != point_id);
    REQUIRE(time_id != line_id);
    REQUIRE(point_id != line_id);
}

TEST_CASE("TimeEntity - clear removes time entities", "[entity][time]") {
    EntityRegistry registry;

    EntityId id = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(42), 0);
    REQUIRE(registry.get(id).has_value());

    registry.clear();

    REQUIRE_FALSE(registry.get(id).has_value());

    // New ID should start from 1 again
    EntityId new_id = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(42), 0);
    REQUIRE(new_id == EntityId(1));
}

TEST_CASE("TimeEntity - bulk registration produces unique sequential IDs", "[entity][time]") {
    EntityRegistry registry;

    constexpr int count = 100;
    std::vector<EntityId> ids;
    ids.reserve(count);

    for (int i = 0; i < count; ++i) {
        ids.push_back(registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(i), 0));
    }

    // All IDs should be unique
    std::unordered_set<EntityId> unique_ids(ids.begin(), ids.end());
    REQUIRE(unique_ids.size() == count);

    // Re-requesting should return the same IDs
    for (int i = 0; i < count; ++i) {
        EntityId id = registry.ensureId("camera", EntityKind::TimeEntity, TimeFrameIndex(i), 0);
        REQUIRE(id == ids[static_cast<std::size_t>(i)]);
    }
}
