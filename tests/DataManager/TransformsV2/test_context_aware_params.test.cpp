/**
 * @file test_context_aware_params.test.cpp
 * @brief Tests for context-aware parameters and temporal normalization transforms
 *
 * These tests verify:
 * 1. ContextAwareParams concept and detection
 * 2. TrialContext creation and usage
 * 3. NormalizeTimeParams context injection
 * 4. Value projection transforms (normalizeTimeValue, normalizeSampleTimeValue)
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/extension/ContextAwareParams.hpp"
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
// Value Projection Transform Tests
// ============================================================================

TEST_CASE("normalizeTimeValue - value projection", "[ContextAware][NormalizeTimeValue]") {
    NormalizeTimeParams params;
    params.setAlignmentTime(TimeFrameIndex{100});

    SECTION("Positive offset (time after alignment)") {
        TimeFrameIndex time{125};
        
        float normalized = normalizeTimeValue(time, params);
        
        CHECK_THAT(normalized, Catch::Matchers::WithinRel(25.0f, 0.001f));
    }

    SECTION("Negative offset (time before alignment)") {
        TimeFrameIndex time{75};
        
        float normalized = normalizeTimeValue(time, params);
        
        CHECK_THAT(normalized, Catch::Matchers::WithinRel(-25.0f, 0.001f));
    }

    SECTION("Zero offset (time at alignment)") {
        TimeFrameIndex time{100};
        
        float normalized = normalizeTimeValue(time, params);
        
        CHECK_THAT(normalized, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Large offset") {
        TimeFrameIndex time{1000};
        
        float normalized = normalizeTimeValue(time, params);
        
        CHECK_THAT(normalized, Catch::Matchers::WithinRel(900.0f, 0.001f));
    }

    SECTION("With context injection via TrialContext") {
        NormalizeTimeParams ctx_params;
        TrialContext ctx{.alignment_time = TimeFrameIndex{50}};
        ctx_params.setContext(ctx);

        TimeFrameIndex time{75};
        float normalized = normalizeTimeValue(time, ctx_params);

        CHECK_THAT(normalized, Catch::Matchers::WithinRel(25.0f, 0.001f));
    }
}

TEST_CASE("normalizeTimeValue - error handling", "[ContextAware][NormalizeTimeValue]") {
    SECTION("Throws when context not set") {
        NormalizeTimeParams params;
        TimeFrameIndex time{100};
        
        CHECK_THROWS_AS(normalizeTimeValue(time, params), std::runtime_error);
    }
}

// ============================================================================
// NormalizeSampleTimeValue Transform Tests
// ============================================================================

TEST_CASE("normalizeSampleTimeValue - value projection", "[ContextAware][NormalizeSampleTimeValue]") {
    NormalizeTimeParams params;
    params.setAlignmentTime(TimeFrameIndex{100});

    SECTION("Positive offset") {
        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{150}, 3.14f};
        
        float normalized = normalizeSampleTimeValue(sample, params);
        
        CHECK_THAT(normalized, Catch::Matchers::WithinRel(50.0f, 0.001f));
    }

    SECTION("Negative offset") {
        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{80}, -2.5f};
        
        float normalized = normalizeSampleTimeValue(sample, params);
        
        CHECK_THAT(normalized, Catch::Matchers::WithinRel(-20.0f, 0.001f));
    }

    SECTION("With context injection") {
        NormalizeTimeParams ctx_params;
        TrialContext ctx{.alignment_time = TimeFrameIndex{200}};
        ctx_params.setContext(ctx);

        AnalogTimeSeries::TimeValuePoint sample{TimeFrameIndex{250}, 1.0f};
        float normalized = normalizeSampleTimeValue(sample, ctx_params);

        CHECK_THAT(normalized, Catch::Matchers::WithinRel(50.0f, 0.001f));
    }
}

// ============================================================================
// Batch Normalization Tests
// ============================================================================

TEST_CASE("normalizeEventTimeValue - batch processing", "[ContextAware][Batch]") {
    NormalizeTimeParams params;
    params.setAlignmentTime(TimeFrameIndex{100});

    // Events have EntityId which can be accessed separately
    std::vector<EventWithId> events{
        {TimeFrameIndex{75}, EntityId{1}},   // -25
        {TimeFrameIndex{100}, EntityId{2}},  // 0
        {TimeFrameIndex{125}, EntityId{3}},  // +25
        {TimeFrameIndex{200}, EntityId{4}}   // +100
    };

    SECTION("Transform all event times in sequence") {
        std::vector<float> normalized;
        normalized.reserve(events.size());
        
        for (auto const& event : events) {
            // Extract .time() from EventWithId to pass to normalizeTimeValue
            normalized.push_back(normalizeTimeValue(event.time(), params));
        }

        REQUIRE(normalized.size() == 4);
        CHECK_THAT(normalized[0], Catch::Matchers::WithinRel(-25.0f, 0.001f));
        CHECK_THAT(normalized[1], Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(normalized[2], Catch::Matchers::WithinRel(25.0f, 0.001f));
        CHECK_THAT(normalized[3], Catch::Matchers::WithinRel(100.0f, 0.001f));
    }

    SECTION("Source events retain entity IDs") {
        for (size_t i = 0; i < events.size(); ++i) {
            // Extract .time() from EventWithId
            float norm_time = normalizeTimeValue(events[i].time(), params);
            // Entity ID still accessible from source event
            CHECK(events[i].id() == EntityId{static_cast<uint64_t>(i + 1)});
            (void)norm_time;  // Use the result to avoid unused warning
        }
    }
}
