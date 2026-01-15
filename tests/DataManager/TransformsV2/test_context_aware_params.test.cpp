/**
 * @file test_context_aware_params.test.cpp
 * @brief Tests for context-aware parameters and temporal normalization transforms
 *
 * These tests verify:
 * 1. ContextAwareParams concept and detection
 * 2. TrialContext creation and usage
 * 3. NormalizeTimeParams context injection
 * 4. NormalizeEventTime transform
 * 5. NormalizeValueTime transform
 * 6. Edge cases and error handling
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/core/ContextAwareParams.hpp"
#include "utils/TimeSeriesConcepts.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Non-context-aware parameter struct for testing concept detection
 */
struct SimpleParams {
    float threshold{0.5f};
};

/**
 * @brief Context-aware parameter struct for testing
 */
struct TestContextParams {
    float some_value{0.0f};
    std::optional<TimeFrameIndex> cached_alignment;

    void setContext(TrialContext const& ctx) {
        cached_alignment = ctx.alignment_time;
    }

    [[nodiscard]] bool hasContext() const noexcept {
        return cached_alignment.has_value();
    }
};

// ============================================================================
// Concept Detection Tests
// ============================================================================

TEST_CASE("ContextAwareParams - concept detection", "[ContextAware][Concepts]") {
    SECTION("Non-context-aware params") {
        STATIC_CHECK_FALSE(ContextAwareParams<SimpleParams, TrialContext>);
        STATIC_CHECK_FALSE(TrialContextAwareParams<SimpleParams>);
        STATIC_CHECK_FALSE(is_trial_context_aware_v<SimpleParams>);
    }

    SECTION("Custom context-aware params") {
        STATIC_CHECK(ContextAwareParams<TestContextParams, TrialContext>);
        STATIC_CHECK(TrialContextAwareParams<TestContextParams>);
        STATIC_CHECK(is_trial_context_aware_v<TestContextParams>);
    }

    SECTION("NormalizeTimeParams is context-aware") {
        STATIC_CHECK(ContextAwareParams<NormalizeTimeParams, TrialContext>);
        STATIC_CHECK(TrialContextAwareParams<NormalizeTimeParams>);
        STATIC_CHECK(is_trial_context_aware_v<NormalizeTimeParams>);
    }
}

// ============================================================================
// TrialContext Tests
// ============================================================================

TEST_CASE("TrialContext - construction and fields", "[ContextAware][TrialContext]") {
    SECTION("Default construction") {
        TrialContext ctx;
        CHECK(ctx.alignment_time == TimeFrameIndex{0});
        CHECK_FALSE(ctx.trial_index.has_value());
        CHECK_FALSE(ctx.trial_duration.has_value());
        CHECK_FALSE(ctx.end_time.has_value());
    }

    SECTION("Designated initializer construction") {
        TrialContext ctx{
            .alignment_time = TimeFrameIndex{100},
            .trial_index = 5,
            .trial_duration = 200,
            .end_time = TimeFrameIndex{300}
        };

        CHECK(ctx.alignment_time == TimeFrameIndex{100});
        CHECK(ctx.trial_index.value() == 5);
        CHECK(ctx.trial_duration.value() == 200);
        CHECK(ctx.end_time.value() == TimeFrameIndex{300});
    }

    SECTION("Partial initialization") {
        TrialContext ctx{
            .alignment_time = TimeFrameIndex{50}
        };

        CHECK(ctx.alignment_time == TimeFrameIndex{50});
        CHECK_FALSE(ctx.trial_index.has_value());
    }
}

// ============================================================================
// Context Injection Helper Tests
// ============================================================================

TEST_CASE("injectContext - injection behavior", "[ContextAware][Helpers]") {
    TrialContext ctx{.alignment_time = TimeFrameIndex{100}};

    SECTION("Injects into context-aware params") {
        TestContextParams params;
        CHECK_FALSE(params.hasContext());

        injectContext(params, ctx);

        CHECK(params.hasContext());
        CHECK(params.cached_alignment.value() == TimeFrameIndex{100});
    }

    SECTION("No-op for non-context-aware params") {
        SimpleParams params{.threshold = 0.5f};
        
        // Should compile and do nothing
        injectContext(params, ctx);
        
        CHECK(params.threshold == 0.5f);  // Unchanged
    }
}

TEST_CASE("hasRequiredContext - context checking", "[ContextAware][Helpers]") {
    SECTION("Non-context-aware params always ready") {
        SimpleParams params;
        CHECK(hasRequiredContext(params));
    }

    SECTION("Context-aware params not ready before injection") {
        TestContextParams params;
        CHECK_FALSE(hasRequiredContext(params));
    }

    SECTION("Context-aware params ready after injection") {
        TestContextParams params;
        TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
        params.setContext(ctx);
        CHECK(hasRequiredContext(params));
    }
}

// ============================================================================
// NormalizeTimeParams Tests
// ============================================================================

