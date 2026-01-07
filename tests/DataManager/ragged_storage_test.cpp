/**
 * @file ragged_storage_test.cpp
 * @brief Unit tests for RaggedStorage CRTP + Variant implementation
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/RaggedStorage.hpp"
#include "utils/RaggedTimeSeries.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <unordered_set>

TEST_CASE("OwningRaggedStorage basic operations", "[RaggedStorage]") {
    OwningRaggedStorage<Point2D<float>> storage;
    
    SECTION("Empty storage") {
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
        CHECK(storage.getTimeCount() == 0);
        CHECK(storage.getStorageType() == RaggedStorageType::Owning);
        CHECK_FALSE(storage.isView());
    }
    
    SECTION("Append single entry") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        
        CHECK(storage.size() == 1);
        CHECK_FALSE(storage.empty());
        CHECK(storage.getTimeCount() == 1);
        
        CHECK(storage.getTime(0) == TimeFrameIndex{10});
        CHECK(storage.getData(0).x == 1.0f);
        CHECK(storage.getData(0).y == 2.0f);
        CHECK(storage.getEntityId(0) == EntityId{100});
    }
    
    SECTION("Append multiple entries at same time") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{10}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        storage.append(TimeFrameIndex{10}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        CHECK(storage.size() == 3);
        CHECK(storage.getTimeCount() == 1);
        
        auto [start, end] = storage.getTimeRange(TimeFrameIndex{10});
        CHECK(start == 0);
        CHECK(end == 3);
    }
    
    SECTION("Append entries at different times") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        storage.append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        CHECK(storage.size() == 3);
        CHECK(storage.getTimeCount() == 3);
        
        auto [start1, end1] = storage.getTimeRange(TimeFrameIndex{10});
        CHECK(start1 == 0);
        CHECK(end1 == 1);
        
        auto [start2, end2] = storage.getTimeRange(TimeFrameIndex{20});
        CHECK(start2 == 1);
        CHECK(end2 == 2);
    }
    
    SECTION("EntityId lookup") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        storage.append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        auto idx = storage.findByEntityId(EntityId{101});
        REQUIRE(idx.has_value());
        CHECK(*idx == 1);
        CHECK(storage.getData(*idx).x == 3.0f);
        
        CHECK_FALSE(storage.findByEntityId(EntityId{999}).has_value());
    }
    
    SECTION("Remove by EntityId") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        storage.append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        CHECK(storage.removeByEntityId(EntityId{101}));
        CHECK(storage.size() == 2);
        CHECK_FALSE(storage.findByEntityId(EntityId{101}).has_value());
        
        // Remaining entries should still be findable
        auto idx0 = storage.findByEntityId(EntityId{100});
        auto idx2 = storage.findByEntityId(EntityId{102});
        REQUIRE(idx0.has_value());
        REQUIRE(idx2.has_value());
        CHECK(storage.getData(*idx0).x == 1.0f);
        CHECK(storage.getData(*idx2).x == 5.0f);
    }
    
    SECTION("Clear") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        storage.clear();
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
        CHECK(storage.getTimeCount() == 0);
    }
}

TEST_CASE("ViewRaggedStorage basic operations", "[RaggedStorage]") {
    auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
    
    // Populate source
    source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
    source->append(TimeFrameIndex{10}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
    source->append(TimeFrameIndex{20}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
    source->append(TimeFrameIndex{30}, Point2D<float>{7.0f, 8.0f}, EntityId{103});
    source->append(TimeFrameIndex{30}, Point2D<float>{9.0f, 10.0f}, EntityId{104});
    
    ViewRaggedStorage<Point2D<float>> view(source);
    
    SECTION("Empty view") {
        CHECK(view.size() == 0);
        CHECK(view.empty());
        CHECK(view.isView());
        CHECK(view.getStorageType() == RaggedStorageType::View);
    }
    
    SECTION("View all entries") {
        view.setAllIndices();
        
        CHECK(view.size() == 5);
        CHECK(view.getTimeCount() == 3);
        
        // Check data is accessible
        CHECK(view.getData(0).x == 1.0f);
        CHECK(view.getData(4).x == 9.0f);
        CHECK(view.getEntityId(2) == EntityId{102});
    }
    
    SECTION("Filter by EntityIds") {
        std::unordered_set<EntityId> entity_set{EntityId{100}, EntityId{103}};
        view.filterByEntityIds(entity_set);
        
        CHECK(view.size() == 2);
        
        // Indices are sorted, so order depends on source order
        auto idx0 = view.findByEntityId(EntityId{100});
        auto idx3 = view.findByEntityId(EntityId{103});
        REQUIRE(idx0.has_value());
        REQUIRE(idx3.has_value());
        CHECK(view.getData(*idx0).x == 1.0f);
        CHECK(view.getData(*idx3).x == 7.0f);
    }
    
    SECTION("Filter by time range") {
        view.filterByTimeRange(TimeFrameIndex{10}, TimeFrameIndex{20});
        
        CHECK(view.size() == 3); // 2 at time 10, 1 at time 20
        CHECK(view.getTimeCount() == 2);
        
        // Should not include time 30 entries
        CHECK_FALSE(view.findByEntityId(EntityId{103}).has_value());
        CHECK_FALSE(view.findByEntityId(EntityId{104}).has_value());
    }
    
    SECTION("View references source data") {
        view.setAllIndices();
        
        // Data should reference same memory
        CHECK(&view.getData(0) == &source->getData(0));
    }
}

TEST_CASE("RaggedStorageVariant operations", "[RaggedStorage]") {
    SECTION("Default construct is owning") {
        RaggedStorageVariant<Point2D<float>> variant;
        
        CHECK(variant.isOwning());
        CHECK_FALSE(variant.isView());
        CHECK(variant.empty());
    }
    
    SECTION("Owning variant operations") {
        OwningRaggedStorage<Point2D<float>> storage;
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        RaggedStorageVariant<Point2D<float>> variant(std::move(storage));
        
        CHECK(variant.size() == 2);
        CHECK(variant.getTime(0) == TimeFrameIndex{10});
        CHECK(variant.getData(0).x == 1.0f);
        CHECK(variant.getEntityId(1) == EntityId{101});
        
        auto idx = variant.findByEntityId(EntityId{100});
        REQUIRE(idx.has_value());
        CHECK(*idx == 0);
    }
    
    SECTION("View variant operations") {
        auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
        source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        source->append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        source->append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        ViewRaggedStorage<Point2D<float>> view(source);
        view.filterByTimeRange(TimeFrameIndex{10}, TimeFrameIndex{20});
        
        RaggedStorageVariant<Point2D<float>> variant(std::move(view));
        
        CHECK(variant.size() == 2);
        CHECK(variant.isView());
        CHECK_FALSE(variant.isOwning());
        
        // Access through unified interface
        CHECK(variant.getData(0).x == 1.0f);
        CHECK(variant.getData(1).x == 3.0f);
    }
    
    SECTION("Visit pattern") {
        OwningRaggedStorage<Point2D<float>> storage;
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        RaggedStorageVariant<Point2D<float>> variant(std::move(storage));
        
        // Sum all x values using visit
        float sum = variant.visit([](auto const& s) {
            float total = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                total += s.getData(i).x;
            }
            return total;
        });
        
        CHECK(sum == 4.0f);
    }
}

TEST_CASE("RaggedStorage with Mask2D", "[RaggedStorage]") {
    OwningRaggedStorage<Mask2D> storage;
    
    SECTION("Append and retrieve Mask2D") {
        Mask2D mask;
        mask.push_back({10, 20});
        mask.push_back({30, 40});
        
        storage.append(TimeFrameIndex{100}, std::move(mask), EntityId{1});
        
        CHECK(storage.size() == 1);
        CHECK(storage.getData(0).size() == 2);
        CHECK(storage.getData(0).points()[0].x == 10);
    }
    
    SECTION("View of Mask2D data") {
        for (int i = 0; i < 5; ++i) {
            Mask2D mask;
            mask.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(i * 2)});
            storage.append(TimeFrameIndex{i * 10}, std::move(mask), EntityId{static_cast<uint64_t>(i)});
        }
        
        auto source = std::make_shared<OwningRaggedStorage<Mask2D>>(std::move(storage));
        ViewRaggedStorage<Mask2D> view(source);
        
        std::unordered_set<EntityId> ids{EntityId{1}, EntityId{3}};
        view.filterByEntityIds(ids);
        
        CHECK(view.size() == 2);
        
        // Verify data integrity
        auto idx1 = view.findByEntityId(EntityId{1});
        auto idx3 = view.findByEntityId(EntityId{3});
        REQUIRE(idx1.has_value());
        REQUIRE(idx3.has_value());
        CHECK(view.getData(*idx1).points()[0].x == 1);
        CHECK(view.getData(*idx3).points()[0].x == 3);
    }
}

// =============================================================================
// Cache Optimization Tests
// =============================================================================

TEST_CASE("RaggedStorageCache - Owning storage", "[RaggedStorage][cache]") {
    OwningRaggedStorage<Point2D<float>> storage;
    
    SECTION("Empty storage has valid but zero-size cache") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 0);
    }
    
    SECTION("Populated storage provides valid cache with pointers") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        storage.append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        auto cache = storage.tryGetCache();
        
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 3);
        
        // Verify cache data matches storage data
        for (size_t i = 0; i < cache.cache_size; ++i) {
            CHECK(cache.getTime(i) == storage.getTime(i));
            CHECK(cache.getData(i).x == storage.getData(i).x);
            CHECK(cache.getData(i).y == storage.getData(i).y);
            CHECK(cache.getEntityId(i) == storage.getEntityId(i));
        }
    }
    
    SECTION("Cache pointers are contiguous with storage") {
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        auto cache = storage.tryGetCache();
        
        // Pointers should match internal vector data
        CHECK(cache.times_ptr == storage.times().data());
        CHECK(cache.data_ptr == storage.data().data());
        CHECK(cache.entity_ids_ptr == storage.entityIds().data());
    }
}

TEST_CASE("RaggedStorageCache - View storage", "[RaggedStorage][cache]") {
    auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
    source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
    source->append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
    source->append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
    
    ViewRaggedStorage<Point2D<float>> view(source);
    view.setAllIndices();
    
    SECTION("Contiguous view returns valid cache") {
        // View with setAllIndices() creates contiguous indices [0,1,2]
        // so it can provide a valid cache pointing to source data
        auto cache = view.tryGetCache();
        
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 3);
        CHECK(cache.times_ptr != nullptr);
        CHECK(cache.data_ptr != nullptr);
        CHECK(cache.entity_ids_ptr != nullptr);
        
        // Verify cache data matches view data
        CHECK(cache.getTime(0) == view.getTime(0));
        CHECK(cache.getData(1).x == view.getData(1).x);
        CHECK(cache.getEntityId(2) == view.getEntityId(2));
    }
    
    SECTION("Non-contiguous filtered view returns invalid cache") {
        // Filtering to non-adjacent indices [0, 2] breaks contiguity
        std::unordered_set<EntityId> ids{EntityId{100}, EntityId{102}};
        view.filterByEntityIds(ids);
        
        auto cache = view.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
    
    SECTION("Contiguous subset view returns valid cache") {
        // A view containing only indices [0, 1] is still contiguous
        std::unordered_set<EntityId> ids{EntityId{100}, EntityId{101}};
        view.filterByEntityIds(ids);
        
        auto cache = view.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 2);
        
        // Verify the cache points to the correct data
        CHECK(cache.getTime(0) == TimeFrameIndex{10});
        CHECK(cache.getTime(1) == TimeFrameIndex{20});
    }
}

// =============================================================================
// RaggedStorageWrapper Tests (Type Erasure)
// =============================================================================

TEST_CASE("RaggedStorageWrapper - Basic operations with owning storage", "[RaggedStorage][wrapper]") {
    OwningRaggedStorage<Point2D<float>> storage;
    storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
    storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
    storage.append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
    
    RaggedStorageWrapper<Point2D<float>> wrapper(std::move(storage));
    
    SECTION("Size and bounds") {
        CHECK(wrapper.size() == 3);
        CHECK_FALSE(wrapper.empty());
        CHECK(wrapper.getTimeCount() == 3);
    }
    
    SECTION("Element access") {
        CHECK(wrapper.getTime(0) == TimeFrameIndex{10});
        CHECK(wrapper.getData(0).x == 1.0f);
        CHECK(wrapper.getData(0).y == 2.0f);
        CHECK(wrapper.getEntityId(1) == EntityId{101});
    }
    
    SECTION("EntityId lookup") {
        auto idx = wrapper.findByEntityId(EntityId{101});
        REQUIRE(idx.has_value());
        CHECK(*idx == 1);
        
        CHECK_FALSE(wrapper.findByEntityId(EntityId{999}).has_value());
    }
    
    SECTION("Time range lookup") {
        auto [start, end] = wrapper.getTimeRange(TimeFrameIndex{10});
        CHECK(start == 0);
        CHECK(end == 1);
    }
    
    SECTION("Storage type identification") {
        CHECK(wrapper.getStorageType() == RaggedStorageType::Owning);
        CHECK_FALSE(wrapper.isView());
    }
}

TEST_CASE("RaggedStorageWrapper - Cache optimization", "[RaggedStorage][wrapper][cache]") {
    SECTION("Owning storage provides valid cache through wrapper") {
        OwningRaggedStorage<Point2D<float>> storage;
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        storage.append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        RaggedStorageWrapper<Point2D<float>> wrapper(std::move(storage));
        
        auto cache = wrapper.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 2);
        
        // Verify cache data matches wrapper data
        CHECK(cache.getTime(0) == wrapper.getTime(0));
        CHECK(cache.getData(0).x == wrapper.getData(0).x);
        CHECK(cache.getEntityId(1) == wrapper.getEntityId(1));
    }
    
    SECTION("Contiguous view provides valid cache through wrapper") {
        auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
        source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        source->append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        ViewRaggedStorage<Point2D<float>> view(source);
        view.setAllIndices();  // Creates contiguous indices [0, 1]
        
        RaggedStorageWrapper<Point2D<float>> wrapper(std::move(view));
        
        auto cache = wrapper.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 2);
        CHECK(wrapper.getStorageType() == RaggedStorageType::View);
    }
    
    SECTION("Non-contiguous view provides invalid cache through wrapper") {
        auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
        source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        source->append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        source->append(TimeFrameIndex{30}, Point2D<float>{5.0f, 6.0f}, EntityId{102});
        
        ViewRaggedStorage<Point2D<float>> view(source);
        view.setAllIndices();
        // Filter to non-contiguous indices [0, 2]
        std::unordered_set<EntityId> ids{EntityId{100}, EntityId{102}};
        view.filterByEntityIds(ids);
        
        RaggedStorageWrapper<Point2D<float>> wrapper(std::move(view));
        
        auto cache = wrapper.tryGetCache();
        CHECK_FALSE(cache.isValid());
        CHECK(wrapper.getStorageType() == RaggedStorageType::View);
    }
}

TEST_CASE("RaggedStorageWrapper - Default construction", "[RaggedStorage][wrapper]") {
    RaggedStorageWrapper<Point2D<float>> wrapper;
    
    CHECK(wrapper.size() == 0);
    CHECK(wrapper.empty());
    CHECK(wrapper.getStorageType() == RaggedStorageType::Owning);
    
    // Even empty owning storage should have valid cache
    auto cache = wrapper.tryGetCache();
    CHECK(cache.isValid());
    CHECK(cache.cache_size == 0);
}

TEST_CASE("RaggedStorageWrapper - Type access", "[RaggedStorage][wrapper]") {
    SECTION("tryGet returns correct type for owning storage") {
        OwningRaggedStorage<Point2D<float>> storage;
        storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        
        RaggedStorageWrapper<Point2D<float>> wrapper(std::move(storage));
        
        auto* owning = wrapper.tryGet<OwningRaggedStorage<Point2D<float>>>();
        REQUIRE(owning != nullptr);
        CHECK(owning->size() == 1);
        
        auto* view = wrapper.tryGet<ViewRaggedStorage<Point2D<float>>>();
        CHECK(view == nullptr);
    }
    
    SECTION("tryGet returns correct type for view storage") {
        auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
        source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        
        ViewRaggedStorage<Point2D<float>> view(source);
        view.setAllIndices();
        
        RaggedStorageWrapper<Point2D<float>> wrapper(std::move(view));
        
        auto* viewPtr = wrapper.tryGet<ViewRaggedStorage<Point2D<float>>>();
        REQUIRE(viewPtr != nullptr);
        CHECK(viewPtr->size() == 1);
        
        auto* owning = wrapper.tryGet<OwningRaggedStorage<Point2D<float>>>();
        CHECK(owning == nullptr);
    }
}

TEST_CASE("RaggedStorageWrapper - Move semantics", "[RaggedStorage][wrapper]") {
    OwningRaggedStorage<Point2D<float>> storage;
    storage.append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
    
    RaggedStorageWrapper<Point2D<float>> wrapper1(std::move(storage));
    CHECK(wrapper1.size() == 1);
    
    // Move construction
    RaggedStorageWrapper<Point2D<float>> wrapper2(std::move(wrapper1));
    CHECK(wrapper2.size() == 1);
    CHECK(wrapper2.getData(0).x == 1.0f);
    
    // Move assignment
    RaggedStorageWrapper<Point2D<float>> wrapper3;
    wrapper3 = std::move(wrapper2);
    CHECK(wrapper3.size() == 1);
    CHECK(wrapper3.getEntityId(0) == EntityId{100});
}

// =============================================================================
// LazyRaggedStorage Tests
// =============================================================================

TEST_CASE("LazyRaggedStorage basic operations", "[RaggedStorage][lazy]") {
    // Create source data as a vector of tuples
    std::vector<std::tuple<TimeFrameIndex, EntityId, Point2D<float>>> source_data{
        {TimeFrameIndex{10}, EntityId{100}, Point2D<float>{1.0f, 2.0f}},
        {TimeFrameIndex{10}, EntityId{101}, Point2D<float>{3.0f, 4.0f}},
        {TimeFrameIndex{20}, EntityId{102}, Point2D<float>{5.0f, 6.0f}},
        {TimeFrameIndex{30}, EntityId{103}, Point2D<float>{7.0f, 8.0f}},
        {TimeFrameIndex{30}, EntityId{104}, Point2D<float>{9.0f, 10.0f}}
    };
    
    // Create a view that applies a transform
    auto transform_view = source_data
        | std::views::transform([](auto const& tuple) {
            auto [time, eid, pt] = tuple;
            return std::make_tuple(time, eid, Point2D<float>{pt.x * 2.0f, pt.y * 2.0f});
        });
    
    using ViewType = decltype(transform_view);
    LazyRaggedStorage<Point2D<float>, ViewType> lazy_storage(transform_view, source_data.size());
    
    SECTION("Size and storage type") {
        CHECK(lazy_storage.size() == 5);
        CHECK_FALSE(lazy_storage.empty());
        CHECK(lazy_storage.getStorageType() == RaggedStorageType::Lazy);
    }
    
    SECTION("Element access computes transform on-demand") {
        // Values should be doubled by the transform
        CHECK(lazy_storage.getTime(0) == TimeFrameIndex{10});
        CHECK(lazy_storage.getData(0).x == 2.0f);  // 1.0 * 2
        CHECK(lazy_storage.getData(0).y == 4.0f);  // 2.0 * 2
        CHECK(lazy_storage.getEntityId(0) == EntityId{100});
        
        CHECK(lazy_storage.getData(2).x == 10.0f); // 5.0 * 2
        CHECK(lazy_storage.getData(2).y == 12.0f); // 6.0 * 2
        CHECK(lazy_storage.getEntityId(2) == EntityId{102});
    }
    
    SECTION("EntityId lookup") {
        auto idx = lazy_storage.findByEntityId(EntityId{102});
        REQUIRE(idx.has_value());
        CHECK(*idx == 2);
        CHECK(lazy_storage.getData(*idx).x == 10.0f);
        
        CHECK_FALSE(lazy_storage.findByEntityId(EntityId{999}).has_value());
    }
    
    SECTION("Time range lookup") {
        CHECK(lazy_storage.getTimeCount() == 3);
        
        auto [start1, end1] = lazy_storage.getTimeRange(TimeFrameIndex{10});
        CHECK(start1 == 0);
        CHECK(end1 == 2);
        
        auto [start2, end2] = lazy_storage.getTimeRange(TimeFrameIndex{20});
        CHECK(start2 == 2);
        CHECK(end2 == 3);
        
        auto [start3, end3] = lazy_storage.getTimeRange(TimeFrameIndex{30});
        CHECK(start3 == 3);
        CHECK(end3 == 5);
    }
    
    SECTION("Cache is always invalid for lazy storage") {
        auto cache = lazy_storage.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
}

TEST_CASE("LazyRaggedStorage through wrapper", "[RaggedStorage][lazy][wrapper]") {
    // Create source data
    std::vector<std::tuple<TimeFrameIndex, EntityId, Point2D<float>>> source_data{
        {TimeFrameIndex{10}, EntityId{100}, Point2D<float>{1.0f, 2.0f}},
        {TimeFrameIndex{20}, EntityId{101}, Point2D<float>{3.0f, 4.0f}},
        {TimeFrameIndex{30}, EntityId{102}, Point2D<float>{5.0f, 6.0f}}
    };
    
    // Create identity transform view (just to test the wrapper integration)
    auto identity_view = source_data | std::views::all;
    
    using ViewType = decltype(identity_view);
    LazyRaggedStorage<Point2D<float>, ViewType> lazy_storage(identity_view, source_data.size());
    
    RaggedStorageWrapper<Point2D<float>> wrapper(std::move(lazy_storage));
    
    SECTION("Wrapper reports correct storage type") {
        CHECK(wrapper.getStorageType() == RaggedStorageType::Lazy);
    }
    
    SECTION("Wrapper provides access through unified interface") {
        CHECK(wrapper.size() == 3);
        CHECK(wrapper.getTime(0) == TimeFrameIndex{10});
        CHECK(wrapper.getData(0).x == 1.0f);
        CHECK(wrapper.getEntityId(1) == EntityId{101});
        
        auto idx = wrapper.findByEntityId(EntityId{102});
        REQUIRE(idx.has_value());
        CHECK(*idx == 2);
    }
    
    SECTION("Cache through wrapper is invalid") {
        auto cache = wrapper.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
    
    SECTION("Mutation operations throw for lazy storage") {
        CHECK_THROWS(wrapper.append(TimeFrameIndex{40}, Point2D<float>{0.0f, 0.0f}, EntityId{999}));
        CHECK_THROWS(wrapper.clear());
        CHECK_THROWS(wrapper.removeByEntityId(EntityId{100}));
        CHECK_THROWS(wrapper.removeAtTime(TimeFrameIndex{10}));
        CHECK_THROWS(wrapper.getMutableData(0));
    }
}

TEST_CASE("LazyRaggedStorage with complex transform", "[RaggedStorage][lazy]") {
    // Create mask data
    std::vector<std::tuple<TimeFrameIndex, EntityId, Mask2D>> source_masks;
    
    for (int i = 0; i < 3; ++i) {
        Mask2D mask;
        mask.push_back({static_cast<uint32_t>(i * 10), static_cast<uint32_t>(i * 20)});
        mask.push_back({static_cast<uint32_t>(i * 10 + 1), static_cast<uint32_t>(i * 20 + 1)});
        source_masks.emplace_back(TimeFrameIndex{i * 100}, EntityId{static_cast<uint64_t>(i)}, std::move(mask));
    }
    
    // Transform that doubles all mask coordinates
    auto transform_view = source_masks
        | std::views::transform([](auto const& tuple) {
            auto [time, eid, mask] = tuple;
            Mask2D new_mask;
            for (auto const& pt : mask.points()) {
                new_mask.push_back({pt.x * 2, pt.y * 2});
            }
            return std::make_tuple(time, eid, new_mask);
        });
    
    using ViewType = decltype(transform_view);
    LazyRaggedStorage<Mask2D, ViewType> lazy_storage(transform_view, source_masks.size());
    
    SECTION("Complex data transform works correctly") {
        CHECK(lazy_storage.size() == 3);
        
        // First mask's first point: (0, 0) -> (0, 0)
        CHECK(lazy_storage.getData(0).points()[0].x == 0);
        CHECK(lazy_storage.getData(0).points()[0].y == 0);
        
        // Second mask's first point: (10, 20) -> (20, 40)
        CHECK(lazy_storage.getData(1).points()[0].x == 20);
        CHECK(lazy_storage.getData(1).points()[0].y == 40);
        
        // Third mask's second point: (21, 41) -> (42, 82)
        CHECK(lazy_storage.getData(2).points()[1].x == 42);
        CHECK(lazy_storage.getData(2).points()[1].y == 82);
    }
}
// =============================================================================
// RaggedTimeSeries createFromView and materialize Tests
// =============================================================================

TEST_CASE("RaggedTimeSeries::createFromView basic usage", "[RaggedTimeSeries][lazy]") {
    // Create source data as tuples
    std::vector<std::tuple<TimeFrameIndex, EntityId, Point2D<float>>> source_data{
        {TimeFrameIndex{10}, EntityId{100}, Point2D<float>{1.0f, 2.0f}},
        {TimeFrameIndex{10}, EntityId{101}, Point2D<float>{3.0f, 4.0f}},
        {TimeFrameIndex{20}, EntityId{102}, Point2D<float>{5.0f, 6.0f}},
        {TimeFrameIndex{30}, EntityId{103}, Point2D<float>{7.0f, 8.0f}}
    };
    
    // Create a transform view that doubles coordinates
    auto transform_view = source_data
        | std::views::transform([](auto const& tuple) {
            auto [time, eid, pt] = tuple;
            return std::make_tuple(time, eid, Point2D<float>{pt.x * 2.0f, pt.y * 2.0f});
        });
    
    std::vector<int> times = {0, 10, 20, 30};
    auto time_frame = std::make_shared<TimeFrame>(times);
    
    SECTION("createFromView creates lazy series") {
        auto lazy_series = RaggedTimeSeries<Point2D<float>>::createFromView(
            transform_view, time_frame, ImageSize{100, 100});
        
        REQUIRE(lazy_series != nullptr);
        CHECK(lazy_series->getTotalEntryCount() == 4);
        CHECK(lazy_series->getStorageType() == RaggedStorageType::Lazy);
        CHECK(lazy_series->isLazy());
        CHECK(lazy_series->getImageSize().width == 100);
        CHECK(lazy_series->getTimeFrame() == time_frame);
    }
    
    SECTION("Lazy series computes values on-demand") {
        auto lazy_series = RaggedTimeSeries<Point2D<float>>::createFromView(
            transform_view, time_frame);
        
        // Access should compute the transform
        auto data_at_10 = lazy_series->getAtTime(TimeFrameIndex{10});
        std::vector<Point2D<float>> points;
        for (auto const& pt : data_at_10) {
            points.push_back(pt);
        }
        
        REQUIRE(points.size() == 2);
        CHECK(points[0].x == 2.0f);  // 1.0 * 2
        CHECK(points[0].y == 4.0f);  // 2.0 * 2
        CHECK(points[1].x == 6.0f);  // 3.0 * 2
        CHECK(points[1].y == 8.0f);  // 4.0 * 2
    }
    
    SECTION("EntityId lookup works on lazy series") {
        auto lazy_series = RaggedTimeSeries<Point2D<float>>::createFromView(
            transform_view, time_frame);
        
        auto data = lazy_series->getDataByEntityId(EntityId{102});
        REQUIRE(data.has_value());
        CHECK(data->get().x == 10.0f);  // 5.0 * 2
        CHECK(data->get().y == 12.0f);  // 6.0 * 2
        
        // Non-existent EntityId
        CHECK_FALSE(lazy_series->getDataByEntityId(EntityId{999}).has_value());
    }
}

TEST_CASE("RaggedTimeSeries::materialize converts to owning", "[RaggedTimeSeries][lazy]") {
    // Create source data
    std::vector<std::tuple<TimeFrameIndex, EntityId, Point2D<float>>> source_data{
        {TimeFrameIndex{10}, EntityId{100}, Point2D<float>{1.0f, 2.0f}},
        {TimeFrameIndex{20}, EntityId{101}, Point2D<float>{3.0f, 4.0f}},
        {TimeFrameIndex{30}, EntityId{102}, Point2D<float>{5.0f, 6.0f}}
    };
    
    auto identity_view = source_data | std::views::all;
    
    std::vector<int> times = {0, 10, 20, 30};
    auto time_frame = std::make_shared<TimeFrame>(times);
    ImageSize img_size{640, 480};
    
    auto lazy_series = RaggedTimeSeries<Point2D<float>>::createFromView(
        identity_view, time_frame, img_size);
    
    SECTION("materialize creates owning storage") {
        auto materialized = lazy_series->materialize();
        
        REQUIRE(materialized != nullptr);
        CHECK(materialized->getStorageType() == RaggedStorageType::Owning);
        CHECK_FALSE(materialized->isLazy());
        CHECK(materialized->isStorageContiguous());
    }
    
    SECTION("materialized series has same data") {
        auto materialized = lazy_series->materialize();
        
        CHECK(materialized->getTotalEntryCount() == lazy_series->getTotalEntryCount());
        CHECK(materialized->getTimeCount() == lazy_series->getTimeCount());
        
        // Check each element matches
        for (size_t i = 0; i < 3; ++i) {
            auto lazy_data = lazy_series->getDataByEntityId(EntityId{100 + i});
            auto mat_data = materialized->getDataByEntityId(EntityId{100 + i});
            
            REQUIRE(lazy_data.has_value());
            REQUIRE(mat_data.has_value());
            CHECK(lazy_data->get().x == mat_data->get().x);
            CHECK(lazy_data->get().y == mat_data->get().y);
        }
    }
    
    SECTION("metadata is preserved after materialization") {
        auto materialized = lazy_series->materialize();
        
        CHECK(materialized->getTimeFrame() == time_frame);
        CHECK(materialized->getImageSize().width == 640);
        CHECK(materialized->getImageSize().height == 480);
    }
    
    SECTION("EntityIds are preserved after materialization") {
        auto materialized = lazy_series->materialize();
        
        CHECK(materialized->getDataByEntityId(EntityId{100}).has_value());
        CHECK(materialized->getDataByEntityId(EntityId{101}).has_value());
        CHECK(materialized->getDataByEntityId(EntityId{102}).has_value());
        CHECK_FALSE(materialized->getDataByEntityId(EntityId{999}).has_value());
    }
    
    SECTION("materialized series supports mutation") {
        auto materialized = lazy_series->materialize();
        
        // Should not throw
        materialized->addAtTime(TimeFrameIndex{40}, Point2D<float>{9.0f, 10.0f}, NotifyObservers::No);
        CHECK(materialized->getTotalEntryCount() == 4);
        
        CHECK(materialized->clearByEntityId(EntityId{100}, NotifyObservers::No));
        CHECK(materialized->getTotalEntryCount() == 3);
    }
}

TEST_CASE("RaggedTimeSeries transform round-trip", "[RaggedTimeSeries][lazy]") {
    // Create initial owning data
    RaggedTimeSeries<Point2D<float>> original;
    std::vector<int> times = {0, 10, 20, 30, 40};
    auto time_frame = std::make_shared<TimeFrame>(times);
    original.setTimeFrame(time_frame);
    original.setImageSize(ImageSize{100, 100});
    
    // Add some data
    original.addAtTime(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, NotifyObservers::No);
    original.addAtTime(TimeFrameIndex{10}, Point2D<float>{3.0f, 4.0f}, NotifyObservers::No);
    original.addAtTime(TimeFrameIndex{20}, Point2D<float>{5.0f, 6.0f}, NotifyObservers::No);
    original.addAtTime(TimeFrameIndex{30}, Point2D<float>{7.0f, 8.0f}, NotifyObservers::No);
    
    SECTION("Full round-trip: owning -> lazy transform -> materialize") {
        // Create a vector with transformed data to use as source
        std::vector<std::tuple<TimeFrameIndex, EntityId, Point2D<float>>> transformed_source;
        for (size_t i = 0; i < original.getTotalEntryCount(); ++i) {
            auto data = original.getDataByEntityId(original.getEntityIdsAtTime(original.getTimesWithData().front()).begin()[i]);
            // Use elements() which returns (time, DataEntry) pairs
        }
        
        // Instead, directly iterate using the storage
        auto view_elements = original.elements();
        for (auto const& [time, entry] : view_elements) {
            auto const& pt = entry.data;
            transformed_source.emplace_back(time, entry.entity_id, Point2D<float>{pt.x * 3.0f, pt.y * 3.0f});
        }
        
        // Create lazy view over the transformed source
        auto scale_view = transformed_source | std::views::all;
        
        auto lazy_scaled = RaggedTimeSeries<Point2D<float>>::createFromView(
            scale_view, time_frame, original.getImageSize());
        
        CHECK(lazy_scaled->isLazy());
        
        // Materialize
        auto final_result = lazy_scaled->materialize();
        
        CHECK_FALSE(final_result->isLazy());
        CHECK(final_result->getTotalEntryCount() == 4);
        
        // Verify transformation was applied correctly
        auto data_at_10 = final_result->getAtTime(TimeFrameIndex{10});
        std::vector<Point2D<float>> points;
        for (auto const& pt : data_at_10) {
            points.push_back(pt);
        }
        
        REQUIRE(points.size() == 2);
        CHECK(points[0].x == 3.0f);   // 1.0 * 3
        CHECK(points[0].y == 6.0f);   // 2.0 * 3
        CHECK(points[1].x == 9.0f);   // 3.0 * 3
        CHECK(points[1].y == 12.0f);  // 4.0 * 3
    }
    
    SECTION("Cache optimization works after materialization") {
        // Create a vector with transformed data using elements()
        std::vector<std::tuple<TimeFrameIndex, EntityId, Point2D<float>>> transformed_source;
        for (auto const& [time, entry] : original.elements()) {
            auto const& pt = entry.data;
            transformed_source.emplace_back(time, entry.entity_id, Point2D<float>{pt.x * 2.0f, pt.y * 2.0f});
        }
        
        auto scale_view = transformed_source | std::views::all;
        
        auto lazy = RaggedTimeSeries<Point2D<float>>::createFromView(
            scale_view, time_frame);
        
        // Lazy storage has invalid cache
        auto lazy_cache = lazy->getStorageCache();
        CHECK_FALSE(lazy_cache.isValid());
        
        // Materialized storage has valid cache
        auto materialized = lazy->materialize();
        auto mat_cache = materialized->getStorageCache();
        CHECK(mat_cache.isValid());
        CHECK(mat_cache.cache_size == 4);
    }
}