/**
 * @file digital_event_storage_test.cpp
 * @brief Unit tests for DigitalEventStorage implementations
 * 
 * Tests the storage abstraction layer for DigitalEventSeries:
 * - OwningDigitalEventStorage: Basic owning storage with SoA layout
 * - ViewDigitalEventStorage: Zero-copy view/filter over owning storage
 * - LazyDigitalEventStorage: On-demand computation from transform views
 * - DigitalEventStorageWrapper: Type-erased wrapper with cache optimization
 */

#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/storage/DigitalEventStorage.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>
#include <unordered_set>

// =============================================================================
// OwningDigitalEventStorage Tests
// =============================================================================

TEST_CASE("OwningDigitalEventStorage basic operations", "[DigitalEventStorage]") {
    OwningDigitalEventStorage storage;
    
    SECTION("Empty storage") {
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
        CHECK(storage.getStorageType() == DigitalEventStorageType::Owning);
        CHECK_FALSE(storage.isView());
        CHECK_FALSE(storage.isLazy());
    }
    
    SECTION("Add single event") {
        bool added = storage.addEvent(TimeFrameIndex{10}, EntityId{100});
        
        CHECK(added);
        CHECK(storage.size() == 1);
        CHECK_FALSE(storage.empty());
        
        CHECK(storage.getEvent(0) == TimeFrameIndex{10});
        CHECK(storage.getEntityId(0) == EntityId{100});
    }
    
    SECTION("Events are sorted") {
        storage.addEvent(TimeFrameIndex{30}, EntityId{3});
        storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        storage.addEvent(TimeFrameIndex{20}, EntityId{2});
        
        CHECK(storage.size() == 3);
        CHECK(storage.getEvent(0) == TimeFrameIndex{10});
        CHECK(storage.getEvent(1) == TimeFrameIndex{20});
        CHECK(storage.getEvent(2) == TimeFrameIndex{30});
    }
    
    SECTION("Duplicate events are rejected") {
        bool added1 = storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        bool added2 = storage.addEvent(TimeFrameIndex{10}, EntityId{2});
        
        CHECK(added1);
        CHECK_FALSE(added2);
        CHECK(storage.size() == 1);
        CHECK(storage.getEntityId(0) == EntityId{1}); // First one kept
    }
    
    SECTION("Remove event by time") {
        storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        storage.addEvent(TimeFrameIndex{20}, EntityId{2});
        storage.addEvent(TimeFrameIndex{30}, EntityId{3});
        
        bool removed = storage.removeEvent(TimeFrameIndex{20});
        
        CHECK(removed);
        CHECK(storage.size() == 2);
        CHECK(storage.getEvent(0) == TimeFrameIndex{10});
        CHECK(storage.getEvent(1) == TimeFrameIndex{30});
        
        // Remove non-existent
        bool removed2 = storage.removeEvent(TimeFrameIndex{100});
        CHECK_FALSE(removed2);
    }
    
    SECTION("Remove event by EntityId") {
        storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        storage.addEvent(TimeFrameIndex{20}, EntityId{2});
        storage.addEvent(TimeFrameIndex{30}, EntityId{3});
        
        bool removed = storage.removeByEntityId(EntityId{2});
        
        CHECK(removed);
        CHECK(storage.size() == 2);
        
        auto opt = storage.findByEntityId(EntityId{2});
        CHECK_FALSE(opt.has_value());
    }
    
    SECTION("Clear") {
        storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        storage.addEvent(TimeFrameIndex{20}, EntityId{2});
        
        storage.clear();
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
    }
    
    SECTION("Find by time") {
        storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        storage.addEvent(TimeFrameIndex{20}, EntityId{2});
        storage.addEvent(TimeFrameIndex{30}, EntityId{3});
        
        auto found = storage.findByTime(TimeFrameIndex{20});
        REQUIRE(found.has_value());
        CHECK(*found == 1);
        
        auto not_found = storage.findByTime(TimeFrameIndex{25});
        CHECK_FALSE(not_found.has_value());
    }
    
    SECTION("Find by EntityId") {
        storage.addEvent(TimeFrameIndex{10}, EntityId{100});
        storage.addEvent(TimeFrameIndex{20}, EntityId{200});
        storage.addEvent(TimeFrameIndex{30}, EntityId{300});
        
        auto found = storage.findByEntityId(EntityId{200});
        REQUIRE(found.has_value());
        CHECK(storage.getEvent(*found) == TimeFrameIndex{20});
        
        auto not_found = storage.findByEntityId(EntityId{999});
        CHECK_FALSE(not_found.has_value());
    }
    
    SECTION("Get time range") {
        storage.addEvent(TimeFrameIndex{10}, EntityId{1});
        storage.addEvent(TimeFrameIndex{20}, EntityId{2});
        storage.addEvent(TimeFrameIndex{30}, EntityId{3});
        storage.addEvent(TimeFrameIndex{40}, EntityId{4});
        storage.addEvent(TimeFrameIndex{50}, EntityId{5});
        
        auto [start, end] = storage.getTimeRange(TimeFrameIndex{15}, TimeFrameIndex{35});
        CHECK(start == 1);  // Index of event at t=20
        CHECK(end == 3);    // Index after event at t=30
    }
}

