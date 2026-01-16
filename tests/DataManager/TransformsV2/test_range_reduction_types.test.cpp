#include <catch2/catch_test_macros.hpp>

#include "transforms/v2/extension/RangeReductionTypes.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"

#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Test Fixtures: Sample Reduction Functions
// ============================================================================

namespace {

// Simple stateless reduction: count elements
struct CountElements {
    template<std::ranges::input_range R>
    int operator()(R && range) const {
        int count = 0;
        for ([[maybe_unused]] auto const & _ : range) {
            ++count;
        }
        return count;
    }
};

// Parameterized reduction: first positive latency
struct FirstPositiveLatencyParams {
    float default_value = std::numeric_limits<float>::infinity();
};

struct FirstPositiveLatency {
    template<std::ranges::input_range R>
    float operator()(R && range, FirstPositiveLatencyParams const & params) const {
        for (auto const & elem : range) {
            if (elem.time().getValue() > 0) {
                return static_cast<float>(elem.time().getValue());
            }
        }
        return params.default_value;
    }
};

// Parameterized reduction for values: max value
struct MaxValueParams {
    float default_value = -std::numeric_limits<float>::infinity();
};

struct MaxValue {
    template<std::ranges::input_range R>
    float operator()(R && range, MaxValueParams const & params) const {
        float max_val = params.default_value;
        for (auto const & elem : range) {
            float val = static_cast<float>(elem.time().getValue());// Using time as value for simplicity
            if (val > max_val) {
                max_val = val;
            }
        }
        return max_val;
    }
};

}// namespace

// ============================================================================
// Concept Satisfaction Tests
// ============================================================================

TEST_CASE("RangeReductionTypes - Concept satisfaction", "[transforms][v2][range_reduction]") {

    SECTION("StatelessRangeReduction concept") {
        // CountElements should satisfy StatelessRangeReduction
        static_assert(
                StatelessRangeReduction<CountElements, EventWithId, int>,
                "CountElements should satisfy StatelessRangeReduction");
    }

    SECTION("ParameterizedRangeReduction concept") {
        // FirstPositiveLatency should satisfy ParameterizedRangeReduction
        static_assert(
                ParameterizedRangeReduction<FirstPositiveLatency, EventWithId, float, FirstPositiveLatencyParams>,
                "FirstPositiveLatency should satisfy ParameterizedRangeReduction");

        static_assert(
                ParameterizedRangeReduction<MaxValue, EventWithId, float, MaxValueParams>,
                "MaxValue should satisfy ParameterizedRangeReduction");
    }
}

// ============================================================================
// Type Trait Tests
// ============================================================================

TEST_CASE("RangeReductionTypes - Type traits", "[transforms][v2][range_reduction]") {

    SECTION("is_stateless_range_reduction_v") {
        static_assert(
                is_stateless_range_reduction_v<CountElements, EventWithId, int>,
                "CountElements should be detected as stateless");
    }

    SECTION("is_range_reduction_v with params") {
        static_assert(
                is_range_reduction_v<FirstPositiveLatency, EventWithId, float, FirstPositiveLatencyParams>,
                "FirstPositiveLatency should be detected as range reduction");
    }
}

// ============================================================================
// Metadata Tests
// ============================================================================

