/**
 * @file ragged_analog_storage_test.cpp
 * @brief Unit tests for RaggedAnalogStorage implementations
 * 
 * Tests the storage abstraction layer for RaggedAnalogTimeSeries:
 * - OwningRaggedAnalogStorage: Basic owning storage with SoA layout
 * - ViewRaggedAnalogStorage: Zero-copy view/filter over owning storage
 * - LazyRaggedAnalogStorage: On-demand computation from transform views
 * - RaggedAnalogStorageWrapper: Type-erased wrapper with cache optimization
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogTimeSeries/RaggedAnalogStorage.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>

// =============================================================================
// OwningRaggedAnalogStorage Tests
// =============================================================================

TEST_CASE("OwningRaggedAnalogStorage basic operations", "[RaggedAnalogStorage]") {
    OwningRaggedAnalogStorage storage;
    
    SECTION("Empty storage") {
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
        CHECK(storage.getTimeCount() == 0);
        CHECK(storage.getStorageType() == RaggedAnalogStorageType::Owning);
        CHECK_FALSE(storage.isView());
        CHECK_FALSE(storage.isLazy());
    }
    
    SECTION("Append single value") {
        storage.append(TimeFrameIndex{10}, 1.5f);
        
        CHECK(storage.size() == 1);
        CHECK_FALSE(storage.empty());
        CHECK(storage.getTimeCount() == 1);
        
        CHECK(storage.getTime(0) == TimeFrameIndex{10});
        CHECK(storage.getValue(0) == 1.5f);
    }
    
    SECTION("Append multiple values at same time") {
        storage.append(TimeFrameIndex{10}, 1.0f);
        storage.append(TimeFrameIndex{10}, 2.0f);
        storage.append(TimeFrameIndex{10}, 3.0f);
        
        CHECK(storage.size() == 3);
        CHECK(storage.getTimeCount() == 1);
        
        auto [start, end] = storage.getTimeRange(TimeFrameIndex{10});
        CHECK(start == 0);
        CHECK(end == 3);
        
        CHECK(storage.getValue(0) == 1.0f);
        CHECK(storage.getValue(1) == 2.0f);
        CHECK(storage.getValue(2) == 3.0f);
    }
    
    SECTION("Append values at different times") {
        storage.append(TimeFrameIndex{10}, 1.0f);
        storage.append(TimeFrameIndex{20}, 2.0f);
        storage.append(TimeFrameIndex{30}, 3.0f);
        
        CHECK(storage.size() == 3);
        CHECK(storage.getTimeCount() == 3);
        
        auto [start1, end1] = storage.getTimeRange(TimeFrameIndex{10});
        CHECK(start1 == 0);
        CHECK(end1 == 1);
        
        auto [start2, end2] = storage.getTimeRange(TimeFrameIndex{20});
        CHECK(start2 == 1);
        CHECK(end2 == 2);
    }
    
    SECTION("appendBatch") {
        std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f};
        storage.appendBatch(TimeFrameIndex{10}, values);
        
        CHECK(storage.size() == 4);
        CHECK(storage.getTimeCount() == 1);
        
        auto span = storage.getValuesAtTime(TimeFrameIndex{10});
        REQUIRE(span.size() == 4);
        CHECK(span[0] == 1.0f);
        CHECK(span[3] == 4.0f);
    }
    
    SECTION("setAtTime replaces existing data") {
        storage.appendBatch(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f});
        storage.setAtTime(TimeFrameIndex{10}, std::vector<float>{5.0f, 6.0f, 7.0f});
        
        CHECK(storage.size() == 3);
        
        auto span = storage.getValuesAtTime(TimeFrameIndex{10});
        REQUIRE(span.size() == 3);
        CHECK(span[0] == 5.0f);
        CHECK(span[1] == 6.0f);
        CHECK(span[2] == 7.0f);
    }
    
    SECTION("removeAtTime") {
        storage.appendBatch(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f});
        storage.appendBatch(TimeFrameIndex{20}, std::vector<float>{3.0f});
        storage.appendBatch(TimeFrameIndex{30}, std::vector<float>{4.0f, 5.0f});
        
        CHECK(storage.size() == 5);
        
        size_t removed = storage.removeAtTime(TimeFrameIndex{20});
        CHECK(removed == 1);
        CHECK(storage.size() == 4);
        CHECK(storage.getTimeCount() == 2);
        CHECK_FALSE(storage.hasDataAtTime(TimeFrameIndex{20}));
    }
    
    SECTION("Clear") {
        storage.appendBatch(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f});
        storage.appendBatch(TimeFrameIndex{20}, std::vector<float>{3.0f});
        
        storage.clear();
        CHECK(storage.size() == 0);
        CHECK(storage.empty());
        CHECK(storage.getTimeCount() == 0);
    }
}

TEST_CASE("OwningRaggedAnalogStorage cache optimization", "[RaggedAnalogStorage][cache]") {
    OwningRaggedAnalogStorage storage;
    
    // Add some data
    for (int t = 0; t < 10; ++t) {
        for (int i = 0; i < 5; ++i) {
            storage.append(TimeFrameIndex{t * 10}, static_cast<float>(t * 10 + i));
        }
    }
    
    SECTION("Cache is valid for owning storage") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 50);
    }
    
    SECTION("Cache provides direct access") {
        auto cache = storage.tryGetCache();
        
        for (size_t i = 0; i < cache.cache_size; ++i) {
            CHECK(cache.getTime(i) == storage.getTime(i));
            CHECK(cache.getValue(i) == storage.getValue(i));
        }
    }
}

// =============================================================================
// ViewRaggedAnalogStorage Tests
// =============================================================================

TEST_CASE("ViewRaggedAnalogStorage basic operations", "[RaggedAnalogStorage][view]") {
    auto source = std::make_shared<OwningRaggedAnalogStorage>();
    
    // Populate source
    source->appendBatch(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f});
    source->appendBatch(TimeFrameIndex{20}, std::vector<float>{3.0f, 4.0f, 5.0f});
    source->appendBatch(TimeFrameIndex{30}, std::vector<float>{6.0f});
    
    ViewRaggedAnalogStorage view(source);
    
    SECTION("Empty view") {
        CHECK(view.size() == 0);
        CHECK(view.empty());
        CHECK(view.isView());
        CHECK(view.getStorageType() == RaggedAnalogStorageType::View);
    }
    
    SECTION("View all entries") {
        view.setAllIndices();
        
        CHECK(view.size() == 6);
        CHECK(view.getTimeCount() == 3);
        
        CHECK(view.getValue(0) == 1.0f);
        CHECK(view.getValue(5) == 6.0f);
    }
    
    SECTION("Filter by time range") {
        view.filterByTimeRange(TimeFrameIndex{10}, TimeFrameIndex{20});
        
        CHECK(view.size() == 5);  // 2 at t=10, 3 at t=20
        CHECK(view.getTimeCount() == 2);
        CHECK_FALSE(view.hasDataAtTime(TimeFrameIndex{30}));
    }
    
    SECTION("View cache is valid when contiguous") {
        view.setAllIndices();
        auto cache = view.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 6);
    }
}

// =============================================================================
// LazyRaggedAnalogStorage Tests
// =============================================================================

TEST_CASE("LazyRaggedAnalogStorage basic operations", "[RaggedAnalogStorage][lazy]") {
    // Create source data
    std::vector<std::pair<TimeFrameIndex, float>> source_data;
    for (int t = 0; t < 5; ++t) {
        for (int i = 0; i < 3; ++i) {
            source_data.emplace_back(TimeFrameIndex{t * 10}, static_cast<float>(t * 10 + i));
        }
    }
    
    // Create lazy view that doubles values
    auto transform_view = source_data 
        | std::views::transform([](auto const& pair) {
            return std::make_pair(pair.first, pair.second * 2.0f);
        });
    
    // Need to materialize to a vector for random access
    std::vector<std::pair<TimeFrameIndex, float>> materialized(
        std::ranges::begin(transform_view), 
        std::ranges::end(transform_view)
    );
    
    auto view = std::ranges::ref_view(materialized);
    
    LazyRaggedAnalogStorage<decltype(view)> lazy_storage(view, materialized.size());
    
    SECTION("Lazy storage basic properties") {
        CHECK(lazy_storage.size() == 15);
        CHECK(lazy_storage.getTimeCount() == 5);
        CHECK(lazy_storage.isLazy());
        CHECK(lazy_storage.getStorageType() == RaggedAnalogStorageType::Lazy);
    }
    
    SECTION("Lazy storage computes values on access") {
        // Original value at index 0 was 0.0f, doubled is 0.0f
        CHECK(lazy_storage.getValue(0) == 0.0f);
        
        // Original value at index 1 was 1.0f, doubled is 2.0f
        CHECK(lazy_storage.getValue(1) == 2.0f);
        
        // At time 40, first value was 40.0f, doubled is 80.0f
        CHECK(lazy_storage.getValue(12) == 80.0f);
    }
    
    SECTION("Lazy cache is always invalid") {
        auto cache = lazy_storage.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }
    
    SECTION("Time range lookup works") {
        auto [start, end] = lazy_storage.getTimeRange(TimeFrameIndex{20});
        CHECK(end - start == 3);
    }
}

// =============================================================================
// RaggedAnalogStorageWrapper Tests
// =============================================================================

TEST_CASE("RaggedAnalogStorageWrapper type erasure", "[RaggedAnalogStorage][wrapper]") {
    
    SECTION("Default constructor creates owning storage") {
        RaggedAnalogStorageWrapper wrapper;
        
        CHECK(wrapper.empty());
        CHECK(wrapper.getStorageType() == RaggedAnalogStorageType::Owning);
    }
    
    SECTION("Wrap owning storage") {
        OwningRaggedAnalogStorage owning;
        owning.append(TimeFrameIndex{10}, 1.0f);
        owning.append(TimeFrameIndex{10}, 2.0f);
        
        RaggedAnalogStorageWrapper wrapper(std::move(owning));
        
        CHECK(wrapper.size() == 2);
        CHECK(wrapper.getStorageType() == RaggedAnalogStorageType::Owning);
        CHECK(wrapper.getValue(0) == 1.0f);
        CHECK(wrapper.getValue(1) == 2.0f);
    }
    
    SECTION("Wrap view storage") {
        auto source = std::make_shared<OwningRaggedAnalogStorage>();
        source->appendBatch(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f, 3.0f});
        
        ViewRaggedAnalogStorage view(source);
        view.setAllIndices();
        
        RaggedAnalogStorageWrapper wrapper(std::move(view));
        
        CHECK(wrapper.size() == 3);
        CHECK(wrapper.getStorageType() == RaggedAnalogStorageType::View);
    }
    
    SECTION("Mutation through wrapper") {
        RaggedAnalogStorageWrapper wrapper;
        
        wrapper.append(TimeFrameIndex{10}, 1.0f);
        wrapper.append(TimeFrameIndex{10}, 2.0f);
        wrapper.appendBatch(TimeFrameIndex{20}, std::vector<float>{3.0f, 4.0f});
        
        CHECK(wrapper.size() == 4);
        CHECK(wrapper.getTimeCount() == 2);
    }
    
    SECTION("Cache optimization through wrapper") {
        RaggedAnalogStorageWrapper wrapper;
        wrapper.appendBatch(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f, 3.0f});
        
        auto cache = wrapper.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.cache_size == 3);
    }
}

// =============================================================================
// RaggedAnalogTimeSeries Integration Tests
// =============================================================================

TEST_CASE("RaggedAnalogTimeSeries with new storage", "[RaggedAnalogTimeSeries]") {
    RaggedAnalogTimeSeries series;
    
    SECTION("Basic operations unchanged") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
        series.setDataAtTime(TimeFrameIndex{20}, std::vector<float>{3.0f}, NotifyObservers::No);
        
        CHECK(series.getNumTimePoints() == 2);
        CHECK(series.getTotalValueCount() == 3);
        CHECK(series.hasDataAtTime(TimeFrameIndex{10}));
        CHECK(series.hasDataAtTime(TimeFrameIndex{20}));
        
        auto data = series.getDataAtTime(TimeFrameIndex{10});
        REQUIRE(data.size() == 2);
        CHECK(data[0] == 1.0f);
        CHECK(data[1] == 2.0f);
    }
    
    SECTION("appendAtTime") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f}, NotifyObservers::No);
        series.appendAtTime(TimeFrameIndex{10}, std::vector<float>{2.0f, 3.0f}, NotifyObservers::No);
        
        auto data = series.getDataAtTime(TimeFrameIndex{10});
        REQUIRE(data.size() == 3);
        CHECK(data[0] == 1.0f);
        CHECK(data[1] == 2.0f);
        CHECK(data[2] == 3.0f);
    }
    
    SECTION("clearAtTime") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
        series.setDataAtTime(TimeFrameIndex{20}, std::vector<float>{3.0f}, NotifyObservers::No);
        
        bool cleared = series.clearAtTime(TimeFrameIndex{10}, NotifyObservers::No);
        CHECK(cleared);
        CHECK_FALSE(series.hasDataAtTime(TimeFrameIndex{10}));
        CHECK(series.hasDataAtTime(TimeFrameIndex{20}));
        CHECK(series.getTotalValueCount() == 1);
    }
    
    SECTION("clearAll") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
        series.setDataAtTime(TimeFrameIndex{20}, std::vector<float>{3.0f}, NotifyObservers::No);
        
        series.clearAll(NotifyObservers::No);
        CHECK(series.getTotalValueCount() == 0);
        CHECK(series.getNumTimePoints() == 0);
    }
    
    SECTION("elements() iteration") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
        series.setDataAtTime(TimeFrameIndex{20}, std::vector<float>{3.0f}, NotifyObservers::No);
        
        std::vector<std::pair<TimeFrameIndex, float>> collected;
        for (auto [time, value] : series.elements()) {
            collected.emplace_back(time, value);
        }
        
        REQUIRE(collected.size() == 3);
        CHECK(collected[0].first == TimeFrameIndex{10});
        CHECK(collected[0].second == 1.0f);
        CHECK(collected[1].first == TimeFrameIndex{10});
        CHECK(collected[1].second == 2.0f);
        CHECK(collected[2].first == TimeFrameIndex{20});
        CHECK(collected[2].second == 3.0f);
    }
    
    SECTION("time_slices() iteration") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
        series.setDataAtTime(TimeFrameIndex{20}, std::vector<float>{3.0f, 4.0f, 5.0f}, NotifyObservers::No);
        
        std::vector<std::pair<TimeFrameIndex, size_t>> slice_info;
        for (auto [time, values_span] : series.time_slices()) {
            slice_info.emplace_back(time, values_span.size());
        }
        
        REQUIRE(slice_info.size() == 2);
        CHECK(slice_info[0].first == TimeFrameIndex{10});
        CHECK(slice_info[0].second == 2);
        CHECK(slice_info[1].first == TimeFrameIndex{20});
        CHECK(slice_info[1].second == 3);
    }
    
    SECTION("Storage cache is valid") {
        series.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
        
        auto cache = series.getStorageCache();
        CHECK(cache.isValid());
    }
}

TEST_CASE("RaggedAnalogTimeSeries lazy storage", "[RaggedAnalogTimeSeries][lazy]") {
    // Create source series
    RaggedAnalogTimeSeries source;
    source.setDataAtTime(TimeFrameIndex{10}, std::vector<float>{1.0f, 2.0f}, NotifyObservers::No);
    source.setDataAtTime(TimeFrameIndex{20}, std::vector<float>{3.0f, 4.0f, 5.0f}, NotifyObservers::No);
    
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
    source.setTimeFrame(time_frame);
    
    SECTION("createFromView creates lazy storage") {
        // Materialize the elements view to a vector for random access
        std::vector<std::pair<TimeFrameIndex, float>> elements_vec;
        for (auto elem : source.elements()) {
            elements_vec.push_back(elem);
        }
        
        // Create transform view
        auto scaled_vec = elements_vec 
            | std::views::transform([](auto pair) {
                return std::make_pair(pair.first, pair.second * 10.0f);
            });
        
        // Need to materialize for random access
        std::vector<std::pair<TimeFrameIndex, float>> materialized(
            std::ranges::begin(scaled_vec),
            std::ranges::end(scaled_vec)
        );
        
        auto view = std::ranges::ref_view(materialized);
        auto lazy_series = RaggedAnalogTimeSeries::createFromView(view, time_frame);
        
        CHECK(lazy_series->isLazy());
        CHECK(lazy_series->getTotalValueCount() == 5);
    }
    
    SECTION("materialize converts to owning storage") {
        // Use getValuesAtTimeVec which works for all storage types
        auto materialized = source.materialize();
        
        CHECK_FALSE(materialized->isLazy());
        CHECK(materialized->getTotalValueCount() == source.getTotalValueCount());
        
        // Verify data is preserved
        auto source_data = source.getDataAtTime(TimeFrameIndex{10});
        auto mat_data = materialized->getDataAtTime(TimeFrameIndex{10});
        REQUIRE(source_data.size() == mat_data.size());
        CHECK(source_data[0] == mat_data[0]);
        CHECK(source_data[1] == mat_data[1]);
    }
}

TEST_CASE("RaggedAnalogTimeSeries range constructor", "[RaggedAnalogTimeSeries]") {
    
    SECTION("Construct from vector of pairs") {
        std::vector<std::pair<TimeFrameIndex, float>> data = {
            {TimeFrameIndex{10}, 1.0f},
            {TimeFrameIndex{10}, 2.0f},
            {TimeFrameIndex{20}, 3.0f}
        };
        
        RaggedAnalogTimeSeries series(data);
        
        CHECK(series.getTotalValueCount() == 3);
        CHECK(series.getNumTimePoints() == 2);
    }
    
    SECTION("Construct from transformed view") {
        std::vector<int> indices = {0, 1, 2, 3, 4};
        
        auto transformed = indices 
            | std::views::transform([](int i) {
                return std::make_pair(TimeFrameIndex{i / 2 * 10}, static_cast<float>(i));
            });
        
        RaggedAnalogTimeSeries series(transformed);
        
        CHECK(series.getTotalValueCount() == 5);
    }
}
