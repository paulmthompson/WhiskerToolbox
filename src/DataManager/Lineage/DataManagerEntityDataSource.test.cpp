#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Lineage/DataManagerEntityDataSource.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

using namespace WhiskerToolbox::Lineage;
using Catch::Matchers::UnorderedEquals;

// =============================================================================
// Helper functions to create test data
// =============================================================================

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

std::shared_ptr<MaskData> createTestMaskData() {
    auto mask_data = std::make_shared<MaskData>();
    
    // Create some simple masks with uint32_t points
    std::vector<Point2D<uint32_t>> mask1_points = {
        {0, 0}, {5, 0}, {5, 5}, {0, 5}
    };
    std::vector<Point2D<uint32_t>> mask2_points = {
        {10, 10}, {15, 10}, {15, 15}, {10, 15}
    };
    
    mask_data->addAtTime(TimeFrameIndex(10), Mask2D(mask1_points), NotifyObservers::No);
    mask_data->addAtTime(TimeFrameIndex(20), Mask2D(mask2_points), NotifyObservers::No);
    
    return mask_data;
}

std::shared_ptr<PointData> createTestPointData() {
    auto point_data = std::make_shared<PointData>();
    
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{1.0f, 2.0f}, NotifyObservers::No);
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);  // Two points at T5
    point_data->addAtTime(TimeFrameIndex(15), Point2D<float>{5.0f, 6.0f}, NotifyObservers::No);
    
    return point_data;
}

std::shared_ptr<DigitalEventSeries> createTestEventSeries() {
    auto event_data = std::make_shared<DigitalEventSeries>();
    
    event_data->addEvent(TimeFrameIndex(5));
    event_data->addEvent(TimeFrameIndex(15));
    event_data->addEvent(TimeFrameIndex(25));
    
    return event_data;
}

std::shared_ptr<DigitalIntervalSeries> createTestIntervalSeries() {
    auto interval_data = std::make_shared<DigitalIntervalSeries>();
    
    // Add intervals using addEvent
    interval_data->addEvent(TimeFrameIndex(5), TimeFrameIndex(15));   // Covers T5-T15
    interval_data->addEvent(TimeFrameIndex(20), TimeFrameIndex(30));  // Covers T20-T30
    
    return interval_data;
}

} // anonymous namespace

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - Construction", "[lineage][datasource]") {
    SECTION("Construct with valid DataManager") {
        DataManager dm;
        DataManagerEntityDataSource data_source(&dm);
        
        // Should not throw, and null data manager should return empty results
        auto ids = data_source.getEntityIds("nonexistent", TimeFrameIndex(0), 0);
        REQUIRE(ids.empty());
    }
    
    SECTION("Construct with null DataManager") {
        DataManagerEntityDataSource data_source(nullptr);
        
        auto ids = data_source.getEntityIds("any_key", TimeFrameIndex(0), 0);
        REQUIRE(ids.empty());
        
        auto all_ids = data_source.getAllEntityIds("any_key");
        REQUIRE(all_ids.empty());
        
        REQUIRE(data_source.getElementCount("any_key", TimeFrameIndex(0)) == 0);
    }
}

