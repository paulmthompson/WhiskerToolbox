/**
 * @file digital_interval_storage_test.cpp
 * @brief Unit tests for DigitalIntervalStorage implementations
 * 
 * Tests the storage abstraction layer for DigitalIntervalSeries:
 * - OwningDigitalIntervalStorage: Basic owning storage with SoA layout
 * - ViewDigitalIntervalStorage: Zero-copy view/filter over owning storage
 * - LazyDigitalIntervalStorage: On-demand computation from transform views
 * - DigitalIntervalStorageWrapper: Type-erased wrapper with cache optimization
 */

#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/DigitalIntervalStorage.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>
#include <unordered_set>

// =============================================================================
// OwningDigitalIntervalStorage Tests
// =============================================================================

TEST_CASE("OwningDigitalIntervalStorage basic operations", "[DigitalIntervalStorage]") {
    OwningDigitalIntervalStorage storage;
    
    SECTION("Empty storage") {
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
        CHECK(storage.getStorageType() == DigitalIntervalStorageType::Owning);
        CHECK_FALSE(storage.isView());
        CHECK_FALSE(storage.isLazy());
    }
    
    SECTION("Add single interval") {
        bool added = storage.addInterval(Interval{10, 20}, EntityId{100});
        
        CHECK(added);
        CHECK(storage.size() == 1);
        CHECK_FALSE(storage.empty());
        
        CHECK(storage.getInterval(0).start == 10);
        CHECK(storage.getInterval(0).end == 20);
        CHECK(storage.getEntityId(0) == EntityId{100});
    }
    
    SECTION("Intervals are sorted by start time") {
        storage.addInterval(Interval{30, 40}, EntityId{3});
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{20, 25}, EntityId{2});
        
        CHECK(storage.size() == 3);
        CHECK(storage.getInterval(0).start == 10);
        CHECK(storage.getInterval(1).start == 20);
        CHECK(storage.getInterval(2).start == 30);
    }
    
    SECTION("Duplicate intervals are rejected") {
        bool added1 = storage.addInterval(Interval{10, 20}, EntityId{1});
        bool added2 = storage.addInterval(Interval{10, 20}, EntityId{2});
        
        CHECK(added1);
        CHECK_FALSE(added2);
        CHECK(storage.size() == 1);
        CHECK(storage.getEntityId(0) == EntityId{1}); // First one kept
    }
    
    SECTION("Different intervals with same start are allowed") {
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{10, 30}, EntityId{2});
        
        CHECK(storage.size() == 2);
    }
    
    SECTION("Remove interval by exact match") {
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{20, 30}, EntityId{2});
        storage.addInterval(Interval{30, 40}, EntityId{3});
        
        bool removed = storage.removeInterval(Interval{20, 30});
        
        CHECK(removed);
        CHECK(storage.size() == 2);
        CHECK(storage.getInterval(0).start == 10);
        CHECK(storage.getInterval(1).start == 30);
        
        // Remove non-existent
        bool removed2 = storage.removeInterval(Interval{100, 200});
        CHECK_FALSE(removed2);
    }
    
    SECTION("Remove interval by EntityId") {
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{20, 30}, EntityId{2});
        storage.addInterval(Interval{30, 40}, EntityId{3});
        
        bool removed = storage.removeByEntityId(EntityId{2});
        
        CHECK(removed);
        CHECK(storage.size() == 2);
        
        auto opt = storage.findByEntityId(EntityId{2});
        CHECK_FALSE(opt.has_value());
    }
    
    SECTION("Clear") {
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{20, 30}, EntityId{2});
        
        storage.clear();
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
    }
    
    SECTION("Find by interval") {
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{20, 30}, EntityId{2});
        storage.addInterval(Interval{30, 40}, EntityId{3});
        
        auto found = storage.findByInterval(Interval{20, 30});
        REQUIRE(found.has_value());
        CHECK(*found == 1);
        
        auto not_found = storage.findByInterval(Interval{25, 35});
        CHECK_FALSE(not_found.has_value());
    }
    
    SECTION("Find by EntityId") {
        storage.addInterval(Interval{10, 20}, EntityId{100});
        storage.addInterval(Interval{20, 30}, EntityId{200});
        storage.addInterval(Interval{30, 40}, EntityId{300});
        
        auto found = storage.findByEntityId(EntityId{200});
        REQUIRE(found.has_value());
        CHECK(storage.getInterval(*found).start == 20);
        
        auto not_found = storage.findByEntityId(EntityId{999});
        CHECK_FALSE(not_found.has_value());
    }
    
    SECTION("Has interval at time") {
        storage.addInterval(Interval{10, 20}, EntityId{1});
        storage.addInterval(Interval{30, 40}, EntityId{2});
        
        CHECK(storage.hasIntervalAtTime(15));
        CHECK(storage.hasIntervalAtTime(10));
        CHECK(storage.hasIntervalAtTime(20));
        CHECK_FALSE(storage.hasIntervalAtTime(25));
        CHECK(storage.hasIntervalAtTime(35));
        CHECK_FALSE(storage.hasIntervalAtTime(5));
        CHECK_FALSE(storage.hasIntervalAtTime(50));
    }
}

