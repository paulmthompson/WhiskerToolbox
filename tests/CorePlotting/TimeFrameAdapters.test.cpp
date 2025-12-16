#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/TimeFrameAdapters.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

#include <ranges>
#include <vector>

using namespace CorePlotting;

// ============================================================================
// Test Fixtures
// ============================================================================

namespace {

/**
 * @brief Create a simple TimeFrame with times [0, 10, 20, 30, ...]
 */
std::shared_ptr<TimeFrame> createLinearTimeFrame(int count, int step = 10) {
    std::vector<int> times;
    times.reserve(count);
    for (int i = 0; i < count; ++i) {
        times.push_back(i * step);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a TimeFrame with non-uniform spacing
 * Times: [0, 5, 15, 30, 50, 75, ...]  (increasing gaps)
 */
std::shared_ptr<TimeFrame> createNonUniformTimeFrame(int count) {
    std::vector<int> times;
    times.reserve(count);
    int current = 0;
    for (int i = 0; i < count; ++i) {
        times.push_back(current);
        current += (i + 1) * 5;  // Gap increases: 5, 10, 15, 20, ...
    }
    return std::make_shared<TimeFrame>(times);
}

// Simple EventWithId-like struct for testing
struct TestEventWithId {
    TimeFrameIndex event_time;
    EntityId entity_id;
};

// Simple IntervalWithId-like struct for testing
struct TestIntervalWithId {
    Interval interval;
    EntityId entity_id;
};

} // anonymous namespace

// ============================================================================
// ToAbsoluteTimeAdapter Tests
// ============================================================================

TEST_CASE("ToAbsoluteTimeAdapter - pair<TimeFrameIndex, float>", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    
    SECTION("Single element transformation") {
        ToAbsoluteTimeAdapter adapter(tf.get());
        
        auto result = adapter(std::pair{TimeFrameIndex{0}, 1.5f});
        REQUIRE(result.time == 0);
        REQUIRE(result.value == 1.5f);
        
        result = adapter(std::pair{TimeFrameIndex{5}, -2.5f});
        REQUIRE(result.time == 50);
        REQUIRE(result.value == -2.5f);
    }
    
    SECTION("Range transformation with pipe operator") {
        std::vector<std::pair<TimeFrameIndex, float>> data = {
            {TimeFrameIndex{0}, 1.0f},
            {TimeFrameIndex{1}, 2.0f},
            {TimeFrameIndex{2}, 3.0f},
            {TimeFrameIndex{3}, 4.0f}
        };
        
        std::vector<int> times;
        std::vector<float> values;
        
        for (auto const& [time, value] : data | toAbsoluteTime(tf.get())) {
            times.push_back(time);
            values.push_back(value);
        }
        
        REQUIRE(times == std::vector<int>{0, 10, 20, 30});
        REQUIRE(values == std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f});
    }
    
    SECTION("Works with views::filter") {
        std::vector<std::pair<TimeFrameIndex, float>> data = {
            {TimeFrameIndex{0}, 1.0f},
            {TimeFrameIndex{1}, -2.0f},  // negative
            {TimeFrameIndex{2}, 3.0f},
            {TimeFrameIndex{3}, -4.0f}   // negative
        };
        
        std::vector<int> positive_times;
        
        for (auto const& [time, value] : data 
                | std::views::filter([](auto const& p) { return p.second > 0; })
                | toAbsoluteTime(tf.get())) {
            positive_times.push_back(time);
        }
        
        REQUIRE(positive_times == std::vector<int>{0, 20});
    }
}

TEST_CASE("ToAbsoluteTimeAdapter - bare TimeFrameIndex", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    
    SECTION("Single element transformation") {
        ToAbsoluteTimeAdapter adapter(tf.get());
        
        int result = adapter(TimeFrameIndex{0});
        REQUIRE(result == 0);
        
        result = adapter(TimeFrameIndex{5});
        REQUIRE(result == 50);
        
        result = adapter(TimeFrameIndex{9});
        REQUIRE(result == 90);
    }
    
    SECTION("Range transformation with pipe operator") {
        std::vector<TimeFrameIndex> indices = {
            TimeFrameIndex{0},
            TimeFrameIndex{2},
            TimeFrameIndex{5},
            TimeFrameIndex{9}
        };
        
        std::vector<int> times;
        for (int time : indices | toAbsoluteTime(tf.get())) {
            times.push_back(time);
        }
        
        REQUIRE(times == std::vector<int>{0, 20, 50, 90});
    }
}

TEST_CASE("ToAbsoluteTimeAdapter - EventWithId-like types", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    
    SECTION("Single element transformation") {
        ToAbsoluteTimeAdapter adapter(tf.get());
        
        TestEventWithId event{TimeFrameIndex{3}, EntityId{42}};
        auto result = adapter(event);
        
        REQUIRE(result.time == 30);
        REQUIRE(result.entity_id == EntityId{42});
    }
    
    SECTION("Range transformation") {
        std::vector<TestEventWithId> events = {
            {TimeFrameIndex{1}, EntityId{100}},
            {TimeFrameIndex{4}, EntityId{101}},
            {TimeFrameIndex{7}, EntityId{102}}
        };
        
        std::vector<int> times;
        std::vector<EntityId> ids;
        
        for (auto const& result : events | toAbsoluteTime(tf.get())) {
            times.push_back(result.time);
            ids.push_back(result.entity_id);
        }
        
        REQUIRE(times == std::vector<int>{10, 40, 70});
        REQUIRE(ids == std::vector<EntityId>{EntityId{100}, EntityId{101}, EntityId{102}});
    }
}

TEST_CASE("ToAbsoluteTimeAdapter - Interval types", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    
    SECTION("Bare Interval") {
        ToAbsoluteTimeAdapter adapter(tf.get());
        
        Interval interval{2, 5};  // indices 2-5 → times 20-50
        auto result = adapter(interval);
        
        REQUIRE(result.start == 20);
        REQUIRE(result.end == 50);
    }
    
    SECTION("IntervalWithId") {
        ToAbsoluteTimeAdapter adapter(tf.get());
        
        TestIntervalWithId interval_with_id{Interval{1, 3}, EntityId{999}};
        auto result = adapter(interval_with_id);
        
        REQUIRE(result.start == 10);
        REQUIRE(result.end == 30);
        REQUIRE(result.entity_id == EntityId{999});
    }
    
    SECTION("Range of Intervals") {
        std::vector<Interval> intervals = {
            {0, 2},
            {3, 5},
            {6, 9}
        };
        
        std::vector<AbsoluteTimeInterval> results;
        for (auto const& result : intervals | toAbsoluteTime(tf.get())) {
            results.push_back(result);
        }
        
        REQUIRE(results.size() == 3);
        REQUIRE(results[0].start == 0);
        REQUIRE(results[0].end == 20);
        REQUIRE(results[1].start == 30);
        REQUIRE(results[1].end == 50);
        REQUIRE(results[2].start == 60);
        REQUIRE(results[2].end == 90);
    }
}

TEST_CASE("ToAbsoluteTimeAdapter - Non-uniform TimeFrame", "[TimeFrameAdapters]") {
    auto tf = createNonUniformTimeFrame(6);  // [0, 5, 15, 30, 50, 75]
    
    SECTION("Indices map to correct non-uniform times") {
        std::vector<TimeFrameIndex> indices = {
            TimeFrameIndex{0},
            TimeFrameIndex{1},
            TimeFrameIndex{2},
            TimeFrameIndex{3},
            TimeFrameIndex{4},
            TimeFrameIndex{5}
        };
        
        std::vector<int> times;
        for (int time : indices | toAbsoluteTime(tf.get())) {
            times.push_back(time);
        }
        
        REQUIRE(times == std::vector<int>{0, 5, 15, 30, 50, 75});
    }
}

// ============================================================================
// toTimeFrameIndex (Inverse) Tests
// ============================================================================

TEST_CASE("toTimeFrameIndex - Linear TimeFrame", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    
    SECTION("Exact matches") {
        REQUIRE(toTimeFrameIndex(0, tf.get()) == TimeFrameIndex{0});
        REQUIRE(toTimeFrameIndex(10, tf.get()) == TimeFrameIndex{1});
        REQUIRE(toTimeFrameIndex(50, tf.get()) == TimeFrameIndex{5});
        REQUIRE(toTimeFrameIndex(90, tf.get()) == TimeFrameIndex{9});
    }
    
    SECTION("Between values - preceding") {
        // 15 is between index 1 (time 10) and index 2 (time 20)
        // Should return closest, which depends on TimeFrame implementation
        auto idx = toTimeFrameIndex(15, tf.get(), true);
        REQUIRE((idx == TimeFrameIndex{1} || idx == TimeFrameIndex{2}));
    }
    
    SECTION("Float input") {
        REQUIRE(toTimeFrameIndex(0.0f, tf.get()) == TimeFrameIndex{0});
        REQUIRE(toTimeFrameIndex(50.0f, tf.get()) == TimeFrameIndex{5});
    }
}

TEST_CASE("toTimeFrameIndex - Non-uniform TimeFrame", "[TimeFrameAdapters]") {
    auto tf = createNonUniformTimeFrame(6);  // [0, 5, 15, 30, 50, 75]
    
    SECTION("Exact matches") {
        REQUIRE(toTimeFrameIndex(0, tf.get()) == TimeFrameIndex{0});
        REQUIRE(toTimeFrameIndex(5, tf.get()) == TimeFrameIndex{1});
        REQUIRE(toTimeFrameIndex(15, tf.get()) == TimeFrameIndex{2});
        REQUIRE(toTimeFrameIndex(30, tf.get()) == TimeFrameIndex{3});
    }
}

// ============================================================================
// TimeFrameConverter Tests
// ============================================================================

TEST_CASE("TimeFrameConverter - Bidirectional conversion", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    TimeFrameConverter converter(tf.get());
    
    SECTION("Forward conversion") {
        REQUIRE(converter.toAbsolute(TimeFrameIndex{0}) == 0);
        REQUIRE(converter.toAbsolute(TimeFrameIndex{5}) == 50);
        REQUIRE(converter.toAbsolute(TimeFrameIndex{9}) == 90);
    }
    
    SECTION("Inverse conversion") {
        REQUIRE(converter.toIndex(0) == TimeFrameIndex{0});
        REQUIRE(converter.toIndex(50) == TimeFrameIndex{5});
        REQUIRE(converter.toIndex(90) == TimeFrameIndex{9});
    }
    
    SECTION("Round-trip: index → absolute → index") {
        for (int i = 0; i < 10; ++i) {
            TimeFrameIndex original{i};
            int absolute = converter.toAbsolute(original);
            TimeFrameIndex round_trip = converter.toIndex(absolute);
            REQUIRE(round_trip == original);
        }
    }
    
    SECTION("Adapter method") {
        std::vector<std::pair<TimeFrameIndex, float>> data = {
            {TimeFrameIndex{0}, 1.0f},
            {TimeFrameIndex{3}, 2.0f}
        };
        
        std::vector<int> times;
        for (auto const& [time, value] : data | converter.adapter()) {
            times.push_back(time);
        }
        
        REQUIRE(times == std::vector<int>{0, 30});
    }
}

// ============================================================================
// Cross-TimeFrame Conversion Tests
// ============================================================================

TEST_CASE("convertTimeFrameIndex - Same TimeFrame", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(10);
    
    // Same pointer - should return input unchanged
    auto result = convertTimeFrameIndex(TimeFrameIndex{5}, tf.get(), tf.get());
    REQUIRE(result == TimeFrameIndex{5});
}

TEST_CASE("convertTimeFrameIndex - Different TimeFrames", "[TimeFrameAdapters]") {
    // Source: [0, 10, 20, 30, 40, 50] (step 10)
    auto source_tf = createLinearTimeFrame(6, 10);
    
    // Target: [0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50] (step 5, 2x resolution)
    auto target_tf = createLinearTimeFrame(11, 5);
    
    SECTION("Index 0 in source → Index 0 in target") {
        // Source index 0 = time 0 → Target index 0 = time 0
        auto result = convertTimeFrameIndex(TimeFrameIndex{0}, source_tf.get(), target_tf.get());
        REQUIRE(result == TimeFrameIndex{0});
    }
    
    SECTION("Index 1 in source → Index 2 in target") {
        // Source index 1 = time 10 → Target index 2 = time 10
        auto result = convertTimeFrameIndex(TimeFrameIndex{1}, source_tf.get(), target_tf.get());
        REQUIRE(result == TimeFrameIndex{2});
    }
    
    SECTION("Index 5 in source → Index 10 in target") {
        // Source index 5 = time 50 → Target index 10 = time 50
        auto result = convertTimeFrameIndex(TimeFrameIndex{5}, source_tf.get(), target_tf.get());
        REQUIRE(result == TimeFrameIndex{10});
    }
}

TEST_CASE("ToTargetFrameAdapter - Range conversion", "[TimeFrameAdapters]") {
    // Source: [0, 100, 200, 300, 400] (step 100)
    auto source_tf = createLinearTimeFrame(5, 100);
    
    // Target: [0, 50, 100, 150, 200, 250, 300, 350, 400] (step 50, 2x resolution)
    auto target_tf = createLinearTimeFrame(9, 50);
    
    SECTION("TimeFrameIndex range conversion") {
        std::vector<TimeFrameIndex> source_indices = {
            TimeFrameIndex{0},  // time 0 → target index 0
            TimeFrameIndex{1},  // time 100 → target index 2
            TimeFrameIndex{2},  // time 200 → target index 4
            TimeFrameIndex{4}   // time 400 → target index 8
        };
        
        std::vector<TimeFrameIndex> target_indices;
        for (auto idx : source_indices | toTargetFrame(source_tf.get(), target_tf.get())) {
            target_indices.push_back(idx);
        }
        
        REQUIRE(target_indices.size() == 4);
        REQUIRE(target_indices[0] == TimeFrameIndex{0});
        REQUIRE(target_indices[1] == TimeFrameIndex{2});
        REQUIRE(target_indices[2] == TimeFrameIndex{4});
        REQUIRE(target_indices[3] == TimeFrameIndex{8});
    }
    
    SECTION("Time-value pair range conversion") {
        std::vector<std::pair<TimeFrameIndex, float>> data = {
            {TimeFrameIndex{0}, 1.0f},
            {TimeFrameIndex{2}, 2.0f},
            {TimeFrameIndex{4}, 3.0f}
        };
        
        std::vector<TimeFrameIndex> target_indices;
        std::vector<float> values;
        
        for (auto const& [idx, val] : data | toTargetFrame(source_tf.get(), target_tf.get())) {
            target_indices.push_back(idx);
            values.push_back(val);
        }
        
        REQUIRE(target_indices == std::vector<TimeFrameIndex>{
            TimeFrameIndex{0}, TimeFrameIndex{4}, TimeFrameIndex{8}
        });
        REQUIRE(values == std::vector<float>{1.0f, 2.0f, 3.0f});
    }
}

// ============================================================================
// Chaining Adapters Tests
// ============================================================================

TEST_CASE("Chaining adapters", "[TimeFrameAdapters]") {
    // Source: [0, 100, 200, 300, 400]
    auto source_tf = createLinearTimeFrame(5, 100);
    
    // Target/Master: [0, 50, 100, 150, 200, 250, 300, 350, 400]
    auto master_tf = createLinearTimeFrame(9, 50);
    
    SECTION("toTargetFrame then toAbsoluteTime") {
        std::vector<std::pair<TimeFrameIndex, float>> data = {
            {TimeFrameIndex{0}, 1.0f},  // source idx 0 → master idx 0 → time 0
            {TimeFrameIndex{2}, 2.0f},  // source idx 2 → master idx 4 → time 200
            {TimeFrameIndex{4}, 3.0f}   // source idx 4 → master idx 8 → time 400
        };
        
        std::vector<int> absolute_times;
        std::vector<float> values;
        
        for (auto const& [time, val] : data 
                | toTargetFrame(source_tf.get(), master_tf.get())
                | toAbsoluteTime(master_tf.get())) {
            absolute_times.push_back(time);
            values.push_back(val);
        }
        
        // Should get same times regardless of which timeframe we use
        // (since they have the same absolute times at corresponding indices)
        REQUIRE(absolute_times == std::vector<int>{0, 200, 400});
        REQUIRE(values == std::vector<float>{1.0f, 2.0f, 3.0f});
    }
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_CASE("Edge cases", "[TimeFrameAdapters]") {
    auto tf = createLinearTimeFrame(5);  // [0, 10, 20, 30, 40]
    
    SECTION("Empty range") {
        std::vector<TimeFrameIndex> empty;
        
        int count = 0;
        for ([[maybe_unused]] int time : empty | toAbsoluteTime(tf.get())) {
            ++count;
        }
        
        REQUIRE(count == 0);
    }
    
    SECTION("Single element range") {
        std::vector<TimeFrameIndex> single = {TimeFrameIndex{2}};
        
        std::vector<int> times;
        for (int time : single | toAbsoluteTime(tf.get())) {
            times.push_back(time);
        }
        
        REQUIRE(times == std::vector<int>{20});
    }
}
