/**
 * @file test_range_reduction_registry.test.cpp
 * @brief Tests for RangeReductionRegistry
 *
 * These tests verify:
 * 1. Reduction registration and lookup
 * 2. Type-safe execution
 * 3. Discovery API
 * 4. Stateless reduction registration
 * 5. Type-erased execution
 * 6. Parameter executor factory
 */

#include "transforms/v2/core/RangeReductionRegistry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <rfl/json.hpp>

#include <cmath>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Test Element Types
// ============================================================================

/**
 * @brief Simple event for testing
 * 
 * Note: This is a simplified test type. Real production types like EventWithId
 * have time() returning TimeFrameIndex, but for testing the registry mechanics
 * we use simple float time to avoid coupling to TimeFrame infrastructure.
 */
struct TestEvent {
    float time_{0.0f};
    int value_{0};

    [[nodiscard]] float time() const { return time_; }
    void setTime(float t) { time_ = t; }

    bool operator==(TestEvent const &) const = default;
};

/**
 * @brief Value point for testing
 */
struct TestValuePoint {
    float time_{0.0f};
    float value_{0.0f};

    [[nodiscard]] float time() const { return time_; }
    void setTime(float t) { time_ = t; }
    [[nodiscard]] float value() const { return value_; }

    bool operator==(TestValuePoint const &) const = default;
};

// ============================================================================
// Test Parameter Types
// ============================================================================

/**
 * @brief Parameters for latency calculation
 */
struct LatencyParams {
    float zero_time{0.0f};///< Reference time point
};

/**
 * @brief Parameters for threshold counting
 */
struct ThresholdParams {
    float threshold{0.0f};///< Value threshold
};

// ============================================================================
// Test Reduction Functions
// ============================================================================

/**
 * @brief Count elements in range (stateless)
 */
inline int countElements(std::span<TestEvent const> events) {
    return static_cast<int>(events.size());
}

/**
 * @brief Sum of values (stateless)
 */
inline float sumValues(std::span<TestValuePoint const> points) {
    return std::accumulate(
            points.begin(),
            points.end(),
            0.0f,
            [](float acc, TestValuePoint const & p) { return acc + p.value(); });
}

/**
 * @brief First positive latency (parameterized)
 */