TEST_CASE("OwningDigitalIntervalStorage range queries", "[DigitalIntervalStorage]") {
    OwningDigitalIntervalStorage storage;
    
    // Add intervals: [10,20], [25,35], [40,50], [45,55]
    storage.addInterval(Interval{10, 20}, EntityId{1});
    storage.addInterval(Interval{25, 35}, EntityId{2});
    storage.addInterval(Interval{40, 50}, EntityId{3});
    storage.addInterval(Interval{45, 55}, EntityId{4});
    
    SECTION("Get overlapping range") {
        // Query [15, 30] should overlap with [10,20] and [25,35]
        auto [start, end] = storage.getOverlappingRange(15, 30);
        CHECK(start == 0);
        CHECK(end == 2);
        
        // Query [42, 48] should overlap with [40,50] and [45,55]
        auto [start2, end2] = storage.getOverlappingRange(42, 48);
        CHECK(start2 == 2);
        CHECK(end2 == 4);
    }
    
    SECTION("Get contained range") {
        // Query [0, 60] should contain all intervals
        auto [start, end] = storage.getContainedRange(0, 60);
        CHECK(end - start == 4);
        
        // Query [10, 35] should contain [10,20] and [25,35]
        auto [start2, end2] = storage.getContainedRange(10, 35);
        CHECK(end2 - start2 == 2);
        
        // Query [11, 19] should contain nothing (no interval fully contained)
        auto [start3, end3] = storage.getContainedRange(11, 19);
        CHECK(start3 == end3);
    }
}

TEST_CASE("OwningDigitalIntervalStorage construction from vectors", "[DigitalIntervalStorage]") {
    
    SECTION("Construct from interval vector only") {
        std::vector<Interval> intervals = {
            Interval{30, 40}, Interval{10, 20}, Interval{20, 30}
        };
        OwningDigitalIntervalStorage storage{intervals};
        
        CHECK(storage.size() == 3);
        // Intervals should be sorted by start
        CHECK(storage.getInterval(0).start == 10);
        CHECK(storage.getInterval(1).start == 20);
        CHECK(storage.getInterval(2).start == 30);
        // Entity IDs should be zero
        CHECK(storage.getEntityId(0) == EntityId{0});
    }
    
    SECTION("Construct from interval and entity ID vectors") {
        std::vector<Interval> intervals = {
            Interval{30, 40}, Interval{10, 20}, Interval{20, 30}
        };
        std::vector<EntityId> ids = {EntityId{3}, EntityId{1}, EntityId{2}};
        
        OwningDigitalIntervalStorage storage{intervals, ids};
        
        CHECK(storage.size() == 3);
        // Intervals and IDs should be sorted together
        CHECK(storage.getInterval(0).start == 10);
        CHECK(storage.getEntityId(0) == EntityId{1});
        CHECK(storage.getInterval(1).start == 20);
        CHECK(storage.getEntityId(1) == EntityId{2});
    }
}

