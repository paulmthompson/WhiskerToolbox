#include <catch2/catch_test_macros.hpp>

#include "Entity/EntityTypes.hpp"

TEST_CASE("EntityTypes - EntityKind enum values", "[entitytypes][enum]") {
    REQUIRE(static_cast<std::uint8_t>(EntityKind::PointEntity) == 0);
    REQUIRE(static_cast<std::uint8_t>(EntityKind::LineEntity) == 1);
    REQUIRE(static_cast<std::uint8_t>(EntityKind::EventEntity) == 2);
    REQUIRE(static_cast<std::uint8_t>(EntityKind::IntervalEntity) == 3);
}

TEST_CASE("EntityTypes - EntityDescriptor construction and access", "[entitytypes][descriptor]") {
    EntityDescriptor desc;
    desc.data_key = "test_data";
    desc.kind = EntityKind::PointEntity;
    desc.time_value = 12345;
    desc.local_index = 7;

    REQUIRE(desc.data_key == "test_data");
    REQUIRE(desc.kind == EntityKind::PointEntity);
    REQUIRE(desc.time_value == 12345);
    REQUIRE(desc.local_index == 7);
}

TEST_CASE("EntityTypes - EntityDescriptor with different kinds", "[entitytypes][descriptor]") {
    SECTION("Line entity") {
        EntityDescriptor line_desc{"line_data", EntityKind::LineEntity, 5000, 3};
        REQUIRE(line_desc.data_key == "line_data");
        REQUIRE(line_desc.kind == EntityKind::LineEntity);
        REQUIRE(line_desc.time_value == 5000);
        REQUIRE(line_desc.local_index == 3);
    }

    SECTION("Event entity") {
        EntityDescriptor event_desc{"event_data", EntityKind::EventEntity, 999, 0};
        REQUIRE(event_desc.data_key == "event_data");
        REQUIRE(event_desc.kind == EntityKind::EventEntity);
        REQUIRE(event_desc.time_value == 999);
        REQUIRE(event_desc.local_index == 0);
    }

    SECTION("Interval entity") {
        EntityDescriptor interval_desc{"interval_data", EntityKind::IntervalEntity, 2500, 15};
        REQUIRE(interval_desc.data_key == "interval_data");
        REQUIRE(interval_desc.kind == EntityKind::IntervalEntity);
        REQUIRE(interval_desc.time_value == 2500);
        REQUIRE(interval_desc.local_index == 15);
    }
}

TEST_CASE("EntityTypes - EntityId is uint64_t", "[entitytypes][entityid]") {
    EntityId id = EntityId(0);
    REQUIRE(std::is_same_v<EntityId, std::uint64_t>);
    
    id = EntityId(12345);
    REQUIRE(id == EntityId(12345));
    
    id = EntityId(std::numeric_limits<std::uint64_t>::max());
    REQUIRE(id == EntityId(std::numeric_limits<std::uint64_t>::max()));
}