TEST_CASE("NormalizeTimeParams - context injection", "[ContextAware][NormalizeTimeParams]") {
    SECTION("Not ready before context injection") {
        NormalizeTimeParams params;
        CHECK_FALSE(params.hasContext());
    }

    SECTION("Ready after setContext") {
        NormalizeTimeParams params;
        TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
        
        params.setContext(ctx);
        
        CHECK(params.hasContext());
        CHECK(params.getAlignmentTime() == TimeFrameIndex{100});
    }

    SECTION("Ready after manual setAlignmentTime") {
        NormalizeTimeParams params;
        
        params.setAlignmentTime(TimeFrameIndex{50});
        
        CHECK(params.hasContext());
        CHECK(params.getAlignmentTime() == TimeFrameIndex{50});
    }

    SECTION("getAlignmentTime throws when not set") {
        NormalizeTimeParams params;
        CHECK_THROWS_AS(params.getAlignmentTime(), std::runtime_error);
    }
}

// ============================================================================
// NormalizedEvent Tests
// ============================================================================

TEST_CASE("NormalizedEvent - construction and accessors", "[ContextAware][NormalizedEvent]") {
    SECTION("Default construction") {
        NormalizedEvent event;
        CHECK(event.normalized_time == 0.0f);
        CHECK(event.original_time == TimeFrameIndex{0});
        CHECK(event.entity_id == EntityId{0});
    }

    SECTION("Parameterized construction") {
        NormalizedEvent event{25.5f, TimeFrameIndex{125}, EntityId{42}};
        
        CHECK(event.normalized_time == 25.5f);
        CHECK(event.original_time == TimeFrameIndex{125});
        CHECK(event.entity_id == EntityId{42});
    }

    SECTION("Concept accessors") {
        NormalizedEvent event{-15.7f, TimeFrameIndex{85}, EntityId{7}};
        
        // time() truncates to integer
        CHECK(event.time() == TimeFrameIndex{-15});
        CHECK(event.id() == EntityId{7});
        CHECK(event.value() == -15.7f);
    }

    SECTION("Equality comparison") {
        NormalizedEvent a{25.0f, TimeFrameIndex{125}, EntityId{1}};
        NormalizedEvent b{25.0f, TimeFrameIndex{125}, EntityId{1}};
        NormalizedEvent c{26.0f, TimeFrameIndex{125}, EntityId{1}};
        
        CHECK(a == b);
        CHECK_FALSE(a == c);
    }
}

// ============================================================================
// NormalizedValue Tests
// ============================================================================

TEST_CASE("NormalizedValue - construction and accessors", "[ContextAware][NormalizedValue]") {
    SECTION("Default construction") {
        NormalizedValue val;
        CHECK(val.normalized_time == 0.0f);
        CHECK(val.sample_value == 0.0f);
        CHECK(val.original_time == TimeFrameIndex{0});
    }

    SECTION("Parameterized construction") {
        NormalizedValue val{50.5f, 3.14f, TimeFrameIndex{150}};
        
        CHECK(val.normalized_time == 50.5f);
        CHECK(val.sample_value == 3.14f);
        CHECK(val.original_time == TimeFrameIndex{150});
    }

    SECTION("Concept accessors - value returns sample, not time") {
        NormalizedValue val{50.0f, 3.14f, TimeFrameIndex{150}};
        
        CHECK(val.time() == TimeFrameIndex{50});
        CHECK(val.value() == 3.14f);  // Returns sample_value, not normalized_time
    }

    SECTION("Equality comparison") {
        NormalizedValue a{50.0f, 3.14f, TimeFrameIndex{150}};
        NormalizedValue b{50.0f, 3.14f, TimeFrameIndex{150}};
        NormalizedValue c{50.0f, 2.71f, TimeFrameIndex{150}};  // Different value
        
        CHECK(a == b);
        CHECK_FALSE(a == c);
    }
}

// ============================================================================
// NormalizeEventTime Transform Tests
// ============================================================================

TEST_CASE("normalizeEventTime - event normalization", "[ContextAware][NormalizeEventTime]") {
    NormalizeTimeParams params;
    params.setAlignmentTime(TimeFrameIndex{100});

    SECTION("Positive offset (event after alignment)") {
        EventWithId event{TimeFrameIndex{125}, EntityId{1}};
        
        auto normalized = normalizeEventTime(event, params);
        
        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(25.0f, 0.001f));
        CHECK(normalized.original_time == TimeFrameIndex{125});
        CHECK(normalized.entity_id == EntityId{1});
    }

    SECTION("Negative offset (event before alignment)") {
        EventWithId event{TimeFrameIndex{75}, EntityId{2}};
        
        auto normalized = normalizeEventTime(event, params);
        
        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(-25.0f, 0.001f));
        CHECK(normalized.original_time == TimeFrameIndex{75});
    }

    SECTION("Zero offset (event at alignment)") {
        EventWithId event{TimeFrameIndex{100}, EntityId{3}};
        
        auto normalized = normalizeEventTime(event, params);
        
        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Large offset") {
        EventWithId event{TimeFrameIndex{1000}, EntityId{4}};
        
        auto normalized = normalizeEventTime(event, params);
        
        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(900.0f, 0.001f));
    }

    SECTION("With context injection via TrialContext") {
        NormalizeTimeParams ctx_params;
        TrialContext ctx{.alignment_time = TimeFrameIndex{50}};
        ctx_params.setContext(ctx);

        EventWithId event{TimeFrameIndex{75}, EntityId{5}};
        auto normalized = normalizeEventTime(event, ctx_params);

        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(25.0f, 0.001f));
    }
}

