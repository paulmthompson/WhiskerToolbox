/**
 * @file test_value_projection_types.test.cpp
 * @brief Tests for value projection types and helper functions
 *
 * These tests verify:
 * 1. ValueProjectionFn type works correctly
 * 2. ValueProjectionFactory creates projections from context
 * 3. makeProjectedView creates lazy views
 * 4. makeValueView creates value-only views
 * 5. Type erasure and recovery helpers work correctly
 * 6. Concepts detect valid projections
 */

#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/extension/ContextAwareParams.hpp"
#include "transforms/v2/extension/ValueProjectionTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <numeric>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Create test events for projection tests
 */
std::vector<EventWithId> createTestEvents() {
    return {
        EventWithId{TimeFrameIndex{100}, EntityId{1}},
        EventWithId{TimeFrameIndex{150}, EntityId{2}},
        EventWithId{TimeFrameIndex{200}, EntityId{1}},
        EventWithId{TimeFrameIndex{250}, EntityId{3}},
        EventWithId{TimeFrameIndex{300}, EntityId{2}}
    };
}

/**
 * @brief Simple normalization function for testing
 */
float normalizeTime(EventWithId const& event, TimeFrameIndex alignment) {
    return static_cast<float>(event.time().getValue() - alignment.getValue());
}

// ============================================================================
// Basic Type Tests
// ============================================================================

TEST_CASE("ValueProjectionFn - basic usage", "[ValueProjection][Types]") {
    auto events = createTestEvents();
    TimeFrameIndex alignment{100};

    // Create a simple value projection
    ValueProjectionFn<EventWithId, float> projection = 
        [alignment](EventWithId const& event) {
            return normalizeTime(event, alignment);
        };

    SECTION("Project single element") {
        float result = projection(events[0]);
        REQUIRE_THAT(result, WithinAbs(0.0f, 0.001f));  // 100 - 100 = 0
    }

    SECTION("Project all elements") {
        std::vector<float> results;
        for (auto const& event : events) {
            results.push_back(projection(event));
        }

        REQUIRE(results.size() == 5);
        REQUIRE_THAT(results[0], WithinAbs(0.0f, 0.001f));    // 100 - 100
        REQUIRE_THAT(results[1], WithinAbs(50.0f, 0.001f));   // 150 - 100
        REQUIRE_THAT(results[2], WithinAbs(100.0f, 0.001f));  // 200 - 100
        REQUIRE_THAT(results[3], WithinAbs(150.0f, 0.001f));  // 250 - 100
        REQUIRE_THAT(results[4], WithinAbs(200.0f, 0.001f));  // 300 - 100
    }

    SECTION("Source element accessible for identity") {
        for (auto const& event : events) {
            float norm_time = projection(event);
            EntityId id = event.id();  // Still accessible from source

            // Verify we can use both together
            REQUIRE(id >= EntityId{1});
            REQUIRE(id <= EntityId{3});
            (void)norm_time;  // Used
        }
    }
}