TEST_CASE("OwningDigitalEventStorage construction from vectors", "[DigitalEventStorage]") {
    
    SECTION("Construct from event vector only") {
        std::vector<TimeFrameIndex> events = {
            TimeFrameIndex{30}, TimeFrameIndex{10}, TimeFrameIndex{20}
        };
        OwningDigitalEventStorage storage{events};
        
        CHECK(storage.size() == 3);
        // Events should be sorted
        CHECK(storage.getEvent(0) == TimeFrameIndex{10});
        CHECK(storage.getEvent(1) == TimeFrameIndex{20});
        CHECK(storage.getEvent(2) == TimeFrameIndex{30});
        // Entity IDs should be zero
        CHECK(storage.getEntityId(0) == EntityId{0});
    }
    
    SECTION("Construct from event and entity ID vectors") {
        std::vector<TimeFrameIndex> events = {
            TimeFrameIndex{30}, TimeFrameIndex{10}, TimeFrameIndex{20}
        };
        std::vector<EntityId> ids = {EntityId{3}, EntityId{1}, EntityId{2}};
        
        OwningDigitalEventStorage storage{events, ids};
        
        CHECK(storage.size() == 3);
        // Events and IDs should be sorted together
        CHECK(storage.getEvent(0) == TimeFrameIndex{10});
        CHECK(storage.getEntityId(0) == EntityId{1});
        CHECK(storage.getEvent(1) == TimeFrameIndex{20});
        CHECK(storage.getEntityId(1) == EntityId{2});
    }
}

TEST_CASE("OwningDigitalEventStorage cache optimization", "[DigitalEventStorage][cache]") {
    OwningDigitalEventStorage storage;
    
    // Add some events
    for (int i = 0; i < 10; ++i) {
        storage.addEvent(TimeFrameIndex{i * 10}, EntityId{static_cast<uint32_t>(i)});
    }
    
    SECTION("Cache is valid for owning storage") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 10);
    }
    
    SECTION("Cache provides direct access") {
        auto cache = storage.tryGetCache();
        
        for (size_t i = 0; i < cache.cache_size; ++i) {
            CHECK(cache.getEvent(i) == storage.getEvent(i));
            CHECK(cache.getEntityId(i) == storage.getEntityId(i));
        }
    }
}

// =============================================================================
// ViewDigitalEventStorage Tests
// =============================================================================

TEST_CASE("ViewDigitalEventStorage basic operations", "[DigitalEventStorage][view]") {
    // Create source storage
    auto source = std::make_shared<OwningDigitalEventStorage>();
    source->addEvent(TimeFrameIndex{10}, EntityId{1});
    source->addEvent(TimeFrameIndex{20}, EntityId{2});
    source->addEvent(TimeFrameIndex{30}, EntityId{3});
    source->addEvent(TimeFrameIndex{40}, EntityId{4});
    source->addEvent(TimeFrameIndex{50}, EntityId{5});
    
    ViewDigitalEventStorage view{source};
    
    SECTION("Empty view initially") {
        CHECK(view.size() == 0);
        CHECK(view.empty());
        CHECK(view.getStorageType() == DigitalEventStorageType::View);
        CHECK(view.isView());
        CHECK_FALSE(view.isLazy());
    }
    
    SECTION("Set all indices") {
        view.setAllIndices();
        
        CHECK(view.size() == 5);
        CHECK(view.getEvent(0) == TimeFrameIndex{10});
        CHECK(view.getEvent(4) == TimeFrameIndex{50});
    }
    
    SECTION("Filter by time range") {
        view.filterByTimeRange(TimeFrameIndex{15}, TimeFrameIndex{35});
        
        CHECK(view.size() == 2);
        CHECK(view.getEvent(0) == TimeFrameIndex{20});
        CHECK(view.getEvent(1) == TimeFrameIndex{30});
    }
    
    SECTION("Filter by EntityIds") {
        std::unordered_set<EntityId> ids{EntityId{1}, EntityId{3}, EntityId{5}};
        view.filterByEntityIds(ids);
        
        CHECK(view.size() == 3);
        CHECK(view.getEntityId(0) == EntityId{1});
        CHECK(view.getEntityId(1) == EntityId{3});
        CHECK(view.getEntityId(2) == EntityId{5});
    }
    
    SECTION("Find operations in view") {
        view.setAllIndices();
        
        auto found = view.findByTime(TimeFrameIndex{30});
        REQUIRE(found.has_value());
        CHECK(*found == 2);
        
        auto found_id = view.findByEntityId(EntityId{3});
        REQUIRE(found_id.has_value());
        CHECK(view.getEvent(*found_id) == TimeFrameIndex{30});
    }
    
    SECTION("Get time range in view") {
        view.setAllIndices();
        
        auto [start, end] = view.getTimeRange(TimeFrameIndex{15}, TimeFrameIndex{35});
        CHECK(start == 1);
        CHECK(end == 3);
    }
}

TEST_CASE("ViewDigitalEventStorage cache optimization", "[DigitalEventStorage][view][cache]") {
    auto source = std::make_shared<OwningDigitalEventStorage>();
    for (int i = 0; i < 10; ++i) {
        source->addEvent(TimeFrameIndex{i * 10}, EntityId{static_cast<uint32_t>(i)});
    }
    
    ViewDigitalEventStorage view{source};
    
    SECTION("Contiguous view returns valid cache") {
        view.setAllIndices();
        
        auto cache = view.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 10);
    }
    
    SECTION("Non-contiguous view returns invalid cache") {
        // Filter to non-contiguous indices
        std::unordered_set<EntityId> ids{EntityId{0}, EntityId{2}, EntityId{4}};
        view.filterByEntityIds(ids);
        
        auto cache = view.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
    
    SECTION("Contiguous time range returns valid cache") {
        // Events at indices 2,3,4 form a contiguous range
        view.filterByTimeRange(TimeFrameIndex{20}, TimeFrameIndex{40});
        
        auto cache = view.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 3);
    }
}