TEST_CASE("normalizeEventTime - error handling", "[ContextAware][NormalizeEventTime]") {
    SECTION("Throws when context not set") {
        NormalizeTimeParams params;
        EventWithId event{TimeFrameIndex{100}, EntityId{1}};
        
        CHECK_THROWS_AS(normalizeEventTime(event, params), std::runtime_error);
    }
}

// ============================================================================
// NormalizeValueTime Transform Tests
// ============================================================================

TEST_CASE("normalizeValueTime - value normalization", "[ContextAware][NormalizeValueTime]") {
    NormalizeTimeParams params;
    params.setAlignmentTime(TimeFrameIndex{100});

    SECTION("Positive offset with value preservation") {
        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{150}, 3.14f};
        
        auto normalized = normalizeValueTime(sample, params);
        
        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(50.0f, 0.001f));
        CHECK_THAT(normalized.sample_value, Catch::Matchers::WithinRel(3.14f, 0.001f));
        CHECK(normalized.original_time == TimeFrameIndex{150});
    }

    SECTION("Negative offset") {
        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{80}, -2.5f};
        
        auto normalized = normalizeValueTime(sample, params);
        
        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(-20.0f, 0.001f));
        CHECK_THAT(normalized.sample_value, Catch::Matchers::WithinRel(-2.5f, 0.001f));
    }

    SECTION("Zero value preserved") {
        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{120}, 0.0f};
        
        auto normalized = normalizeValueTime(sample, params);
        
        CHECK_THAT(normalized.sample_value, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("With context injection") {
        NormalizeTimeParams ctx_params;
        TrialContext ctx{.alignment_time = TimeFrameIndex{200}};
        ctx_params.setContext(ctx);

        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{250}, 1.0f};
        auto normalized = normalizeValueTime(sample, ctx_params);

        CHECK_THAT(normalized.normalized_time, Catch::Matchers::WithinRel(50.0f, 0.001f));
    }
}

// ============================================================================
// Batch Normalization Tests
// ============================================================================

TEST_CASE("normalizeEventTime - batch processing", "[ContextAware][Batch]") {
    NormalizeTimeParams params;
    params.setAlignmentTime(TimeFrameIndex{100});

    std::vector<EventWithId> events{
        {TimeFrameIndex{75}, EntityId{1}},   // -25
        {TimeFrameIndex{100}, EntityId{2}},  // 0
        {TimeFrameIndex{125}, EntityId{3}},  // +25
        {TimeFrameIndex{200}, EntityId{4}}   // +100
    };

    SECTION("Transform all events in sequence") {
        std::vector<NormalizedEvent> normalized;
        normalized.reserve(events.size());
        
        for (auto const& event : events) {
            normalized.push_back(normalizeEventTime(event, params));
        }

        REQUIRE(normalized.size() == 4);
        CHECK_THAT(normalized[0].normalized_time, Catch::Matchers::WithinRel(-25.0f, 0.001f));
        CHECK_THAT(normalized[1].normalized_time, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(normalized[2].normalized_time, Catch::Matchers::WithinRel(25.0f, 0.001f));
        CHECK_THAT(normalized[3].normalized_time, Catch::Matchers::WithinRel(100.0f, 0.001f));
    }

    SECTION("Entity IDs preserved") {
        std::vector<NormalizedEvent> normalized;
        for (auto const& event : events) {
            normalized.push_back(normalizeEventTime(event, params));
        }

        CHECK(normalized[0].entity_id == EntityId{1});
        CHECK(normalized[1].entity_id == EntityId{2});
        CHECK(normalized[2].entity_id == EntityId{3});
        CHECK(normalized[3].entity_id == EntityId{4});
    }
}

// ============================================================================
// Type Trait Tests
// ============================================================================

TEST_CASE("Type traits - concept satisfaction", "[ContextAware][TypeTraits]") {
    SECTION("NormalizedEvent satisfies concepts") {
        using namespace WhiskerToolbox::Concepts;
        
        STATIC_CHECK(TimeSeriesElement<NormalizedEvent>);
        STATIC_CHECK(EntityElement<NormalizedEvent>);
        STATIC_CHECK(ValueElement<NormalizedEvent, float>);
    }

    SECTION("NormalizedValue satisfies concepts") {
        using namespace WhiskerToolbox::Concepts;
        
        STATIC_CHECK(TimeSeriesElement<NormalizedValue>);
        STATIC_CHECK(ValueElement<NormalizedValue, float>);
        // NormalizedValue does NOT satisfy EntityElement (no id())
    }
}