TEST_CASE("ValueProjectionFactory - context-aware creation", "[ValueProjection][Factory]") {
    auto events = createTestEvents();

    // Factory that captures alignment from context
    ValueProjectionFactory<EventWithId, float> factory = 
        [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
            TimeFrameIndex alignment = ctx.alignment_time;
            return [alignment](EventWithId const& event) {
                return normalizeTime(event, alignment);
            };
        };

    SECTION("Create projection with context alignment=100") {
        TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
        auto projection = factory(ctx);

        REQUIRE_THAT(projection(events[0]), WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(projection(events[1]), WithinAbs(50.0f, 0.001f));
    }

    SECTION("Create projection with context alignment=200") {
        TrialContext ctx{.alignment_time = TimeFrameIndex{200}};
        auto projection = factory(ctx);

        REQUIRE_THAT(projection(events[0]), WithinAbs(-100.0f, 0.001f));  // 100 - 200
        REQUIRE_THAT(projection(events[2]), WithinAbs(0.0f, 0.001f));     // 200 - 200
        REQUIRE_THAT(projection(events[4]), WithinAbs(100.0f, 0.001f));   // 300 - 200
    }

    SECTION("Different contexts produce different projections") {
        TrialContext ctx1{.alignment_time = TimeFrameIndex{100}};
        TrialContext ctx2{.alignment_time = TimeFrameIndex{150}};

        auto proj1 = factory(ctx1);
        auto proj2 = factory(ctx2);

        // Same event, different results
        REQUIRE_THAT(proj1(events[1]), WithinAbs(50.0f, 0.001f));   // 150 - 100
        REQUIRE_THAT(proj2(events[1]), WithinAbs(0.0f, 0.001f));    // 150 - 150
    }
}

// ============================================================================
// Projected View Tests
// ============================================================================

TEST_CASE("makeProjectedView - lazy iteration", "[ValueProjection][View]") {
    auto events = createTestEvents();
    TimeFrameIndex alignment{100};

    auto projection = [alignment](EventWithId const& event) {
        return normalizeTime(event, alignment);
    };

    SECTION("Iterate with both element and projected value") {
        // makeProjectedView returns pairs of (Element const&, Value)
        // std::make_pair with std::cref creates pair<T const&, U>
        auto projected_view = makeProjectedView(events, projection);

        std::vector<std::pair<EntityId, float>> collected;
        for (auto const& pair : projected_view) {
            // pair.first is EventWithId const& (reference to original)
            // pair.second is float (projected value)
            collected.emplace_back(pair.first.id(), pair.second);
        }

        REQUIRE(collected.size() == 5);
        REQUIRE(collected[0].first == EntityId{1});
        REQUIRE_THAT(collected[0].second, WithinAbs(0.0f, 0.001f));
        REQUIRE(collected[1].first == EntityId{2});
        REQUIRE_THAT(collected[1].second, WithinAbs(50.0f, 0.001f));
    }

    SECTION("View is lazy - projection computed on iteration") {
        int call_count = 0;
        auto counting_projection = [&call_count, alignment](EventWithId const& event) {
            ++call_count;
            return normalizeTime(event, alignment);
        };

        auto projected_view = makeProjectedView(events, counting_projection);
        REQUIRE(call_count == 0);  // Not computed yet

        // Iterate and dereference to trigger projection
        auto it = projected_view.begin();
        [[maybe_unused]] auto val1 = *it;  // First projection call
        ++it;
        [[maybe_unused]] auto val2 = *it;  // Second projection call
        REQUIRE(call_count == 2);  // Computed when dereferenced
    }
}

TEST_CASE("makeValueView - value-only iteration", "[ValueProjection][View]") {
    auto events = createTestEvents();
    TimeFrameIndex alignment{100};

    auto projection = [alignment](EventWithId const& event) {
        return normalizeTime(event, alignment);
    };

    SECTION("Collect all values") {
        auto value_view = makeValueView(events, projection);

        std::vector<float> values;
        for (float v : value_view) {
            values.push_back(v);
        }

        REQUIRE(values.size() == 5);
        REQUIRE_THAT(values[0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(values[4], WithinAbs(200.0f, 0.001f));
    }

    SECTION("Use with algorithms") {
        auto value_view = makeValueView(events, projection);

        // Sum all normalized times
        float sum = 0.0f;
        for (float v : value_view) {
            sum += v;
        }

        // 0 + 50 + 100 + 150 + 200 = 500
        REQUIRE_THAT(sum, WithinAbs(500.0f, 0.001f));
    }

    SECTION("View is lazy") {
        int call_count = 0;
        auto counting_projection = [&call_count, alignment](EventWithId const& event) {
            ++call_count;
            return normalizeTime(event, alignment);
        };

        auto value_view = makeValueView(events, counting_projection);
        REQUIRE(call_count == 0);

        // Count elements without storing
        int count = 0;
        for ([[maybe_unused]] float v : value_view) {
            ++count;
        }
        REQUIRE(count == 5);
        REQUIRE(call_count == 5);
    }
}

// ============================================================================
// Type Erasure Tests
// ============================================================================

TEST_CASE("eraseValueProjection - type erasure", "[ValueProjection][TypeErasure]") {
    TimeFrameIndex alignment{100};

    ValueProjectionFn<EventWithId, float> typed_fn = 
        [alignment](EventWithId const& event) {
            return normalizeTime(event, alignment);
        };

    auto erased = eraseValueProjection<EventWithId, float>(typed_fn);

    SECTION("Execute erased projection") {
        EventWithId event{TimeFrameIndex{150}, EntityId{1}};

        // Pass element by value in std::any (not via std::cref)
        std::any result = erased(std::any{event});
        float value = std::any_cast<float>(result);

        REQUIRE_THAT(value, WithinAbs(50.0f, 0.001f));
    }
}

TEST_CASE("eraseValueProjectionFactory - factory type erasure", "[ValueProjection][TypeErasure]") {
    ValueProjectionFactory<EventWithId, float> typed_factory = 
        [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
            TimeFrameIndex alignment = ctx.alignment_time;
            return [alignment](EventWithId const& event) {
                return normalizeTime(event, alignment);
            };
        };

    auto erased_factory = eraseValueProjectionFactory<EventWithId, float>(typed_factory);

    SECTION("Create and execute erased projection") {
        TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
        auto erased_fn = erased_factory(ctx);

        EventWithId event{TimeFrameIndex{200}, EntityId{2}};
        // Pass element by value in std::any
        std::any result = erased_fn(std::any{event});
        float value = std::any_cast<float>(result);

        REQUIRE_THAT(value, WithinAbs(100.0f, 0.001f));
    }
}

TEST_CASE("recoverValueProjection - recover typed from erased", "[ValueProjection][TypeErasure]") {
    TimeFrameIndex alignment{150};

    // Create typed, erase, then recover
    ValueProjectionFn<EventWithId, float> original = 
        [alignment](EventWithId const& event) {
            return normalizeTime(event, alignment);
        };

    auto erased = eraseValueProjection<EventWithId, float>(original);
    auto recovered = recoverValueProjection<EventWithId, float>(erased);

    SECTION("Recovered function works correctly") {
        EventWithId event{TimeFrameIndex{200}, EntityId{1}};
        float result = recovered(event);

        REQUIRE_THAT(result, WithinAbs(50.0f, 0.001f));  // 200 - 150
    }
}

TEST_CASE("recoverValueProjectionFactory - recover typed factory", "[ValueProjection][TypeErasure]") {
    ValueProjectionFactory<EventWithId, float> original_factory = 
        [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
            TimeFrameIndex alignment = ctx.alignment_time;
            return [alignment](EventWithId const& event) {
                return normalizeTime(event, alignment);
            };
        };

    auto erased_factory = eraseValueProjectionFactory<EventWithId, float>(original_factory);
    auto recovered_factory = recoverValueProjectionFactory<EventWithId, float>(erased_factory);

    SECTION("Recovered factory creates working projections") {
        TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
        auto projection = recovered_factory(ctx);

        EventWithId event{TimeFrameIndex{175}, EntityId{1}};
        float result = projection(event);

        REQUIRE_THAT(result, WithinAbs(75.0f, 0.001f));  // 175 - 100
    }
}

// ============================================================================
// Concept Tests
// ============================================================================

TEST_CASE("ValueProjection concept", "[ValueProjection][Concepts]") {
    SECTION("Lambda satisfies concept") {
        auto lambda = [](EventWithId const& e) { 
            return static_cast<float>(e.time().getValue()); 
        };

        STATIC_CHECK(ValueProjection<decltype(lambda), EventWithId, float>);
    }

    SECTION("std::function satisfies concept") {
        ValueProjectionFn<EventWithId, float> fn = [](EventWithId const& e) {
            return static_cast<float>(e.time().getValue());
        };

        STATIC_CHECK(ValueProjection<decltype(fn), EventWithId, float>);
    }

    SECTION("Wrong output type fails concept") {
        auto wrong_output = [](EventWithId const& e) { 
            return static_cast<int>(e.time().getValue()); 
        };

        // int is convertible to float, so this should still satisfy
        STATIC_CHECK(ValueProjection<decltype(wrong_output), EventWithId, float>);

        // But not convertible to std::string
        STATIC_CHECK_FALSE(ValueProjection<decltype(wrong_output), EventWithId, std::string>);
    }
}

TEST_CASE("ValueProjectionFactoryConcept", "[ValueProjection][Concepts]") {
    SECTION("Valid factory satisfies concept") {
        auto factory = [](TrialContext const&) -> ValueProjectionFn<EventWithId, float> {
            return [](EventWithId const& e) { 
                return static_cast<float>(e.time().getValue()); 
            };
        };

        STATIC_CHECK(ValueProjectionFactoryConcept<decltype(factory), EventWithId, float>);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Value projection workflow - simulated trial analysis", "[ValueProjection][Integration]") {
    // Simulate multiple trials with different alignments
    struct Trial {
        std::vector<EventWithId> events;
        TimeFrameIndex alignment;
    };

    std::vector<Trial> trials = {
        {{{TimeFrameIndex{100}, EntityId{1}}, {TimeFrameIndex{120}, EntityId{2}}}, TimeFrameIndex{100}},
        {{{TimeFrameIndex{200}, EntityId{1}}, {TimeFrameIndex{250}, EntityId{2}}}, TimeFrameIndex{200}},
        {{{TimeFrameIndex{300}, EntityId{1}}, {TimeFrameIndex{380}, EntityId{2}}}, TimeFrameIndex{300}}
    };

    // Create factory
    ValueProjectionFactory<EventWithId, float> factory = 
        [](TrialContext const& ctx) -> ValueProjectionFn<EventWithId, float> {
            TimeFrameIndex alignment = ctx.alignment_time;
            return [alignment](EventWithId const& event) {
                return normalizeTime(event, alignment);
            };
        };

    SECTION("Process all trials with context-aware projection") {
        std::vector<std::vector<float>> all_normalized;

        for (auto const& trial : trials) {
            TrialContext ctx{.alignment_time = trial.alignment};
            auto projection = factory(ctx);

            std::vector<float> normalized;
            for (auto const& event : trial.events) {
                normalized.push_back(projection(event));
            }
            all_normalized.push_back(normalized);
        }

        REQUIRE(all_normalized.size() == 3);

        // Trial 0: events at 100, 120 with alignment 100
        REQUIRE_THAT(all_normalized[0][0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(all_normalized[0][1], WithinAbs(20.0f, 0.001f));

        // Trial 1: events at 200, 250 with alignment 200
        REQUIRE_THAT(all_normalized[1][0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(all_normalized[1][1], WithinAbs(50.0f, 0.001f));

        // Trial 2: events at 300, 380 with alignment 300
        REQUIRE_THAT(all_normalized[2][0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(all_normalized[2][1], WithinAbs(80.0f, 0.001f));
    }

    SECTION("Use makeValueView for lazy trial processing") {
        std::vector<float> first_event_times;

        for (auto const& trial : trials) {
            TrialContext ctx{.alignment_time = trial.alignment};
            auto projection = factory(ctx);

            auto value_view = makeValueView(trial.events, projection);
            first_event_times.push_back(*value_view.begin());
        }

        // All first events should be at t=0 (aligned)
        for (float t : first_event_times) {
            REQUIRE_THAT(t, WithinAbs(0.0f, 0.001f));
        }
    }
}
