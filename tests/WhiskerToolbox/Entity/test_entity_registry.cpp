#include <catch2/catch_test_macros.hpp>

#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

TEST_CASE("EntityRegistry - Basic ID generation", "[entityregistry][basic]") {
    EntityRegistry registry;
    
    TimeFrameIndex time_index(100);
    EntityId id1 = registry.ensureId("data1", EntityKind::Point, time_index, 0);
    EntityId id2 = registry.ensureId("data2", EntityKind::Line, time_index, 1);
    
    REQUIRE(id1 != id2);
    REQUIRE(id1 == 0); // First ID should be 0
    REQUIRE(id2 == 1); // Second ID should be 1
}

TEST_CASE("EntityRegistry - Deterministic ID generation", "[entityregistry][deterministic]") {
    EntityRegistry registry;
    
    TimeFrameIndex time_index(500);
    
    // Request same entity multiple times
    EntityId id1 = registry.ensureId("test_data", EntityKind::Point, time_index, 5);
    EntityId id2 = registry.ensureId("test_data", EntityKind::Point, time_index, 5);
    EntityId id3 = registry.ensureId("test_data", EntityKind::Point, time_index, 5);
    
    REQUIRE(id1 == id2);
    REQUIRE(id2 == id3);
    REQUIRE(id1 == id3);
}

TEST_CASE("EntityRegistry - Different parameters generate different IDs", "[entityregistry][unique]") {
    EntityRegistry registry;
    
    TimeFrameIndex time_index(200);
    
    EntityId id1 = registry.ensureId("data1", EntityKind::Point, time_index, 0);
    EntityId id2 = registry.ensureId("data2", EntityKind::Point, time_index, 0); // Different data_key
    EntityId id3 = registry.ensureId("data1", EntityKind::Line, time_index, 0);  // Different kind
    EntityId id4 = registry.ensureId("data1", EntityKind::Point, TimeFrameIndex(201), 0); // Different time
    EntityId id5 = registry.ensureId("data1", EntityKind::Point, time_index, 1); // Different local_index
    
    REQUIRE(id1 != id2);
    REQUIRE(id1 != id3);
    REQUIRE(id1 != id4);
    REQUIRE(id1 != id5);
    REQUIRE(id2 != id3);
    REQUIRE(id2 != id4);
    REQUIRE(id2 != id5);
    REQUIRE(id3 != id4);
    REQUIRE(id3 != id5);
    REQUIRE(id4 != id5);
}

TEST_CASE("EntityRegistry - Entity lookup", "[entityregistry][lookup]") {
    EntityRegistry registry;
    
    TimeFrameIndex time_index(750);
    EntityId id = registry.ensureId("lookup_data", EntityKind::Event, time_index, 3);
    
    auto descriptor_opt = registry.get(id);
    REQUIRE(descriptor_opt.has_value());
    
    auto descriptor = descriptor_opt.value();
    REQUIRE(descriptor.data_key == "lookup_data");
    REQUIRE(descriptor.kind == EntityKind::Event);
    REQUIRE(descriptor.time_value == 750);
    REQUIRE(descriptor.local_index == 3);
}

TEST_CASE("EntityRegistry - Lookup non-existent entity", "[entityregistry][lookup][invalid]") {
    EntityRegistry registry;
    
    // Try to lookup an ID that was never created
    auto descriptor_opt = registry.get(9999);
    REQUIRE_FALSE(descriptor_opt.has_value());
}

TEST_CASE("EntityRegistry - Clear functionality", "[entityregistry][clear]") {
    EntityRegistry registry;
    
    TimeFrameIndex time_index(300);
    EntityId id1 = registry.ensureId("clear_test", EntityKind::Interval, time_index, 0);
    EntityId id2 = registry.ensureId("clear_test2", EntityKind::Point, time_index, 1);
    
    // Verify entities exist
    REQUIRE(registry.get(id1).has_value());
    REQUIRE(registry.get(id2).has_value());
    
    // Clear the registry
    registry.clear();
    
    // Verify entities no longer exist
    REQUIRE_FALSE(registry.get(id1).has_value());
    REQUIRE_FALSE(registry.get(id2).has_value());
    
    // Verify new IDs start from 0 again
    EntityId new_id = registry.ensureId("new_data", EntityKind::Point, time_index, 0);
    REQUIRE(new_id == 0);
}

TEST_CASE("EntityRegistry - Multiple entities with different time indices", "[entityregistry][multiple]") {
    EntityRegistry registry;
    
    std::vector<EntityId> ids;
    std::vector<TimeFrameIndex> times;
    
    // Create entities at different times
    for (int i = 0; i < 10; ++i) {
        TimeFrameIndex time(i * 100);
        times.push_back(time);
        EntityId id = registry.ensureId("multi_data", EntityKind::Point, time, i);
        ids.push_back(id);
    }
    
    // Verify all IDs are unique
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            REQUIRE(ids[i] != ids[j]);
        }
    }
    
    // Verify all entities can be looked up correctly
    for (size_t i = 0; i < ids.size(); ++i) {
        auto descriptor_opt = registry.get(ids[i]);
        REQUIRE(descriptor_opt.has_value());
        
        auto descriptor = descriptor_opt.value();
        REQUIRE(descriptor.data_key == "multi_data");
        REQUIRE(descriptor.kind == EntityKind::Point);
        REQUIRE(descriptor.time_value == times[i].getValue());
        REQUIRE(descriptor.local_index == static_cast<int>(i));
    }
}

TEST_CASE("EntityRegistry - Large scale operations", "[entityregistry][scale]") {
    EntityRegistry registry;
    
    const int num_entities = 1000;
    std::vector<EntityId> ids;
    ids.reserve(num_entities);
    
    // Create many entities
    TimeFrameIndex time_index(1000);
    for (int i = 0; i < num_entities; ++i) {
        std::string data_key = "scale_data_" + std::to_string(i);
        EntityId id = registry.ensureId(data_key, EntityKind::Point, time_index, 0);
        ids.push_back(id);
    }
    
    // Verify all IDs are unique
    for (int i = 0; i < num_entities; ++i) {
        for (int j = i + 1; j < num_entities; ++j) {
            REQUIRE(ids[i] != ids[j]);
        }
    }
    
    // Verify all can be looked up
    for (int i = 0; i < num_entities; ++i) {
        auto descriptor_opt = registry.get(ids[i]);
        REQUIRE(descriptor_opt.has_value());
        REQUIRE(descriptor_opt->data_key == "scale_data_" + std::to_string(i));
    }
}