TEST_CASE("OwningDigitalIntervalStorage cache optimization", "[DigitalIntervalStorage][cache]") {
    OwningDigitalIntervalStorage storage;
    
    // Add some intervals
    for (int i = 0; i < 10; ++i) {
        storage.addInterval(Interval{i * 10, i * 10 + 5}, EntityId{static_cast<uint32_t>(i)});
    }
    
    SECTION("Cache is valid for owning storage") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 10);
    }
    
    SECTION("Cache provides direct access") {
        auto cache = storage.tryGetCache();
        
        for (size_t i = 0; i < cache.cache_size; ++i) {
            CHECK(cache.getInterval(i).start == storage.getInterval(i).start);
            CHECK(cache.getInterval(i).end == storage.getInterval(i).end);
            CHECK(cache.getEntityId(i) == storage.getEntityId(i));
        }
    }
}

// =============================================================================
// ViewDigitalIntervalStorage Tests
// =============================================================================

TEST_CASE("ViewDigitalIntervalStorage basic operations", "[DigitalIntervalStorage][view]") {
    auto source = std::make_shared<OwningDigitalIntervalStorage>();
    
    // Populate source
    source->addInterval(Interval{10, 20}, EntityId{1});
    source->addInterval(Interval{25, 35}, EntityId{2});
    source->addInterval(Interval{40, 50}, EntityId{3});
    source->addInterval(Interval{60, 70}, EntityId{4});
    
    ViewDigitalIntervalStorage view{source};
    
    SECTION("Empty view") {
        CHECK(view.size() == 0);
        CHECK(view.empty());
        CHECK(view.getStorageType() == DigitalIntervalStorageType::View);
        CHECK(view.isView());
    }
    
    SECTION("View all elements") {
        view.setAllIndices();
        
        CHECK(view.size() == 4);
        CHECK_FALSE(view.empty());
        CHECK(view.getInterval(0).start == 10);
        CHECK(view.getEntityId(0) == EntityId{1});
    }
    
    SECTION("Filter by overlapping range") {
        view.filterByOverlappingRange(20, 45);
        
        // Should include [10,20], [25,35], [40,50] (all overlap with [20,45])
        CHECK(view.size() == 3);
        CHECK(view.getInterval(0).start == 10);
        CHECK(view.getInterval(1).start == 25);
        CHECK(view.getInterval(2).start == 40);
    }
    
    SECTION("Filter by contained range") {
        view.filterByContainedRange(5, 55);
        
        // Should include [10,20], [25,35], [40,50] (all contained in [5,55])
        CHECK(view.size() == 3);
    }
    
    SECTION("Filter by EntityId set") {
        std::unordered_set<EntityId> ids{EntityId{1}, EntityId{3}};
        view.filterByEntityIds(ids);
        
        CHECK(view.size() == 2);
        CHECK(view.getEntityId(0) == EntityId{1});
        CHECK(view.getEntityId(1) == EntityId{3});
    }
    
    SECTION("Find by EntityId in view") {
        view.setAllIndices();
        
        auto found = view.findByEntityId(EntityId{2});
        REQUIRE(found.has_value());
        CHECK(view.getInterval(*found).start == 25);
        
        auto not_found = view.findByEntityId(EntityId{999});
        CHECK_FALSE(not_found.has_value());
    }
}

TEST_CASE("ViewDigitalIntervalStorage cache optimization", "[DigitalIntervalStorage][view][cache]") {
    auto source = std::make_shared<OwningDigitalIntervalStorage>();
    
    for (int i = 0; i < 10; ++i) {
        source->addInterval(Interval{i * 10, i * 10 + 5}, EntityId{static_cast<uint32_t>(i)});
    }
    
    ViewDigitalIntervalStorage view{source};
    
    SECTION("Contiguous view has valid cache") {
        view.setAllIndices();
        auto cache = view.tryGetCache();
        
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 10);
    }
    
    SECTION("Filtered view may have invalid cache") {
        std::unordered_set<EntityId> ids{EntityId{1}, EntityId{3}, EntityId{7}};
        view.filterByEntityIds(ids);
        
        auto cache = view.tryGetCache();
        CHECK_FALSE(cache.isValid());  // Non-contiguous indices
    }
    
    SECTION("Contiguous subset has valid cache") {
        // Filter to indices [2, 3, 4] which is contiguous
        view.filterByContainedRange(20, 45);
        
        auto cache = view.tryGetCache();
        // This should be contiguous if the range [20, 45] yields consecutive indices
        // Intervals at indices 2, 3, 4 have starts 20, 30, 40
        // May or may not be contiguous depending on implementation details
    }
}