// =============================================================================
// LineData Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - LineData", "[lineage][datasource][line]") {
    DataManager dm;
    auto line_data = createTestLineData();
    dm.setData<LineData>("lines", line_data, TimeKey("time"));
    
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("getEntityIds - single line") {
        auto ids = data_source.getEntityIds("lines", TimeFrameIndex(20), 0);
        REQUIRE(ids.size() == 1);
    }
    
    SECTION("getEntityIds - multiple lines at same time") {
        auto ids_first = data_source.getEntityIds("lines", TimeFrameIndex(10), 0);
        auto ids_second = data_source.getEntityIds("lines", TimeFrameIndex(10), 1);
        
        REQUIRE(ids_first.size() == 1);
        REQUIRE(ids_second.size() == 1);
        REQUIRE(ids_first[0] != ids_second[0]);
    }
    
    SECTION("getEntityIds - out of range local_index") {
        auto ids = data_source.getEntityIds("lines", TimeFrameIndex(10), 5);
        REQUIRE(ids.empty());
    }
    
    SECTION("getAllEntityIdsAtTime") {
        auto ids_at_t10 = data_source.getAllEntityIdsAtTime("lines", TimeFrameIndex(10));
        auto ids_at_t20 = data_source.getAllEntityIdsAtTime("lines", TimeFrameIndex(20));
        
        REQUIRE(ids_at_t10.size() == 2);  // Two lines at T10
        REQUIRE(ids_at_t20.size() == 1);  // One line at T20
    }
    
    SECTION("getAllEntityIds") {
        auto all_ids = data_source.getAllEntityIds("lines");
        REQUIRE(all_ids.size() == 3);  // 3 total lines
    }
    
    SECTION("getElementCount") {
        REQUIRE(data_source.getElementCount("lines", TimeFrameIndex(10)) == 2);
        REQUIRE(data_source.getElementCount("lines", TimeFrameIndex(20)) == 1);
        REQUIRE(data_source.getElementCount("lines", TimeFrameIndex(100)) == 0);
    }
}

// =============================================================================
// MaskData Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - MaskData", "[lineage][datasource][mask]") {
    DataManager dm;
    auto mask_data = createTestMaskData();
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));
    
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("getEntityIds") {
        auto ids_at_t10 = data_source.getEntityIds("masks", TimeFrameIndex(10), 0);
        auto ids_at_t20 = data_source.getEntityIds("masks", TimeFrameIndex(20), 0);
        
        REQUIRE(ids_at_t10.size() == 1);
        REQUIRE(ids_at_t20.size() == 1);
        REQUIRE(ids_at_t10[0] != ids_at_t20[0]);
    }
    
    SECTION("getAllEntityIds") {
        auto all_ids = data_source.getAllEntityIds("masks");
        REQUIRE(all_ids.size() == 2);
    }
    
    SECTION("getElementCount") {
        REQUIRE(data_source.getElementCount("masks", TimeFrameIndex(10)) == 1);
        REQUIRE(data_source.getElementCount("masks", TimeFrameIndex(20)) == 1);
    }
}

// =============================================================================
// PointData Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - PointData", "[lineage][datasource][point]") {
    DataManager dm;
    auto point_data = createTestPointData();
    dm.setData<PointData>("points", point_data, TimeKey("time"));
    
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("getEntityIds - multiple points at same time") {
        auto ids_first = data_source.getEntityIds("points", TimeFrameIndex(5), 0);
        auto ids_second = data_source.getEntityIds("points", TimeFrameIndex(5), 1);
        
        REQUIRE(ids_first.size() == 1);
        REQUIRE(ids_second.size() == 1);
        REQUIRE(ids_first[0] != ids_second[0]);
    }
    
    SECTION("getAllEntityIdsAtTime") {
        auto ids_at_t5 = data_source.getAllEntityIdsAtTime("points", TimeFrameIndex(5));
        REQUIRE(ids_at_t5.size() == 2);  // Two points at T5
    }
    
    SECTION("getAllEntityIds") {
        auto all_ids = data_source.getAllEntityIds("points");
        REQUIRE(all_ids.size() == 3);  // 3 total points
    }
}

// =============================================================================
// DigitalEventSeries Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - DigitalEventSeries", "[lineage][datasource][event]") {
    DataManager dm;
    auto event_data = createTestEventSeries();
    dm.setData<DigitalEventSeries>("events", event_data, TimeKey("time"));
    
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("getEntityIds - at event time") {
        auto ids_at_t5 = data_source.getEntityIds("events", TimeFrameIndex(5), 0);
        auto ids_at_t15 = data_source.getEntityIds("events", TimeFrameIndex(15), 0);
        
        REQUIRE(ids_at_t5.size() == 1);
        REQUIRE(ids_at_t15.size() == 1);
        REQUIRE(ids_at_t5[0] != ids_at_t15[0]);
    }
    
    SECTION("getEntityIds - at non-event time") {
        auto ids_at_t10 = data_source.getEntityIds("events", TimeFrameIndex(10), 0);
        REQUIRE(ids_at_t10.empty());
    }
    
    SECTION("getAllEntityIdsAtTime") {
        auto ids_at_t5 = data_source.getAllEntityIdsAtTime("events", TimeFrameIndex(5));
        REQUIRE(ids_at_t5.size() == 1);
        
        auto ids_at_t10 = data_source.getAllEntityIdsAtTime("events", TimeFrameIndex(10));
        REQUIRE(ids_at_t10.empty());
    }
    
    SECTION("getAllEntityIds") {
        auto all_ids = data_source.getAllEntityIds("events");
        REQUIRE(all_ids.size() == 3);  // 3 events
    }
    
    SECTION("getElementCount") {
        REQUIRE(data_source.getElementCount("events", TimeFrameIndex(5)) == 1);
        REQUIRE(data_source.getElementCount("events", TimeFrameIndex(10)) == 0);
    }
}