// =============================================================================
// LazyDigitalEventStorage Tests
// =============================================================================

TEST_CASE("LazyDigitalEventStorage basic operations", "[DigitalEventStorage][lazy]") {
    
    // Create a simple transform view that doubles event times
    std::vector<std::pair<TimeFrameIndex, EntityId>> data = {
        {TimeFrameIndex{10}, EntityId{1}},
        {TimeFrameIndex{20}, EntityId{2}},
        {TimeFrameIndex{30}, EntityId{3}}
    };
    
    auto view = data | std::views::transform([](auto const& p) {
        return std::pair{TimeFrameIndex{p.first.getValue() * 2}, p.second};
    });
    
    LazyDigitalEventStorage<decltype(view)> lazy{view, data.size()};
    
    SECTION("Basic properties") {
        CHECK(lazy.size() == 3);
        CHECK_FALSE(lazy.empty());
        CHECK(lazy.getStorageType() == DigitalEventStorageType::Lazy);
        CHECK_FALSE(lazy.isView());
        CHECK(lazy.isLazy());
    }
    
    SECTION("Element access computes on demand") {
        CHECK(lazy.getEvent(0) == TimeFrameIndex{20});  // 10 * 2
        CHECK(lazy.getEvent(1) == TimeFrameIndex{40});  // 20 * 2
        CHECK(lazy.getEvent(2) == TimeFrameIndex{60});  // 30 * 2
        
        CHECK(lazy.getEntityId(0) == EntityId{1});
        CHECK(lazy.getEntityId(1) == EntityId{2});
        CHECK(lazy.getEntityId(2) == EntityId{3});
    }
    
    SECTION("Cache is invalid for lazy storage") {
        auto cache = lazy.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
    
    SECTION("Find by EntityId works") {
        auto found = lazy.findByEntityId(EntityId{2});
        REQUIRE(found.has_value());
        CHECK(lazy.getEvent(*found) == TimeFrameIndex{40});
    }
}

// =============================================================================
// DigitalEventStorageWrapper Tests
// =============================================================================

TEST_CASE("DigitalEventStorageWrapper with owning storage", "[DigitalEventStorage][wrapper]") {
    DigitalEventStorageWrapper wrapper;
    
    SECTION("Default constructor creates owning storage") {
        CHECK(wrapper.getStorageType() == DigitalEventStorageType::Owning);
        CHECK(wrapper.empty());
    }
    
    SECTION("Mutation operations work") {
        wrapper.addEvent(TimeFrameIndex{10}, EntityId{1});
        wrapper.addEvent(TimeFrameIndex{20}, EntityId{2});
        
        CHECK(wrapper.size() == 2);
        CHECK(wrapper.getEvent(0) == TimeFrameIndex{10});
        
        wrapper.removeEvent(TimeFrameIndex{10});
        CHECK(wrapper.size() == 1);
        
        wrapper.clear();
        CHECK(wrapper.empty());
    }
    
    SECTION("Cache optimization works") {
        wrapper.addEvent(TimeFrameIndex{10}, EntityId{1});
        wrapper.addEvent(TimeFrameIndex{20}, EntityId{2});
        
        auto cache = wrapper.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 2);
    }
    
    SECTION("Type access via tryGet") {
        wrapper.addEvent(TimeFrameIndex{10}, EntityId{1});
        
        auto* owning = wrapper.tryGetMutableOwning();
        REQUIRE(owning != nullptr);
        CHECK(owning->size() == 1);
        
        auto const* const_owning = wrapper.tryGetOwning();
        REQUIRE(const_owning != nullptr);
    }
}

TEST_CASE("DigitalEventStorageWrapper with view storage", "[DigitalEventStorage][wrapper]") {
    // Create source
    auto source = std::make_shared<OwningDigitalEventStorage>();
    source->addEvent(TimeFrameIndex{10}, EntityId{1});
    source->addEvent(TimeFrameIndex{20}, EntityId{2});
    source->addEvent(TimeFrameIndex{30}, EntityId{3});
    
    // Create view
    ViewDigitalEventStorage view{source};
    view.filterByTimeRange(TimeFrameIndex{15}, TimeFrameIndex{25});
    
    DigitalEventStorageWrapper wrapper{std::move(view)};
    
    SECTION("View storage properties") {
        CHECK(wrapper.getStorageType() == DigitalEventStorageType::View);
        CHECK(wrapper.isView());
        CHECK(wrapper.size() == 1);
        CHECK(wrapper.getEvent(0) == TimeFrameIndex{20});
    }
    
    SECTION("Mutation operations throw for view") {
        CHECK_THROWS(wrapper.addEvent(TimeFrameIndex{100}, EntityId{100}));
        CHECK_THROWS(wrapper.removeEvent(TimeFrameIndex{20}));
        CHECK_THROWS(wrapper.clear());
    }
    
    SECTION("tryGetMutableOwning returns nullptr for view") {
        CHECK(wrapper.tryGetMutableOwning() == nullptr);
    }
}

// =============================================================================
// DigitalEventSeries Integration Tests
// =============================================================================