inline float firstPositiveLatency(std::span<TestEvent const> events,
                                  LatencyParams const & params) {
    for (auto const & e : events) {
        if (e.time() > params.zero_time) {
            return e.time() - params.zero_time;
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

/**
 * @brief Count events above threshold (parameterized)
 */
inline int countAboveThreshold(std::span<TestValuePoint const> points,
                               ThresholdParams const & params) {
    int count = 0;
    for (auto const & p : points) {
        if (p.value() > params.threshold) {
            ++count;
        }
    }
    return count;
}

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Create a fresh registry for each test
 *
 * Note: We can't use the singleton for tests since registrations persist.
 * Instead, we create a local registry instance.
 */
class TestRegistry {
public:
    TestRegistry() {
        // Register test reductions
        registry_.registerStatelessReduction<TestEvent, int>(
                "CountEvents",
                countElements,
                {.description = "Count events in range", .category = "Statistics"});

        registry_.registerStatelessReduction<TestValuePoint, float>(
                "SumValues",
                sumValues,
                {.description = "Sum values in range", .category = "Statistics"});

        registry_.registerReduction<TestEvent, float, LatencyParams>(
                "FirstPositiveLatency",
                firstPositiveLatency,
                {.description = "First event latency after zero_time",
                 .category = "Event Analysis"});

        registry_.registerReduction<TestValuePoint, int, ThresholdParams>(
                "CountAboveThreshold",
                countAboveThreshold,
                {.description = "Count points above threshold",
                 .category = "Statistics"});
    }

    RangeReductionRegistry & get() { return registry_; }

private:
    RangeReductionRegistry registry_;
};

// ============================================================================
// Registration Tests
// ============================================================================

TEST_CASE("RangeReductionRegistry - Stateless reduction registration",
          "[RangeReduction][Registry]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("Reduction is discoverable by name") {
        REQUIRE(registry.hasReduction("CountEvents"));
        REQUIRE(registry.hasReduction("SumValues"));
    }

    SECTION("Metadata is populated correctly") {
        auto const * meta = registry.getMetadata("CountEvents");
        REQUIRE(meta != nullptr);
        CHECK(meta->name == "CountEvents");
        CHECK(meta->description == "Count events in range");
        CHECK(meta->category == "Statistics");
        CHECK(meta->input_type == std::type_index(typeid(TestEvent)));
        CHECK(meta->output_type == std::type_index(typeid(int)));
        CHECK(meta->params_type == std::type_index(typeid(NoReductionParams)));
    }
}

TEST_CASE("RangeReductionRegistry - Parameterized reduction registration",
          "[RangeReduction][Registry]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("Reduction is discoverable by name") {
        REQUIRE(registry.hasReduction("FirstPositiveLatency"));
        REQUIRE(registry.hasReduction("CountAboveThreshold"));
    }

    SECTION("Metadata includes parameter type") {
        auto const * meta = registry.getMetadata("FirstPositiveLatency");
        REQUIRE(meta != nullptr);
        CHECK(meta->params_type == std::type_index(typeid(LatencyParams)));

        auto const * meta2 = registry.getMetadata("CountAboveThreshold");
        REQUIRE(meta2 != nullptr);
        CHECK(meta2->params_type == std::type_index(typeid(ThresholdParams)));
    }
}

// ============================================================================
// Discovery API Tests
// ============================================================================

TEST_CASE("RangeReductionRegistry - Discovery API", "[RangeReduction][Registry]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("getReductionNames returns all registered names") {
        auto names = registry.getReductionNames();
        CHECK(names.size() == 4);
        CHECK(std::find(names.begin(), names.end(), "CountEvents") != names.end());
        CHECK(std::find(names.begin(), names.end(), "SumValues") != names.end());
        CHECK(std::find(names.begin(), names.end(), "FirstPositiveLatency") != names.end());
        CHECK(std::find(names.begin(), names.end(), "CountAboveThreshold") != names.end());
    }

    SECTION("getReductionsForInputType filters correctly") {
        auto event_reductions = registry.getReductionsForInputType<TestEvent>();
        CHECK(event_reductions.size() == 2);
        CHECK(std::find(event_reductions.begin(), event_reductions.end(), "CountEvents") !=
              event_reductions.end());
        CHECK(std::find(event_reductions.begin(), event_reductions.end(), "FirstPositiveLatency") !=
              event_reductions.end());

        auto value_reductions = registry.getReductionsForInputType<TestValuePoint>();
        CHECK(value_reductions.size() == 2);
        CHECK(std::find(value_reductions.begin(), value_reductions.end(), "SumValues") !=
              value_reductions.end());
        CHECK(std::find(value_reductions.begin(), value_reductions.end(), "CountAboveThreshold") !=
              value_reductions.end());
    }

    SECTION("getReductionsForOutputType filters correctly") {
        auto int_outputs = registry.getReductionsForOutputType<int>();
        CHECK(int_outputs.size() == 2);// CountEvents, CountAboveThreshold

        auto float_outputs = registry.getReductionsForOutputType<float>();
        CHECK(float_outputs.size() == 2);// SumValues, FirstPositiveLatency
    }

    SECTION("hasReductionForType checks both name and type") {
        CHECK(registry.hasReductionForType<TestEvent>("CountEvents"));
        CHECK(registry.hasReductionForType<TestEvent>("FirstPositiveLatency"));
        CHECK_FALSE(registry.hasReductionForType<TestEvent>("SumValues"));
        CHECK_FALSE(registry.hasReductionForType<TestValuePoint>("CountEvents"));
    }
}

// ============================================================================
// Type-Safe Execution Tests
// ============================================================================