// =============================================================================
// DigitalIntervalSeries Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - DigitalIntervalSeries", "[lineage][datasource][interval]") {
    DataManager dm;
    auto interval_data = createTestIntervalSeries();
    dm.setData<DigitalIntervalSeries>("intervals", interval_data, TimeKey("time"));
    
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("getEntityIds - within interval") {
        // T10 is within first interval (5-15)
        auto ids_at_t10 = data_source.getEntityIds("intervals", TimeFrameIndex(10), 0);
        REQUIRE(ids_at_t10.size() == 1);
    }
    
    SECTION("getEntityIds - outside intervals") {
        // T17 is between intervals
        auto ids_at_t17 = data_source.getEntityIds("intervals", TimeFrameIndex(17), 0);
        REQUIRE(ids_at_t17.empty());
    }
    
    SECTION("getEntityIds - at interval boundary") {
        // T5 and T15 are boundaries of first interval
        auto ids_at_t5 = data_source.getEntityIds("intervals", TimeFrameIndex(5), 0);
        auto ids_at_t15 = data_source.getEntityIds("intervals", TimeFrameIndex(15), 0);
        
        REQUIRE(ids_at_t5.size() == 1);
        REQUIRE(ids_at_t15.size() == 1);
        REQUIRE(ids_at_t5[0] == ids_at_t15[0]);  // Same interval
    }
    
    SECTION("getAllEntityIds") {
        auto all_ids = data_source.getAllEntityIds("intervals");
        REQUIRE(all_ids.size() == 2);  // 2 intervals
    }
    
    SECTION("getElementCount") {
        REQUIRE(data_source.getElementCount("intervals", TimeFrameIndex(10)) == 1);
        REQUIRE(data_source.getElementCount("intervals", TimeFrameIndex(17)) == 0);
        REQUIRE(data_source.getElementCount("intervals", TimeFrameIndex(25)) == 1);
    }
}

// =============================================================================
// Unknown/Unsupported Data Types Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - Unsupported Types", "[lineage][datasource]") {
    DataManager dm;
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("Non-existent key") {
        auto ids = data_source.getEntityIds("nonexistent", TimeFrameIndex(10), 0);
        REQUIRE(ids.empty());
        
        auto all_ids = data_source.getAllEntityIds("nonexistent");
        REQUIRE(all_ids.empty());
    }
}

// =============================================================================
// Multiple Container Types Tests
// =============================================================================

TEST_CASE("DataManagerEntityDataSource - Multiple Containers", "[lineage][datasource]") {
    DataManager dm;
    
    auto line_data = createTestLineData();
    auto event_data = createTestEventSeries();
    
    dm.setData<LineData>("lines", line_data, TimeKey("time"));
    dm.setData<DigitalEventSeries>("events", event_data, TimeKey("time"));
    
    DataManagerEntityDataSource data_source(&dm);
    
    SECTION("EntityIds are unique across containers") {
        auto line_ids = data_source.getAllEntityIds("lines");
        auto event_ids = data_source.getAllEntityIds("events");
        
        // EntityIds should not overlap between containers
        for (auto const& line_id : line_ids) {
            REQUIRE(event_ids.count(line_id) == 0);
        }
    }
}
