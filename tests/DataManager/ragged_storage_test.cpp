/**
 * @file ragged_storage_test.cpp
 * @brief Unit tests for RaggedStorage CRTP + Variant implementation
 */

#include <catch2/catch_test_macros.hpp>

#include "utils/RaggedStorage.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

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
    
    SECTION("View storage returns invalid cache") {
        auto cache = view.tryGetCache();
        
        CHECK_FALSE(cache.isValid());
        CHECK(cache.times_ptr == nullptr);
        CHECK(cache.data_ptr == nullptr);
        CHECK(cache.entity_ids_ptr == nullptr);
    }
    
    SECTION("Filtered view also returns invalid cache") {
        std::unordered_set<EntityId> ids{EntityId{100}, EntityId{102}};
        view.filterByEntityIds(ids);
        
        auto cache = view.tryGetCache();
        CHECK_FALSE(cache.isValid());
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
    
    SECTION("View storage provides invalid cache through wrapper") {
        auto source = std::make_shared<OwningRaggedStorage<Point2D<float>>>();
        source->append(TimeFrameIndex{10}, Point2D<float>{1.0f, 2.0f}, EntityId{100});
        source->append(TimeFrameIndex{20}, Point2D<float>{3.0f, 4.0f}, EntityId{101});
        
        ViewRaggedStorage<Point2D<float>> view(source);
        view.setAllIndices();
        
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