TEST_CASE("RangeReductionRegistry - Type-safe execution", "[RangeReduction][Execution]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("Stateless reduction executes correctly") {
        std::vector<TestEvent> events = {
                {.time_ = 1.0f, .value_ = 1},
                {.time_ = 2.0f, .value_ = 2},
                {.time_ = 3.0f, .value_ = 3}};

        auto count = registry.execute<TestEvent, int, NoReductionParams>(
                "CountEvents",
                std::span{events},
                NoReductionParams{});

        CHECK(count == 3);
    }

    SECTION("Stateless reduction with values") {
        std::vector<TestValuePoint> points = {
                {.time_ = 1.0f, .value_ = 10.0f},
                {.time_ = 2.0f, .value_ = 20.0f},
                {.time_ = 3.0f, .value_ = 30.0f}};

        auto sum = registry.execute<TestValuePoint, float, NoReductionParams>(
                "SumValues",
                std::span{points},
                NoReductionParams{});

        CHECK_THAT(sum, Catch::Matchers::WithinRel(60.0f, 0.001f));
    }

    SECTION("Parameterized reduction executes correctly") {
        std::vector<TestEvent> events = {
                {.time_ = -1.0f, .value_ = 1},
                {.time_ = 0.5f, .value_ = 2},
                {.time_ = 1.5f, .value_ = 3}};

        auto latency = registry.execute<TestEvent, float, LatencyParams>(
                "FirstPositiveLatency",
                std::span{events},
                LatencyParams{.zero_time = 0.0f});

        CHECK_THAT(latency, Catch::Matchers::WithinRel(0.5f, 0.001f));
    }

    SECTION("Parameterized reduction with threshold") {
        std::vector<TestValuePoint> points = {
                {.time_ = 1.0f, .value_ = 5.0f},
                {.time_ = 2.0f, .value_ = 15.0f},
                {.time_ = 3.0f, .value_ = 25.0f}};

        auto count = registry.execute<TestValuePoint, int, ThresholdParams>(
                "CountAboveThreshold",
                std::span{points},
                ThresholdParams{.threshold = 10.0f});

        CHECK(count == 2);
    }

    SECTION("Empty input range") {
        std::vector<TestEvent> empty;
        auto count = registry.execute<TestEvent, int, NoReductionParams>(
                "CountEvents",
                std::span{empty},
                NoReductionParams{});

        CHECK(count == 0);
    }

    SECTION("Reduction not found throws") {
        std::vector<TestEvent> events = {{.time_ = 1.0f, .value_ = 1}};
        CHECK_THROWS_AS(
                (registry.execute<TestEvent, int, NoReductionParams>(
                        "NonExistent",
                        std::span{events},
                        NoReductionParams{})),
                std::runtime_error);
    }
}

// ============================================================================
// Type-Erased Execution Tests
// ============================================================================

TEST_CASE("RangeReductionRegistry - Type-erased execution", "[RangeReduction][Execution]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("executeErased works with correct types") {
        std::vector<TestEvent> events = {
                {.time_ = 1.0f, .value_ = 1},
                {.time_ = 2.0f, .value_ = 2}};

        std::span<TestEvent const> event_span{events};
        std::any input_any = event_span;
        std::any params_any = NoReductionParams{};

        auto result = registry.executeErased(
                "CountEvents",
                std::type_index(typeid(TestEvent)),
                input_any,
                params_any);

        CHECK(std::any_cast<int>(result) == 2);
    }

    SECTION("executeErased with parameterized reduction") {
        std::vector<TestValuePoint> points = {
                {.time_ = 1.0f, .value_ = 5.0f},
                {.time_ = 2.0f, .value_ = 15.0f}};

        std::span<TestValuePoint const> point_span{points};
        std::any input_any = point_span;
        std::any params_any = ThresholdParams{.threshold = 10.0f};

        auto result = registry.executeErased(
                "CountAboveThreshold",
                std::type_index(typeid(TestValuePoint)),
                input_any,
                params_any);

        CHECK(std::any_cast<int>(result) == 1);
    }
}