TEST_CASE("DigitalEventSeries with new storage", "[DigitalEventSeries]") {
    
    SECTION("Default construction") {
        DigitalEventSeries series;
        CHECK(series.size() == 0);
        CHECK_FALSE(series.isView());
        CHECK_FALSE(series.isLazy());
    }
    
    SECTION("Construction from vector") {
        std::vector<TimeFrameIndex> events = {
            TimeFrameIndex{30}, TimeFrameIndex{10}, TimeFrameIndex{20}
        };
        DigitalEventSeries series{events};
        
        CHECK(series.size() == 3);
        
        // Events should be sorted
        auto const& sorted = series.view();
        CHECK(sorted[0].time() == TimeFrameIndex{10});
        CHECK(sorted[1].time() == TimeFrameIndex{20});
        CHECK(sorted[2].time() == TimeFrameIndex{30});
    }
    
    SECTION("Add and remove events") {
        DigitalEventSeries series;
        
        series.addEvent(TimeFrameIndex{20});
        series.addEvent(TimeFrameIndex{10});
        series.addEvent(TimeFrameIndex{30});
        
        CHECK(series.size() == 3);
        
        bool removed = series.removeEvent(TimeFrameIndex{20});
        CHECK(removed);
        CHECK(series.size() == 2);
    }
    
    SECTION("Clear") {
        std::vector<TimeFrameIndex> events = {TimeFrameIndex{10}, TimeFrameIndex{20}};
        DigitalEventSeries series{events};
        
        series.clear();
        CHECK(series.size() == 0);
    }
    
    SECTION("Iterator access") {
        std::vector<TimeFrameIndex> events = {TimeFrameIndex{10}, TimeFrameIndex{20}};
        DigitalEventSeries series{events};
        
        auto view = series.view();
        CHECK(view.size() == 2);
        
        auto it = view.begin();
        CHECK((*it).event_time == TimeFrameIndex{10});
        ++it;
        CHECK((*it).event_time == TimeFrameIndex{20});
    }
}

TEST_CASE("DigitalEventSeries view creation", "[DigitalEventSeries][view]") {
    std::vector<TimeFrameIndex> events = {
        TimeFrameIndex{10}, TimeFrameIndex{20}, TimeFrameIndex{30},
        TimeFrameIndex{40}, TimeFrameIndex{50}
    };
    auto source = std::make_shared<DigitalEventSeries>(events);
    
    SECTION("Create view by time range") {
        auto view_series = DigitalEventSeries::createView(source, TimeFrameIndex{15}, TimeFrameIndex{35});
        
        REQUIRE(view_series->size() == 2);
        CHECK(view_series->isView());
        
        auto const& event_vec = view_series->view();
        REQUIRE(event_vec[0].time() == TimeFrameIndex{20});
        REQUIRE(event_vec[1].time() == TimeFrameIndex{30});
    }
}

TEST_CASE("DigitalEventSeries view creation with DataManager", "[DigitalEventSeries][view][entity]") {
    // Use DataManager to get proper EntityId registration
    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40, 50, 60});
    data_manager->setTime(TimeKey("test_time"), time_frame);
    
    data_manager->setData<DigitalEventSeries>("source_events", TimeKey("test_time"));
    auto source = data_manager->getData<DigitalEventSeries>("source_events");
    
    // Add events - they will get unique EntityIds from the registry
    source->addEvent(TimeFrameIndex{10});
    source->addEvent(TimeFrameIndex{20});
    source->addEvent(TimeFrameIndex{30});
    source->addEvent(TimeFrameIndex{40});
    source->addEvent(TimeFrameIndex{50});
    
    REQUIRE(source->size() == 5);
    
    SECTION("Create view by EntityIds") {

        // Verify all IDs are unique
        std::unordered_set<EntityId> all_ids;
        for (auto const& event : source->view()) {
            all_ids.insert(event.id());
        }
        REQUIRE(all_ids.size() == 5);
        
        // Filter to keep only events at indices 0, 2, 4
        std::unordered_set<EntityId> filter_ids{source->view()[0].id(), 
            source->view()[2].id(), 
            source->view()[4].id()};
        auto view_series = DigitalEventSeries::createView(source, filter_ids);
        
        REQUIRE(view_series->size() == 3);
        REQUIRE(view_series->isView());
        
        // Verify the events are the right ones
        auto const& event_vec = view_series->view();
        REQUIRE(event_vec[0].time() == TimeFrameIndex{10});
        REQUIRE(event_vec[1].time() == TimeFrameIndex{30});
        REQUIRE(event_vec[2].time() == TimeFrameIndex{50});
    }
}

TEST_CASE("DigitalEventSeries materialization", "[DigitalEventSeries][materialize]") {
    std::vector<TimeFrameIndex> events = {
        TimeFrameIndex{10}, TimeFrameIndex{20}, TimeFrameIndex{30}
    };
    auto source = std::make_shared<DigitalEventSeries>(events);
    
    // Create view
    auto view_series = DigitalEventSeries::createView(source, TimeFrameIndex{15}, TimeFrameIndex{35});
    CHECK(view_series->isView());
    
    // Materialize
    auto materialized = view_series->materialize();
    CHECK_FALSE(materialized->isView());
    CHECK_FALSE(materialized->isLazy());
    CHECK(materialized->getStorageType() == DigitalEventStorageType::Owning);
    
    // Content should be the same
    REQUIRE(materialized->size() == 2);
    auto const& events_mat = materialized->view();
    REQUIRE(events_mat[0].time() == TimeFrameIndex{20});
    REQUIRE(events_mat[1].time() == TimeFrameIndex{30});
    
    // Should now be mutable
    materialized->addEvent(TimeFrameIndex{25});
    CHECK(materialized->size() == 3);
}