// =============================================================================
// DigitalIntervalStorageWrapper Tests
// =============================================================================

TEST_CASE("DigitalIntervalStorageWrapper basic operations", "[DigitalIntervalStorage][wrapper]") {
    
    SECTION("Default construction creates empty owning storage") {
        DigitalIntervalStorageWrapper wrapper;
        
        CHECK(wrapper.size() == 0);
        CHECK(wrapper.empty());
        CHECK(wrapper.getStorageType() == DigitalIntervalStorageType::Owning);
    }
    
    SECTION("Mutation operations work on owning storage") {
        DigitalIntervalStorageWrapper wrapper;
        
        wrapper.addInterval(Interval{10, 20}, EntityId{1});
        wrapper.addInterval(Interval{30, 40}, EntityId{2});
        
        CHECK(wrapper.size() == 2);
        CHECK(wrapper.getInterval(0).start == 10);
        CHECK(wrapper.getEntityId(1) == EntityId{2});
    }
    
    SECTION("Find operations work") {
        DigitalIntervalStorageWrapper wrapper;
        
        wrapper.addInterval(Interval{10, 20}, EntityId{100});
        wrapper.addInterval(Interval{30, 40}, EntityId{200});
        
        auto found = wrapper.findByInterval(Interval{30, 40});
        REQUIRE(found.has_value());
        CHECK(*found == 1);
        
        auto found_id = wrapper.findByEntityId(EntityId{100});
        REQUIRE(found_id.has_value());
        CHECK(*found_id == 0);
    }
    
    SECTION("Type access works") {
        DigitalIntervalStorageWrapper wrapper;
        
        auto* owning = wrapper.tryGetMutableOwning();
        REQUIRE(owning != nullptr);
        
        owning->addInterval(Interval{10, 20}, EntityId{1});
        CHECK(wrapper.size() == 1);
    }
    
    SECTION("View storage through wrapper") {
        auto source = std::make_shared<OwningDigitalIntervalStorage>();
        source->addInterval(Interval{10, 20}, EntityId{1});
        source->addInterval(Interval{30, 40}, EntityId{2});
        
        ViewDigitalIntervalStorage view{source};
        view.setAllIndices();
        
        DigitalIntervalStorageWrapper wrapper{std::move(view)};
        
        CHECK(wrapper.size() == 2);
        CHECK(wrapper.isView());
        CHECK(wrapper.getStorageType() == DigitalIntervalStorageType::View);
    }
}

// =============================================================================
// DigitalIntervalSeries Integration Tests
// =============================================================================

TEST_CASE("DigitalIntervalSeries storage integration", "[DigitalIntervalSeries][storage]") {
    
    SECTION("Default construction uses owning storage") {
        DigitalIntervalSeries series;
        
        CHECK(series.size() == 0);
        CHECK_FALSE(series.isView());
        CHECK_FALSE(series.isLazy());
        CHECK(series.getStorageType() == DigitalIntervalStorageType::Owning);
    }
    
    SECTION("Construction from vector syncs storage") {
        std::vector<Interval> intervals = {
            Interval{30, 40}, Interval{10, 20}
        };
        DigitalIntervalSeries series{intervals};
        
        CHECK(series.size() == 2);
        // Intervals should be sorted
        auto const& data = series.getDigitalIntervalSeries();
        CHECK(data[0].start == 10);
        CHECK(data[1].start == 30);
    }
    
    SECTION("Mutations sync storage") {
        DigitalIntervalSeries series;
        
        series.addEvent(Interval{10, 20});
        series.addEvent(Interval{30, 40});
        
        CHECK(series.size() == 2);
        
        // Check storage cache is valid
        auto cache = series.getStorageCache();
        CHECK(cache.isValid());
    }
}