// ============================================================================
// Parameter Executor Tests
// ============================================================================

TEST_CASE("RangeReductionRegistry - Parameter executor", "[RangeReduction][Executor]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("Create typed executor with captured params") {
        auto executor = registry.createParamExecutor<TestValuePoint, int, ThresholdParams>(
                "CountAboveThreshold",
                ThresholdParams{.threshold = 10.0f});
        REQUIRE(executor != nullptr);

        std::vector<TestValuePoint> points = {
                {.time_ = 1.0f, .value_ = 5.0f},
                {.time_ = 2.0f, .value_ = 15.0f},
                {.time_ = 3.0f, .value_ = 25.0f}};

        std::span<TestValuePoint const> point_span{points};
        std::any input_any = point_span;

        auto result = executor->execute(input_any);
        CHECK(std::any_cast<int>(result) == 2);
    }

    SECTION("Executor reports correct output type") {
        auto executor = registry.createParamExecutor<TestValuePoint, float, NoReductionParams>(
                "SumValues",
                NoReductionParams{});
        REQUIRE(executor != nullptr);

        CHECK(executor->outputType() == std::type_index(typeid(float)));
    }
}

// ============================================================================
// JSON Parameter Executor Factory Tests
// ============================================================================

TEST_CASE("RangeReductionRegistry - JSON executor factory", "[RangeReduction][JSON]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("Create executor from JSON params") {
        std::string json = R"({"threshold": 10.0})";

        auto executor = registry.createParamExecutorFromJson("CountAboveThreshold", json);
        REQUIRE(executor != nullptr);

        std::vector<TestValuePoint> points = {
                {.time_ = 1.0f, .value_ = 5.0f},
                {.time_ = 2.0f, .value_ = 15.0f}};

        std::span<TestValuePoint const> point_span{points};
        std::any input_any = point_span;

        auto result = executor->execute(input_any);
        CHECK(std::any_cast<int>(result) == 1);
    }

    SECTION("JSON factory for stateless reduction") {
        std::string json = "{}";

        auto executor = registry.createParamExecutorFromJson("CountEvents", json);
        REQUIRE(executor != nullptr);

        std::vector<TestEvent> events = {
                {.time_ = 1.0f, .value_ = 1},
                {.time_ = 2.0f, .value_ = 2}};

        std::span<TestEvent const> event_span{events};
        std::any input_any = event_span;

        auto result = executor->execute(input_any);
        CHECK(std::any_cast<int>(result) == 2);
    }

    SECTION("Unknown reduction returns nullptr") {
        auto executor = registry.createParamExecutorFromJson("NonExistent", "{}");
        CHECK(executor == nullptr);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("RangeReductionRegistry - Edge cases", "[RangeReduction][EdgeCase]") {
    TestRegistry fixture;
    auto & registry = fixture.get();

    SECTION("NaN result from latency with no matching events") {
        std::vector<TestEvent> events = {
                {.time_ = -2.0f, .value_ = 1},
                {.time_ = -1.0f, .value_ = 2}};

        auto latency = registry.execute<TestEvent, float, LatencyParams>(
                "FirstPositiveLatency",
                std::span{events},
                LatencyParams{.zero_time = 0.0f});

        CHECK(std::isnan(latency));
    }

    SECTION("Large input range") {
        std::vector<TestValuePoint> points;
        points.reserve(10000);
        for (int i = 0; i < 10000; ++i) {
            points.push_back({.time_ = static_cast<float>(i), .value_ = 1.0f});
        }

        auto sum = registry.execute<TestValuePoint, float, NoReductionParams>(
                "SumValues",
                std::span{points},
                NoReductionParams{});

        CHECK_THAT(sum, Catch::Matchers::WithinRel(10000.0f, 0.001f));
    }
}