TEST_CASE("DigitalEventSeries lazy creation", "[DigitalEventSeries][lazy]") {
    // Create a transform view
    std::vector<EventWithId> data = {
        EventWithId{TimeFrameIndex{10}, EntityId{1}},
        EventWithId{TimeFrameIndex{20}, EntityId{2}},
        EventWithId{TimeFrameIndex{30}, EntityId{3}}
    };
    
    auto view = data | std::views::transform([](EventWithId const& e) {
        return EventWithId{TimeFrameIndex{e.event_time.getValue() * 2}, e.entity_id};
    });
    
    auto lazy_series = DigitalEventSeries::createFromView(view, data.size());
    
    CHECK(lazy_series->size() == 3);
    CHECK(lazy_series->isLazy());
    
    // Access computes on demand
    auto const& events = lazy_series->view();
    CHECK(events[0].time() == TimeFrameIndex{20});  // 10 * 2
    CHECK(events[1].time() == TimeFrameIndex{40});  // 20 * 2
    CHECK(events[2].time() == TimeFrameIndex{60});  // 30 * 2
    
    // Materialize lazy series
    auto materialized = lazy_series->materialize();
    CHECK_FALSE(materialized->isLazy());
    CHECK(materialized->size() == 3);
}

// =============================================================================
// Public Interface Tests Across All Storage Backends
// =============================================================================

namespace {

/**
 * @brief Helper struct to create test series with owning storage
 */
struct OwningBackend {
    static std::shared_ptr<DigitalEventSeries> create() {
        std::vector<TimeFrameIndex> events = {
            TimeFrameIndex{10}, TimeFrameIndex{20}, TimeFrameIndex{30},
            TimeFrameIndex{40}, TimeFrameIndex{50}
        };
        return std::make_shared<DigitalEventSeries>(events);
    }
    
    static std::shared_ptr<DigitalEventSeries> createEmpty() {
        return std::make_shared<DigitalEventSeries>();
    }
    
    static constexpr bool is_mutable = true;
    static constexpr DigitalEventStorageType storage_type = DigitalEventStorageType::Owning;
};

/**
 * @brief Helper struct to create test series with view storage
 */
struct ViewBackend {
    static std::shared_ptr<DigitalEventSeries> create() {
        auto source = OwningBackend::create();
        // Create view of all events
        return DigitalEventSeries::createView(source, TimeFrameIndex{0}, TimeFrameIndex{100});
    }
    
    static std::shared_ptr<DigitalEventSeries> createEmpty() {
        auto source = std::make_shared<DigitalEventSeries>();
        return DigitalEventSeries::createView(source, TimeFrameIndex{0}, TimeFrameIndex{100});
    }
    
    static constexpr bool is_mutable = false;
    static constexpr DigitalEventStorageType storage_type = DigitalEventStorageType::View;
};

/**
 * @brief Helper struct to create test series with lazy storage
 */
struct LazyBackend {
    static std::shared_ptr<DigitalEventSeries> create() {
        // Create source data
        static std::vector<EventWithId> data = {
            EventWithId{TimeFrameIndex{10}, EntityId{1}},
            EventWithId{TimeFrameIndex{20}, EntityId{2}},
            EventWithId{TimeFrameIndex{30}, EntityId{3}},
            EventWithId{TimeFrameIndex{40}, EntityId{4}},
            EventWithId{TimeFrameIndex{50}, EntityId{5}}
        };
        
        // Identity transform to test lazy evaluation
        auto view = data | std::views::transform([](EventWithId const& e) {
            return e;
        });
        
        return DigitalEventSeries::createFromView(view, data.size());
    }
    
    static std::shared_ptr<DigitalEventSeries> createEmpty() {
        static std::vector<EventWithId> empty_data;
        auto view = empty_data | std::views::transform([](EventWithId const& e) {
            return e;
        });
        return DigitalEventSeries::createFromView(view, 0);
    }
    
    static constexpr bool is_mutable = false;
    static constexpr DigitalEventStorageType storage_type = DigitalEventStorageType::Lazy;
};

} // anonymous namespace

// =============================================================================
// view() Tests for All Backends
// =============================================================================

TEST_CASE("DigitalEventSeries::view() - Owning backend", "[DigitalEventSeries][interface][owning]") {
    auto series = OwningBackend::create();
    
    SECTION("view() returns correct EventWithId objects") {
        auto v = series->view();
        
        std::vector<TimeFrameIndex> times;
        std::vector<EntityId> ids;
        
        for (auto event : v) {
            times.push_back(event.time());
            ids.push_back(event.id());
            // value() should equal time() for events
            CHECK(event.value() == event.time());
        }
        
        REQUIRE(times.size() == 5);
        CHECK(times[0] == TimeFrameIndex{10});
        CHECK(times[1] == TimeFrameIndex{20});
        CHECK(times[2] == TimeFrameIndex{30});
        CHECK(times[3] == TimeFrameIndex{40});
        CHECK(times[4] == TimeFrameIndex{50});
    }
    
    SECTION("view() supports range algorithms") {
        auto v = series->view();
        
        // Count
        CHECK(std::ranges::distance(v) == 5);
        
        // Find
        auto found = std::ranges::find_if(v, [](auto const& e) {
            return e.time() == TimeFrameIndex{30};
        });
        REQUIRE(found != v.end());
        CHECK((*found).time() == TimeFrameIndex{30});
    }
    
    SECTION("view() is lazy (no allocation on creation)") {
        // This should not allocate or iterate
        auto v = series->view();
        // Only iterating should access storage
        auto first = *v.begin();
        CHECK(first.time() == TimeFrameIndex{10});
    }
}

