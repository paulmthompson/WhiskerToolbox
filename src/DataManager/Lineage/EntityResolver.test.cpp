#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Lineage/EntityResolver.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"
#include "Entity/Lineage/LineageTypes.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

using namespace WhiskerToolbox::Entity::Lineage;
using Catch::Matchers::UnorderedEquals;

// Helper to create test data
namespace {

std::shared_ptr<LineData> createTestLineData() {
    auto line_data = std::make_shared<LineData>();
    
    // Add some lines at different times
    Line2D line1({Point2D<float>{0.0f, 0.0f}, Point2D<float>{10.0f, 10.0f}});
    Line2D line2({Point2D<float>{5.0f, 0.0f}, Point2D<float>{15.0f, 10.0f}});
    Line2D line3({Point2D<float>{0.0f, 5.0f}, Point2D<float>{10.0f, 15.0f}});
    
    line_data->addAtTime(TimeFrameIndex(10), line1, NotifyObservers::No);
    line_data->addAtTime(TimeFrameIndex(10), line2, NotifyObservers::No);  // Two lines at T10
    line_data->addAtTime(TimeFrameIndex(20), line3, NotifyObservers::No);
    
    return line_data;
}

std::shared_ptr<DigitalEventSeries> createTestEventSeries() {
    auto event_data = std::make_shared<DigitalEventSeries>();
    
    event_data->addEvent(TimeFrameIndex(5));
    event_data->addEvent(TimeFrameIndex(15));
    event_data->addEvent(TimeFrameIndex(25));
    
    return event_data;
}

} // anonymous namespace

TEST_CASE("EntityResolver - Basic Construction", "[lineage][resolver]") {
    DataManager dm;
    EntityResolver resolver(&dm);
    
    SECTION("Null DataManager") {
        EntityResolver null_resolver(nullptr);
        
        auto result = null_resolver.resolveToSource("any_key", TimeFrameIndex(0));
        REQUIRE(result.empty());
    }
}

TEST_CASE("EntityResolver - Direct Entity Resolution", "[lineage][resolver]") {
    DataManager dm;
    auto line_data = createTestLineData();
    
    dm.setData<LineData>("lines", line_data, TimeKey("time"));
    
    EntityResolver resolver(&dm);
    
    SECTION("Get EntityIds from LineData") {
        // At T10, we have 2 lines
        auto ids_at_t10 = resolver.resolveToSource("lines", TimeFrameIndex(10), 0);
        REQUIRE(ids_at_t10.size() == 1);  // First line at T10
        
        auto ids_at_t10_second = resolver.resolveToSource("lines", TimeFrameIndex(10), 1);
        REQUIRE(ids_at_t10_second.size() == 1);  // Second line at T10
        
        // Different EntityIds for different lines
        REQUIRE(ids_at_t10[0] != ids_at_t10_second[0]);
    }
    
    SECTION("Get EntityIds at different times") {
        auto ids_at_t10 = resolver.resolveToSource("lines", TimeFrameIndex(10), 0);
        auto ids_at_t20 = resolver.resolveToSource("lines", TimeFrameIndex(20), 0);
        
        REQUIRE(ids_at_t10.size() == 1);
        REQUIRE(ids_at_t20.size() == 1);
        REQUIRE(ids_at_t10[0] != ids_at_t20[0]);
    }
    
    SECTION("Non-existent time returns empty") {
        auto ids_at_t100 = resolver.resolveToSource("lines", TimeFrameIndex(100), 0);
        REQUIRE(ids_at_t100.empty());
    }
    
    SECTION("Non-existent key returns empty") {
        auto ids = resolver.resolveToSource("nonexistent", TimeFrameIndex(10), 0);
        REQUIRE(ids.empty());
    }
}