TEST_CASE("RangeReductionTypes - Metadata structure", "[transforms][v2][range_reduction]") {

    SECTION("Default metadata values") {
        RangeReductionMetadata meta;

        REQUIRE(meta.name.empty());
        REQUIRE(meta.description.empty());
        REQUIRE(meta.category.empty());
        REQUIRE(meta.input_type == typeid(void));
        REQUIRE(meta.output_type == typeid(void));
        REQUIRE(meta.params_type == typeid(void));
        REQUIRE(meta.version == "1.0");
        REQUIRE_FALSE(meta.is_expensive);
        REQUIRE(meta.is_deterministic);
        REQUIRE(meta.requires_time_series_element);
        REQUIRE_FALSE(meta.requires_entity_element);
        REQUIRE_FALSE(meta.requires_value_element);
    }

    SECTION("Populated metadata") {
        RangeReductionMetadata meta{
                .name = "FirstPositiveLatency",
                .description = "Find the time of the first event with positive time",
                .category = "Event Statistics",
                .input_type = typeid(EventWithId),
                .output_type = typeid(float),
                .params_type = typeid(FirstPositiveLatencyParams),
                .input_type_name = "EventWithId",
                .output_type_name = "float",
                .params_type_name = "FirstPositiveLatencyParams",
                .requires_time_series_element = true,
                .requires_entity_element = true,
        };

        REQUIRE(meta.name == "FirstPositiveLatency");
        REQUIRE(meta.input_type == typeid(EventWithId));
        REQUIRE(meta.output_type == typeid(float));
        REQUIRE(meta.requires_entity_element);
    }
}

// ============================================================================
// Functional Tests (actual execution)
// ============================================================================

TEST_CASE("RangeReductionTypes - Reduction execution", "[transforms][v2][range_reduction]") {

    SECTION("CountElements reduction") {
        std::vector<EventWithId> events = {
                EventWithId{TimeFrameIndex{100}, EntityId{1}},
                EventWithId{TimeFrameIndex{200}, EntityId{2}},
                EventWithId{TimeFrameIndex{300}, EntityId{3}},
        };

        CountElements counter;
        int count = counter(events);
        REQUIRE(count == 3);
    }

    SECTION("CountElements on empty range") {
        std::vector<EventWithId> events;

        CountElements counter;
        int count = counter(events);
        REQUIRE(count == 0);
    }

    SECTION("FirstPositiveLatency with positive events") {
        std::vector<EventWithId> events = {
                EventWithId{TimeFrameIndex{-50}, EntityId{1}},
                EventWithId{TimeFrameIndex{-10}, EntityId{2}},
                EventWithId{TimeFrameIndex{25}, EntityId{3}},
                EventWithId{TimeFrameIndex{100}, EntityId{4}},
        };

        FirstPositiveLatency reducer;
        FirstPositiveLatencyParams params{.default_value = -1.0f};

        float latency = reducer(events, params);
        REQUIRE(latency == 25.0f);
    }

    SECTION("FirstPositiveLatency with no positive events") {
        std::vector<EventWithId> events = {
                EventWithId{TimeFrameIndex{-50}, EntityId{1}},
                EventWithId{TimeFrameIndex{-10}, EntityId{2}},
        };

        FirstPositiveLatency reducer;
        FirstPositiveLatencyParams params{.default_value = -999.0f};

        float latency = reducer(events, params);
        REQUIRE(latency == -999.0f);
    }

    SECTION("MaxValue reduction") {
        std::vector<EventWithId> events = {
                EventWithId{TimeFrameIndex{10}, EntityId{1}},
                EventWithId{TimeFrameIndex{50}, EntityId{2}},
                EventWithId{TimeFrameIndex{30}, EntityId{3}},
        };

        MaxValue reducer;
        MaxValueParams params{.default_value = 0.0f};

        float max_val = reducer(events, params);
        REQUIRE(max_val == 50.0f);
    }
}

// ============================================================================
// NoReductionParams Tests
// ============================================================================

TEST_CASE("RangeReductionTypes - NoReductionParams", "[transforms][v2][range_reduction]") {

    SECTION("Default constructible") {
        NoReductionParams params;
        (void) params;// Just verify it compiles
        REQUIRE(true);
    }

    SECTION("Can be used as parameter type") {
        // A reduction that uses NoReductionParams
        auto simple_count = [](std::ranges::input_range auto && range, NoReductionParams const &) {
            int count = 0;
            for ([[maybe_unused]] auto const & _ : range) {
                ++count;
            }
            return count;
        };

        std::vector<EventWithId> events = {
                EventWithId{TimeFrameIndex{100}, EntityId{1}},
                EventWithId{TimeFrameIndex{200}, EntityId{2}},
        };

        int count = simple_count(events, NoReductionParams{});
        REQUIRE(count == 2);
    }
}