TEST_CASE("DigitalEventSeries::view() - View backend", "[DigitalEventSeries][interface][view]") {
    auto series = ViewBackend::create();
    
    SECTION("view() returns correct EventWithId objects") {
        auto v = series->view();
        
        std::vector<TimeFrameIndex> times;
        for (auto event : v) {
            times.push_back(event.time());
        }
        
        REQUIRE(times.size() == 5);
        CHECK(times[0] == TimeFrameIndex{10});
        CHECK(times[4] == TimeFrameIndex{50});
    }
    
    SECTION("view() supports range algorithms") {
        auto v = series->view();
        CHECK(std::ranges::distance(v) == 5);
    }
}

TEST_CASE("DigitalEventSeries::view() - Lazy backend", "[DigitalEventSeries][interface][lazy]") {
    auto series = LazyBackend::create();
    
    SECTION("view() returns correct EventWithId objects") {
        auto v = series->view();
        
        std::vector<TimeFrameIndex> times;
        for (auto event : v) {
            times.push_back(event.time());
        }
        
        REQUIRE(times.size() == 5);
        CHECK(times[0] == TimeFrameIndex{10});
        CHECK(times[4] == TimeFrameIndex{50});
    }
    
    SECTION("view() computes elements on demand") {
        auto v = series->view();
        
        // Access specific element without iterating all
        auto it = v.begin();
        std::advance(it, 2);
        CHECK((*it).time() == TimeFrameIndex{30});
    }
}

TEST_CASE("DigitalEventSeries::view() - Empty series all backends", "[DigitalEventSeries][interface][empty]") {
    SECTION("Owning empty") {
        auto series = OwningBackend::createEmpty();
        auto v = series->view();
        CHECK(std::ranges::distance(v) == 0);
        CHECK(v.begin() == v.end());
    }
    
    SECTION("View empty") {
        auto series = ViewBackend::createEmpty();
        auto v = series->view();
        CHECK(std::ranges::distance(v) == 0);
    }
    
    SECTION("Lazy empty") {
        auto series = LazyBackend::createEmpty();
        auto v = series->view();
        CHECK(std::ranges::distance(v) == 0);
    }
}

// =============================================================================
// size() Tests for All Backends
// =============================================================================

TEST_CASE("DigitalEventSeries::size() - All backends", "[DigitalEventSeries][interface][size]") {
    SECTION("Owning backend") {
        auto series = OwningBackend::create();
        CHECK(series->size() == 5);
        
        auto empty = OwningBackend::createEmpty();
        CHECK(empty->size() == 0);
    }
    
    SECTION("View backend") {
        auto series = ViewBackend::create();
        CHECK(series->size() == 5);
    }
    
    SECTION("Lazy backend") {
        auto series = LazyBackend::create();
        CHECK(series->size() == 5);
    }
}

// =============================================================================
// Storage Type Query Tests
// =============================================================================

TEST_CASE("DigitalEventSeries storage type queries", "[DigitalEventSeries][interface][storage]") {
    SECTION("Owning backend") {
        auto series = OwningBackend::create();
        CHECK(series->getStorageType() == DigitalEventStorageType::Owning);
        CHECK_FALSE(series->isView());
        CHECK_FALSE(series->isLazy());
    }
    
    SECTION("View backend") {
        auto series = ViewBackend::create();
        CHECK(series->getStorageType() == DigitalEventStorageType::View);
        CHECK(series->isView());
        CHECK_FALSE(series->isLazy());
    }
    
    SECTION("Lazy backend") {
        auto series = LazyBackend::create();
        CHECK(series->getStorageType() == DigitalEventStorageType::Lazy);
        CHECK_FALSE(series->isView());
        CHECK(series->isLazy());
    }
}

// =============================================================================
// Mutation Tests (should work for Owning, throw for View/Lazy)
// =============================================================================

TEST_CASE("DigitalEventSeries mutation - Owning backend", "[DigitalEventSeries][interface][mutation][owning]") {
    auto series = OwningBackend::createEmpty();
    
    SECTION("addEvent works") {
        series->addEvent(TimeFrameIndex{100});
        CHECK(series->size() == 1);
        
        series->addEvent(TimeFrameIndex{50});
        CHECK(series->size() == 2);
        
        // Should be sorted
        auto v = series->view();
        auto it = v.begin();
        CHECK((*it).time() == TimeFrameIndex{50});
        ++it;
        CHECK((*it).time() == TimeFrameIndex{100});
    }
    
    SECTION("removeEvent works") {
        series->addEvent(TimeFrameIndex{10});
        series->addEvent(TimeFrameIndex{20});
        series->addEvent(TimeFrameIndex{30});
        
        bool removed = series->removeEvent(TimeFrameIndex{20});
        CHECK(removed);
        CHECK(series->size() == 2);
        
        // Non-existent event
        bool not_removed = series->removeEvent(TimeFrameIndex{999});
        CHECK_FALSE(not_removed);
    }
    
    SECTION("clear works") {
        series->addEvent(TimeFrameIndex{10});
        series->addEvent(TimeFrameIndex{20});
        
        series->clear();
        CHECK(series->size() == 0);
    }
}

