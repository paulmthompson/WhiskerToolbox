/**
 * @file test_context_aware_params.test.cpp
 * @brief Tests for temporal normalization transforms and TrialContext
 *
 * These tests verify:
 * 1. TrialContext creation and usage
 * 2. NormalizeTimeParams alignment time setting
 * 3. Value projection transforms (normalizeTimeValue, normalizeSampleTimeValue)
 *
 * @note The ContextAwareParams concept and context injection have been removed
 * in favor of the V2 pattern using PipelineValueStore and parameter bindings.
 * See PIPELINE_VALUE_STORE_ROADMAP.md for details.
 */

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/extension/ViewAdaptorTypes.hpp"  // Contains TrialContext
#include "utils/TimeSeriesConcepts.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

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
// NormalizeTimeParams Tests
// ============================================================================

TEST_CASE("NormalizeTimeParams - alignment time", "[NormalizeTimeParams]") {
    SECTION("Not ready before alignment time set") {
        NormalizeTimeParams params;
        CHECK_FALSE(params.hasAlignmentTime());
    }

    SECTION("Ready after setAlignmentTime") {
        NormalizeTimeParams params;
        
        params.setAlignmentTime(TimeFrameIndex{50});
        
        CHECK(params.hasAlignmentTime());
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

TEST_CASE("normalizeTimeValue - value projection", "[NormalizeTimeValue]") {
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
}

TEST_CASE("normalizeTimeValue - error handling", "[NormalizeTimeValue]") {
    SECTION("Throws when alignment time not set") {
        NormalizeTimeParams params;
        TimeFrameIndex time{100};
        
        CHECK_THROWS_AS(normalizeTimeValue(time, params), std::runtime_error);
    }
}

// ============================================================================
// NormalizeSampleTimeValue Transform Tests
// ============================================================================

TEST_CASE("normalizeSampleTimeValue - value projection", "[NormalizeSampleTimeValue]") {
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
}

// ============================================================================
// Batch Normalization Tests
// ============================================================================

TEST_CASE("normalizeEventTimeValue - batch processing", "[Batch]") {
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