TEST_CASE("EntityResolver - DigitalEventSeries Resolution", "[lineage][resolver]") {
    DataManager dm;
    auto event_data = createTestEventSeries();
    
    dm.setData<DigitalEventSeries>("events", event_data, TimeKey("time"));
    
    EntityResolver resolver(&dm);
    
    SECTION("Get EntityId at event time") {
        auto ids_at_t5 = resolver.resolveToSource("events", TimeFrameIndex(5), 0);
        REQUIRE(ids_at_t5.size() == 1);
        
        auto ids_at_t15 = resolver.resolveToSource("events", TimeFrameIndex(15), 0);
        REQUIRE(ids_at_t15.size() == 1);
        
        // Different EntityIds for different events
        REQUIRE(ids_at_t5[0] != ids_at_t15[0]);
    }
    
    SECTION("Non-event time returns empty") {
        auto ids_at_t10 = resolver.resolveToSource("events", TimeFrameIndex(10), 0);
        REQUIRE(ids_at_t10.empty());  // No event at T10
    }
}

TEST_CASE("EntityResolver - Get All Source Entities", "[lineage][resolver]") {
    DataManager dm;
    auto line_data = createTestLineData();
    
    dm.setData<LineData>("lines", line_data, TimeKey("time"));
    
    EntityResolver resolver(&dm);
    
    SECTION("Get all entities from container") {
        auto all_ids = resolver.getAllSourceEntities("lines");
        
        // We added 3 lines total
        REQUIRE(all_ids.size() == 3);
    }
    
    SECTION("Empty for non-existent key") {
        auto all_ids = resolver.getAllSourceEntities("nonexistent");
        REQUIRE(all_ids.empty());
    }
}

TEST_CASE("EntityResolver - Lineage Chain", "[lineage][resolver]") {
    DataManager dm;
    EntityResolver resolver(&dm);
    
    SECTION("Single key returns itself") {
        auto chain = resolver.getLineageChain("some_data");
        REQUIRE(chain.size() == 1);
        REQUIRE(chain[0] == "some_data");
    }
}

TEST_CASE("EntityResolver - Source Status", "[lineage][resolver]") {
    DataManager dm;
    EntityResolver resolver(&dm);
    
    SECTION("Without lineage registry, everything is source") {
        REQUIRE(resolver.isSource("any_key"));
        REQUIRE_FALSE(resolver.hasLineage("any_key"));
    }
}

TEST_CASE("EntityResolver - Resolution Strategy Dispatch", "[lineage][resolver][strategies]") {
    
    SECTION("OneToOneByTime resolution") {
        DataManager dm;
        auto line_data = createTestLineData();
        dm.setData<LineData>("source_lines", line_data, TimeKey("time"));
        
        EntityResolver resolver(&dm);
        
        OneToOneByTime lineage{"source_lines"};
        
        // Test the private method via public interface
        // The resolver would use this internally when lineage is registered
        auto ids = resolver.resolveToSource("source_lines", TimeFrameIndex(10), 0);
        REQUIRE(ids.size() == 1);
    }
    
    SECTION("AllToOneByTime would return all entities at time") {
        DataManager dm;
        auto line_data = createTestLineData();
        dm.setData<LineData>("source_lines", line_data, TimeKey("time"));
        
        EntityResolver resolver(&dm);
        
        // When lineage is registered as AllToOneByTime,
        // resolution should return all EntityIds at that time
        // For now, test via getAllSourceEntities as proxy
        auto all_ids = resolver.getAllSourceEntities("source_lines");
        REQUIRE(all_ids.size() == 3);
    }
}

TEST_CASE("EntityResolver - Multiple Container Types", "[lineage][resolver]") {
    DataManager dm;
    
    auto line_data = createTestLineData();
    auto event_data = createTestEventSeries();
    
    dm.setData<LineData>("lines", line_data, TimeKey("time"));
    dm.setData<DigitalEventSeries>("events", event_data, TimeKey("time"));
    
    EntityResolver resolver(&dm);
    
    SECTION("Resolve from different container types") {
        auto line_ids = resolver.getAllSourceEntities("lines");
        auto event_ids = resolver.getAllSourceEntities("events");
        
        REQUIRE(line_ids.size() == 3);
        REQUIRE(event_ids.size() == 3);
        
        // EntityIds should be distinct across containers
        for (auto const& line_id : line_ids) {
            REQUIRE(event_ids.count(line_id) == 0);
        }
    }
}