TEST_CASE("DigitalEventSeries mutation - View backend throws", "[DigitalEventSeries][interface][mutation][view]") {
    auto series = ViewBackend::create();
    
    SECTION("addEvent throws") {
        CHECK_THROWS_AS(series->addEvent(TimeFrameIndex{100}), std::runtime_error);
    }
    
    SECTION("removeEvent throws") {
        CHECK_THROWS_AS(series->removeEvent(TimeFrameIndex{10}), std::runtime_error);
    }
    
    SECTION("clear throws") {
        CHECK_THROWS_AS(series->clear(), std::runtime_error);
    }
}

TEST_CASE("DigitalEventSeries mutation - Lazy backend throws", "[DigitalEventSeries][interface][mutation][lazy]") {
    auto series = LazyBackend::create();
    
    SECTION("addEvent throws") {
        CHECK_THROWS_AS(series->addEvent(TimeFrameIndex{100}), std::runtime_error);
    }
    
    SECTION("removeEvent throws") {
        CHECK_THROWS_AS(series->removeEvent(TimeFrameIndex{10}), std::runtime_error);
    }
    
    SECTION("clear throws") {
        CHECK_THROWS_AS(series->clear(), std::runtime_error);
    }
}

// =============================================================================
// materialize() Tests
// =============================================================================

TEST_CASE("DigitalEventSeries::materialize() - All backends", "[DigitalEventSeries][interface][materialize]") {
    SECTION("Owning to Owning") {
        auto series = OwningBackend::create();
        auto materialized = series->materialize();
        
        CHECK(materialized->getStorageType() == DigitalEventStorageType::Owning);
        CHECK(materialized->size() == 5);
        
        // Should be independent copy
        series->clear();
        CHECK(materialized->size() == 5);
    }
    
    SECTION("View to Owning") {
        auto series = ViewBackend::create();
        auto materialized = series->materialize();
        
        CHECK(materialized->getStorageType() == DigitalEventStorageType::Owning);
        CHECK(materialized->size() == 5);
        CHECK_FALSE(materialized->isView());
        
        // Should now be mutable
        materialized->addEvent(TimeFrameIndex{25});
        CHECK(materialized->size() == 6);
    }
    
    SECTION("Lazy to Owning") {
        auto series = LazyBackend::create();
        auto materialized = series->materialize();
        
        CHECK(materialized->getStorageType() == DigitalEventStorageType::Owning);
        CHECK(materialized->size() == 5);
        CHECK_FALSE(materialized->isLazy());
        
        // Should now be mutable
        materialized->addEvent(TimeFrameIndex{25});
        CHECK(materialized->size() == 6);
    }
    
    SECTION("Materialized content matches original") {
        auto owning = OwningBackend::create();
        auto view_series = ViewBackend::create();
        auto lazy = LazyBackend::create();
        
        auto mat_owning = owning->materialize();
        auto mat_view = view_series->materialize();
        auto mat_lazy = lazy->materialize();
        
        // All should have same events
        auto check_events = [](std::shared_ptr<DigitalEventSeries> s) {
            auto v = s->view();
            std::vector<TimeFrameIndex> times;
            for (auto e : v) { times.push_back(e.time()); }
            return times;
        };
        
        auto times_owning = check_events(mat_owning);
        auto times_view = check_events(mat_view);
        auto times_lazy = check_events(mat_lazy);
        
        CHECK(times_owning == times_view);
        CHECK(times_owning == times_lazy);
    }
}

// =============================================================================
// TimeFrame Integration Tests
// =============================================================================

TEST_CASE("DigitalEventSeries TimeFrame integration", "[DigitalEventSeries][interface][timeframe]") {
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100});
    
    SECTION("setTimeFrame and getTimeFrame") {
        auto series = OwningBackend::create();
        CHECK(series->getTimeFrame() == nullptr);
        
        series->setTimeFrame(time_frame);
        CHECK(series->getTimeFrame() == time_frame);
    }
    
    SECTION("getEventsInRange with same TimeFrame") {
        auto series = OwningBackend::create();
        series->setTimeFrame(time_frame);
        
        auto range = series->viewTimesInRange(TimeFrameIndex{15}, TimeFrameIndex{35}, *time_frame);
        
        std::vector<TimeFrameIndex> events;
        for (auto e : range) {
            events.push_back(e);
        }
        
        // Should get events at 20 and 30
        CHECK(events.size() == 2);
        CHECK(events[0] == TimeFrameIndex{20});
        CHECK(events[1] == TimeFrameIndex{30});
    }
}

// =============================================================================
// viewInRange Tests
// =============================================================================