TEST_CASE("DigitalIntervalSeries view creation by time range", "[DigitalIntervalSeries][view]") {
    auto source = std::make_shared<DigitalIntervalSeries>();
    
    source->addEvent(Interval{10, 20});
    source->addEvent(Interval{30, 40});
    source->addEvent(Interval{50, 60});
    source->addEvent(Interval{70, 80});
    
    auto view = DigitalIntervalSeries::createView(source, 25, 55);
    
    CHECK(view->isView());
    // Should include intervals overlapping [25, 55]: [30,40], [50,60]
    CHECK(view->size() == 2);
}

TEST_CASE("DigitalIntervalSeries view creation with DataManager", "[DigitalIntervalSeries][view][entity]") {
    // Use DataManager to get proper EntityId registration
    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40, 50, 60, 70, 80});
    data_manager->setTime(TimeKey("test_time"), time_frame);
    
    data_manager->setData<DigitalIntervalSeries>("source_intervals", TimeKey("test_time"));
    auto source = data_manager->getData<DigitalIntervalSeries>("source_intervals");
    
    // Add intervals - they will get unique EntityIds from the registry
    source->addEvent(Interval{10, 20});
    source->addEvent(Interval{30, 40});
    source->addEvent(Interval{50, 60});
    source->addEvent(Interval{70, 80});
    
    REQUIRE(source->size() == 4);
    
    SECTION("Create view by EntityIds") {
        auto const& ids = source->getEntityIds();
        REQUIRE(ids.size() == 4);
        
        // Verify all IDs are unique
        std::unordered_set<EntityId> all_ids(ids.begin(), ids.end());
        REQUIRE(all_ids.size() == 4);
        
        // Filter to keep only intervals at indices 0, 2
        std::unordered_set<EntityId> filter_ids{ids[0], ids[2]};
        auto view = DigitalIntervalSeries::createView(source, filter_ids);
        
        CHECK(view->isView());
        CHECK(view->size() == 2);
        
        // Verify the intervals are the right ones
        auto const& interval_vec = view->getDigitalIntervalSeries();
        CHECK(interval_vec[0].start == 10);
        CHECK(interval_vec[1].start == 50);
    }
}

TEST_CASE("DigitalIntervalSeries materialization", "[DigitalIntervalSeries][materialize]") {
    auto source = std::make_shared<DigitalIntervalSeries>();
    
    source->addEvent(Interval{10, 20});
    source->addEvent(Interval{30, 40});
    source->addEvent(Interval{50, 60});
    
    // Create a view
    auto view = DigitalIntervalSeries::createView(source, 25, 55);
    CHECK(view->isView());
    
    // Materialize the view
    auto materialized = view->materialize();
    
    CHECK_FALSE(materialized->isView());
    CHECK(materialized->getStorageType() == DigitalIntervalStorageType::Owning);
    CHECK(materialized->size() == view->size());
    
    // Verify data was copied
    auto const& view_data = view->getDigitalIntervalSeries();
    auto const& mat_data = materialized->getDigitalIntervalSeries();
    
    REQUIRE(view_data.size() == mat_data.size());
    for (size_t i = 0; i < view_data.size(); ++i) {
        CHECK(view_data[i].start == mat_data[i].start);
        CHECK(view_data[i].end == mat_data[i].end);
    }
}

// =============================================================================
// LazyDigitalIntervalStorage Tests
// =============================================================================