TEST_CASE("DigitalEventSeries::viewInRange - All backends", "[DigitalEventSeries][interface][range]") {
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40, 50, 60});
    
    SECTION("Owning backend range query") {
        auto series = OwningBackend::create();
        series->setTimeFrame(time_frame);
        
        auto range = series->viewInRange(TimeFrameIndex{15}, TimeFrameIndex{45}, *time_frame);
        
        std::vector<TimeFrameIndex> events;
        for (auto e : range) {
            events.push_back(e.value());
        }
        
        CHECK(events.size() == 3);
        CHECK(events[0] == TimeFrameIndex{20});
        CHECK(events[1] == TimeFrameIndex{30});
        CHECK(events[2] == TimeFrameIndex{40});
    }
    
    SECTION("View backend range query") {
        auto series = ViewBackend::create();
        series->setTimeFrame(time_frame);
        
        auto range = series->viewInRange(TimeFrameIndex{25}, TimeFrameIndex{55}, *time_frame);
        
        std::vector<TimeFrameIndex> events;
        for (auto e : range) {
            events.push_back(e.value());
        }
        
        CHECK(events.size() == 3);
        CHECK(events[0] == TimeFrameIndex{30});
        CHECK(events[1] == TimeFrameIndex{40});
        CHECK(events[2] == TimeFrameIndex{50});
    }
    
    SECTION("Lazy backend range query") {
        auto series = LazyBackend::create();
        series->setTimeFrame(time_frame);
        
        auto range = series->viewInRange(TimeFrameIndex{10}, TimeFrameIndex{30}, *time_frame);
        
        std::vector<TimeFrameIndex> events;
        for (auto e : range) {
            events.push_back(e.value());
        }
        
        CHECK(events.size() == 3);
        CHECK(events[0] == TimeFrameIndex{10});
        CHECK(events[1] == TimeFrameIndex{20});
        CHECK(events[2] == TimeFrameIndex{30});
    }
    
    SECTION("Empty range returns empty") {
        auto series = OwningBackend::create();
        auto range = series->viewInRange(TimeFrameIndex{100}, TimeFrameIndex{200}, *time_frame);
        CHECK(std::ranges::distance(range) == 0);
    }
    
    SECTION("Range boundary conditions") {
        auto series = OwningBackend::create();
        
        // Exact match on boundaries
        auto range = series->viewInRange(TimeFrameIndex{20}, TimeFrameIndex{40}, *time_frame);
        std::vector<TimeFrameIndex> events;
        for (auto e : range) {
            events.push_back(e.value());
        }
        
        CHECK(events.size() == 3);
        CHECK(events[0] == TimeFrameIndex{20});
        CHECK(events[1] == TimeFrameIndex{30});
        CHECK(events[2] == TimeFrameIndex{40});
    }
}

// =============================================================================
// createView Factory Method Tests
// =============================================================================

TEST_CASE("DigitalEventSeries::createView by time range", "[DigitalEventSeries][interface][factory]") {
    auto source = OwningBackend::create();
    
    SECTION("Creates valid view") {
        auto view_series = DigitalEventSeries::createView(source, TimeFrameIndex{15}, TimeFrameIndex{35});
        
        CHECK(view_series->isView());
        CHECK(view_series->size() == 2);
    }
    
    SECTION("View reflects source data") {
        auto view_series = DigitalEventSeries::createView(source, TimeFrameIndex{0}, TimeFrameIndex{100});
        
        std::vector<TimeFrameIndex> view_times;
        for (auto e : view_series->view()) {
            view_times.push_back(e.time());
        }
        
        std::vector<TimeFrameIndex> source_times;
        for (auto e : source->view()) {
            source_times.push_back(e.time());
        }
        
        CHECK(view_times == source_times);
    }
    
    SECTION("Empty range creates empty view") {
        auto view_series = DigitalEventSeries::createView(source, TimeFrameIndex{100}, TimeFrameIndex{200});
        CHECK(view_series->size() == 0);
    }
}

TEST_CASE("DigitalEventSeries::createView by EntityIds", "[DigitalEventSeries][interface][factory][entity]") {
    // Need to use DataManager for proper entity IDs
    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40, 50, 60});
    data_manager->setTime(TimeKey("test"), time_frame);
    
    data_manager->setData<DigitalEventSeries>("events", TimeKey("test"));
    auto source = data_manager->getData<DigitalEventSeries>("events");
    
    source->addEvent(TimeFrameIndex{10});
    source->addEvent(TimeFrameIndex{20});
    source->addEvent(TimeFrameIndex{30});
    source->addEvent(TimeFrameIndex{40});
    source->addEvent(TimeFrameIndex{50});
    
    REQUIRE(source->size() == 5);
    
    SECTION("Filter by subset of EntityIds") {
        std::unordered_set<EntityId> filter{source->view()[1].id(), source->view()[3].id()};
        auto view_series = DigitalEventSeries::createView(source, filter);
        
        CHECK(view_series->isView());
        CHECK(view_series->size() == 2);
        
        std::vector<TimeFrameIndex> times;
        for (auto e : view_series->view()) {
            times.push_back(e.time());
        }
        
        CHECK(times[0] == TimeFrameIndex{20});
        CHECK(times[1] == TimeFrameIndex{40});
    }
}

// =============================================================================
// Legacy Interface Compatibility Tests
// =============================================================================

TEST_CASE("DigitalEventSeries legacy interface", "[DigitalEventSeries][interface][legacy]") {
    SECTION("getEventSeries returns sorted vector - Owning") {
        auto series = OwningBackend::create();
        auto const& vec = series->view();
        
        REQUIRE(series->size() == 5);
        CHECK(vec[0].time() == TimeFrameIndex{10});
        CHECK(vec[4].time() == TimeFrameIndex{50});
    }
    
    SECTION("getEventSeries returns sorted vector - View") {
        auto series = ViewBackend::create();
        auto const& vec = series->view();
        
        REQUIRE(series->size() == 5);
        CHECK(vec[0].time() == TimeFrameIndex{10});
    }
    
    SECTION("getEventSeries returns sorted vector - Lazy") {
        auto series = LazyBackend::create();
        auto const& vec = series->view();
        
        REQUIRE(series->size() == 5);
        REQUIRE(vec[0].time() == TimeFrameIndex{10});
    }
    
}