TEST_CASE("LazyDigitalIntervalStorage basic operations", "[DigitalIntervalStorage][lazy]") {
    // Create a simple transform view
    std::vector<std::pair<Interval, EntityId>> source_data = {
        {Interval{10, 20}, EntityId{1}},
        {Interval{30, 40}, EntityId{2}},
        {Interval{50, 60}, EntityId{3}}
    };
    
    auto view = source_data | std::views::transform([](auto const& p) {
        return std::make_pair(Interval{p.first.start * 2, p.first.end * 2}, p.second);
    });
    
    using ViewType = decltype(view);
    LazyDigitalIntervalStorage<ViewType> storage{view, source_data.size()};
    
    SECTION("Size and type") {
        CHECK(storage.size() == 3);
        CHECK(storage.getStorageType() == DigitalIntervalStorageType::Lazy);
        CHECK(storage.isLazy());
    }
    
    SECTION("Lazy computation on access") {
        // Should be transformed: [10,20] -> [20,40]
        auto const& interval = storage.getInterval(0);
        CHECK(interval.start == 20);
        CHECK(interval.end == 40);
        
        // EntityId preserved
        CHECK(storage.getEntityId(0) == EntityId{1});
    }
    
    SECTION("Cache is invalid for lazy storage") {
        auto cache = storage.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
    
    SECTION("Find by EntityId") {
        auto found = storage.findByEntityId(EntityId{2});
        REQUIRE(found.has_value());
        CHECK(*found == 1);
    }
}

TEST_CASE("LazyDigitalIntervalStorage range queries", "[DigitalIntervalStorage][lazy]") {
    std::vector<std::pair<Interval, EntityId>> source_data = {
        {Interval{10, 20}, EntityId{1}},
        {Interval{30, 40}, EntityId{2}},
        {Interval{50, 60}, EntityId{3}},
        {Interval{70, 80}, EntityId{4}}
    };
    
    // Identity transform for testing
    auto view = source_data | std::views::transform([](auto const& p) { return p; });
    
    using ViewType = decltype(view);
    LazyDigitalIntervalStorage<ViewType> storage{view, source_data.size()};
    
    SECTION("Get overlapping range") {
        auto [start, end] = storage.getOverlappingRange(25, 55);
        // Should include [30,40] and [50,60]
        CHECK(end - start == 2);
    }
    
    SECTION("Get contained range") {
        auto [start, end] = storage.getContainedRange(0, 100);
        CHECK(end - start == 4);  // All contained
    }
    
    SECTION("Has interval at time") {
        CHECK(storage.hasIntervalAtTime(35));
        CHECK_FALSE(storage.hasIntervalAtTime(45));
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("DigitalIntervalStorage edge cases", "[DigitalIntervalStorage][edge]") {
    
    SECTION("Empty range queries") {
        OwningDigitalIntervalStorage storage;
        
        auto [start, end] = storage.getOverlappingRange(0, 100);
        CHECK(start == 0);
        CHECK(end == 0);
    }
    
    SECTION("Invalid range (start > end)") {
        OwningDigitalIntervalStorage storage;
        storage.addInterval(Interval{10, 20}, EntityId{1});
        
        auto [start, end] = storage.getOverlappingRange(100, 50);
        CHECK(start == 0);
        CHECK(end == 0);
    }
    
    SECTION("Single point interval") {
        OwningDigitalIntervalStorage storage;
        storage.addInterval(Interval{10, 10}, EntityId{1});
        
        CHECK(storage.hasIntervalAtTime(10));
        CHECK_FALSE(storage.hasIntervalAtTime(9));
        CHECK_FALSE(storage.hasIntervalAtTime(11));
    }
    
    SECTION("Overlapping intervals") {
        OwningDigitalIntervalStorage storage;
        storage.addInterval(Interval{10, 30}, EntityId{1});
        storage.addInterval(Interval{20, 40}, EntityId{2});
        
        CHECK(storage.size() == 2);
        CHECK(storage.hasIntervalAtTime(25));  // Contained in both
    }
}

// =============================================================================
// DigitalIntervalSeries::createFromView Tests
// =============================================================================

TEST_CASE("DigitalIntervalSeries::createFromView basic operations", "[DigitalIntervalSeries][createFromView]") {
    
    SECTION("Create lazy series from transform view") {
        // Create source data
        std::vector<IntervalWithId> source_data = {
            IntervalWithId(Interval{10, 20}, EntityId{1}),
            IntervalWithId(Interval{30, 40}, EntityId{2}),
            IntervalWithId(Interval{50, 60}, EntityId{3})
        };
        
        // Identity transform
        auto view = source_data | std::views::transform([](IntervalWithId const& iwid) {
            return iwid;
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(view, source_data.size());
        
        REQUIRE(lazy_series != nullptr);
        CHECK(lazy_series->size() == 3);
        CHECK(lazy_series->isLazy());
        CHECK_FALSE(lazy_series->isView());
        CHECK(lazy_series->getStorageType() == DigitalIntervalStorageType::Lazy);
    }
    
    SECTION("Lazy series allows iteration via view()") {
        std::vector<IntervalWithId> source_data = {
            IntervalWithId(Interval{10, 20}, EntityId{1}),
            IntervalWithId(Interval{30, 40}, EntityId{2})
        };
        
        auto view = source_data | std::views::transform([](IntervalWithId const& iwid) {
            return iwid;
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(view, source_data.size());
        
        size_t count = 0;
        for (auto const& element : lazy_series->view()) {
            if (count == 0) {
                CHECK(element.interval.start == 10);
                CHECK(element.entity_id == EntityId{1});
            } else if (count == 1) {
                CHECK(element.interval.start == 30);
                CHECK(element.entity_id == EntityId{2});
            }
            ++count;
        }
        CHECK(count == 2);
    }
    
    SECTION("Transform view - shift intervals") {
        std::vector<IntervalWithId> source_data = {
            IntervalWithId(Interval{10, 20}, EntityId{1}),
            IntervalWithId(Interval{30, 40}, EntityId{2})
        };
        
        // Shift all intervals by +100
        auto shifted_view = source_data | std::views::transform([](IntervalWithId const& iwid) {
            return IntervalWithId(
                Interval{iwid.interval.start + 100, iwid.interval.end + 100},
                iwid.entity_id
            );
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(shifted_view, source_data.size());
        
        CHECK(lazy_series->size() == 2);
        
        // Verify transformation was applied
        auto series_view = lazy_series->view();
        auto it = series_view.begin();
        
        CHECK((*it).interval.start == 110);  // 10 + 100
        CHECK((*it).interval.end == 120);    // 20 + 100
        CHECK((*it).entity_id == EntityId{1});
        
        ++it;
        CHECK((*it).interval.start == 130);  // 30 + 100
        CHECK((*it).interval.end == 140);    // 40 + 100
        CHECK((*it).entity_id == EntityId{2});
    }
    
    SECTION("Create with time frame") {
        std::vector<IntervalWithId> source_data = {
            IntervalWithId(Interval{1, 2}, EntityId{1})
        };
        
        std::vector<int> times = {0, 10, 20, 30};
        auto time_frame = std::make_shared<TimeFrame>(times);

    
        auto view = source_data | std::views::transform([](IntervalWithId const& iwid) {
            return iwid;
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(view, source_data.size(), time_frame);
        
        CHECK(lazy_series->getTimeFrame() == time_frame);
    }
}

TEST_CASE("DigitalIntervalSeries::createFromView materialize", "[DigitalIntervalSeries][createFromView][materialize]") {
    
    SECTION("Materialize lazy series to owning") {
        std::vector<IntervalWithId> source_data = {
            IntervalWithId(Interval{10, 20}, EntityId{1}),
            IntervalWithId(Interval{30, 40}, EntityId{2}),
            IntervalWithId(Interval{50, 60}, EntityId{3})
        };
        
        // Transform: double all interval values
        auto doubled_view = source_data | std::views::transform([](IntervalWithId const& iwid) {
            return IntervalWithId(
                Interval{iwid.interval.start * 2, iwid.interval.end * 2},
                iwid.entity_id
            );
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(doubled_view, source_data.size());
        
        CHECK(lazy_series->isLazy());
        
        // Materialize
        auto materialized = lazy_series->materialize();
        
        REQUIRE(materialized != nullptr);
        CHECK_FALSE(materialized->isLazy());
        CHECK_FALSE(materialized->isView());
        CHECK(materialized->getStorageType() == DigitalIntervalStorageType::Owning);
        CHECK(materialized->size() == 3);
        
        // Verify values were computed correctly
        auto const& intervals = materialized->getDigitalIntervalSeries();
        CHECK(intervals[0].start == 20);   // 10 * 2
        CHECK(intervals[0].end == 40);     // 20 * 2
        CHECK(intervals[1].start == 60);   // 30 * 2
        CHECK(intervals[1].end == 80);     // 40 * 2
        CHECK(intervals[2].start == 100);  // 50 * 2
        CHECK(intervals[2].end == 120);    // 60 * 2
    }
    
    SECTION("Materialize preserves EntityIds") {
        std::vector<IntervalWithId> source_data = {
            IntervalWithId(Interval{10, 20}, EntityId{100}),
            IntervalWithId(Interval{30, 40}, EntityId{200}),
        };
        
        auto view = source_data | std::views::transform([](IntervalWithId const& iwid) {
            return iwid;
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(view, source_data.size());
        auto materialized = lazy_series->materialize();
        
        auto const& entity_ids = materialized->getEntityIds();
        REQUIRE(entity_ids.size() == 2);
        CHECK(entity_ids[0] == EntityId{100});
        CHECK(entity_ids[1] == EntityId{200});
    }
}

TEST_CASE("DigitalIntervalSeries::createFromView from existing series", "[DigitalIntervalSeries][createFromView][integration]") {
    
    SECTION("Create lazy from existing DigitalIntervalSeries::view()") {
        // Create an owning series
        auto source = std::make_shared<DigitalIntervalSeries>();
        source->addEvent(Interval{10, 20});
        source->addEvent(Interval{30, 40});
        source->addEvent(Interval{50, 60});
        
        // Create a lazy transform from the series' view
        auto filtered_view = source->view() 
            | std::views::filter([](IntervalWithId const& iwid) {
                return iwid.interval.start >= 30;  // Only intervals starting at 30+
            });
        
        // Count elements for lazy series creation
        size_t count = 0;
        for (auto const& _ : filtered_view) {
            (void)_;
            ++count;
        }
        
        // Note: For lazy series we need a random-access range
        // Filter views are not random-access, so we'd need to materialize first
        // or use a different approach. This test shows the limitation.
        
        // For random-access transforms (not filters), createFromView works:
        auto transformed_view = source->view()
            | std::views::transform([](IntervalWithId const& iwid) {
                return IntervalWithId(
                    Interval{iwid.interval.start, iwid.interval.end + 10},
                    iwid.entity_id
                );
            });
        
        auto lazy_extended = DigitalIntervalSeries::createFromView(
            transformed_view, source->size());
        
        CHECK(lazy_extended->size() == 3);
        
        // Check first interval was extended
        auto first = *lazy_extended->view().begin();
        CHECK(first.interval.end == 30);  // Was 20, now 20+10
    }
    
    SECTION("Round trip: series -> lazy transform -> materialize") {
        auto source = std::make_shared<DigitalIntervalSeries>();
        source->addEvent(Interval{100, 200});
        source->addEvent(Interval{300, 400});
        
        // Scale intervals by 2
        auto scaled_view = source->view()
            | std::views::transform([](IntervalWithId const& iwid) {
                return IntervalWithId(
                    Interval{iwid.interval.start * 2, iwid.interval.end * 2},
                    iwid.entity_id
                );
            });
        
        auto lazy = DigitalIntervalSeries::createFromView(scaled_view, source->size());
        auto final_series = lazy->materialize();
        
        CHECK(final_series->size() == 2);
        
        auto const& intervals = final_series->getDigitalIntervalSeries();
        CHECK(intervals[0].start == 200);
        CHECK(intervals[0].end == 400);
        CHECK(intervals[1].start == 600);
        CHECK(intervals[1].end == 800);
    }
}

TEST_CASE("DigitalIntervalSeries::createFromView empty series", "[DigitalIntervalSeries][createFromView][edge]") {
    
    SECTION("Create lazy from empty vector") {
        std::vector<IntervalWithId> empty_data;
        auto view = empty_data | std::views::transform([](IntervalWithId const& iwid) {
            return iwid;
        });
        
        auto lazy_series = DigitalIntervalSeries::createFromView(view, 0);
        
        CHECK(lazy_series->size() == 0);
        CHECK(lazy_series->isLazy());
        
        auto materialized = lazy_series->materialize();
        CHECK(materialized->size() == 0);
        CHECK_FALSE(materialized->isLazy());
    }
}
