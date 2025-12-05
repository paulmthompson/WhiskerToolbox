#include "AnalogEventThreshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "transforms/v2/core/ComputeContext.hpp"
#include "transforms/v2/core/ElementRegistry.hpp" //registerContainerTransform
#include "transforms/v2/core/ParameterIO.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "fixtures/AnalogEventThresholdTestFixture.hpp"

#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Registration (would normally be in RegisteredTransforms.hpp)
// ============================================================================

namespace {
    struct RegisterAnalogEventThreshold {
        RegisterAnalogEventThreshold() {
            auto& registry = ElementRegistry::instance();
            registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
                "AnalogEventThreshold",
                analogEventThreshold,
                ContainerTransformMetadata{
                    .description = "Detect threshold crossing events with lockout period",
                    .category = "Signal Processing",
                    .supports_cancellation = true
                });
        }
    };
    
    static RegisterAnalogEventThreshold register_analog_event_threshold;
}

// ============================================================================
// Tests: Algorithm Correctness (using v1 fixture)
// ============================================================================

TEST_CASE_METHOD(AnalogEventThresholdTestFixture, 
                 "V2 Container Transform: Analog Event Threshold - Happy Path", 
                 "[transforms][v2][container][analog_event_threshold]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<DigitalEventSeries> result_events;
    AnalogEventThresholdParams params;
    std::vector<TimeFrameIndex> expected_events;
    ComputeContext ctx;
    
    // Progress tracking
    int progress_val = -1;
    int call_count = 0;
    ctx.progress = [&](int p) {
        progress_val = p;
        call_count++;
    };
    
    SECTION("Positive threshold, no lockout") {
        auto ats = m_test_signals["positive_no_lockout"];
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
        
        // Check progress was reported
        REQUIRE(progress_val == 100);
        REQUIRE(call_count > 0);
    }
    
    SECTION("Positive threshold, with lockout") {
        auto ats = m_test_signals["positive_with_lockout"];
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 150.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Negative threshold, no lockout") {
        auto ats = m_test_signals["negative_no_lockout"];
        params.threshold_value = -1.0f;
        params.direction = "negative";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Negative threshold, with lockout") {
        auto ats = m_test_signals["negative_with_lockout"];
        params.threshold_value = -1.0f;
        params.direction = "negative";
        params.lockout_time = 150.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Absolute threshold, no lockout") {
        auto ats = m_test_signals["absolute_no_lockout"];
        params.threshold_value = 1.0f;
        params.direction = "absolute";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("Absolute threshold, with lockout") {
        auto ats = m_test_signals["absolute_with_lockout"];
        params.threshold_value = 1.0f;
        params.direction = "absolute";
        params.lockout_time = 150.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(200), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
    
    SECTION("No events expected (threshold too high)") {
        auto ats = m_test_signals["no_events_high_threshold"];
        params.threshold_value = 10.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        REQUIRE(result_events->getEventSeries().empty());
    }
    
    SECTION("All events expected (threshold very low, no lockout)") {
        auto ats = m_test_signals["all_events_low_threshold"];
        params.threshold_value = 0.1f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        expected_events = {TimeFrameIndex(100), TimeFrameIndex(200), TimeFrameIndex(300), 
                          TimeFrameIndex(400), TimeFrameIndex(500)};
        REQUIRE_THAT(result_events->getEventSeries(), Catch::Matchers::Equals(expected_events));
    }
}

TEST_CASE_METHOD(AnalogEventThresholdTestFixture,
                 "V2 Container Transform: Analog Event Threshold - Edge Cases",
                 "[transforms][v2][container][analog_event_threshold]") {
    
    auto& registry = ElementRegistry::instance();
    std::shared_ptr<DigitalEventSeries> result_events;
    AnalogEventThresholdParams params;
    ComputeContext ctx;
    
    SECTION("Empty analog time series") {
        auto ats = m_test_signals["empty_signal"];
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        REQUIRE(result_events != nullptr);
        REQUIRE(result_events->getEventSeries().empty());
    }
    
    SECTION("Lockout time larger than series duration") {
        auto ats = m_test_signals["lockout_larger_than_duration"];
        params.threshold_value = 1.0f;
        params.direction = "positive";
        params.lockout_time = 1000.0f;  // Much larger than series
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        // Should only get first event
        REQUIRE(result_events->getEventSeries().size() == 1);
    }
    
    SECTION("Cancellation support") {
        auto ats = m_test_signals["all_events_low_threshold"];
        params.threshold_value = 0.1f;
        params.direction = "positive";
        params.lockout_time = 0.0f;
        
        // Set cancellation flag
        bool should_cancel = true;
        ctx.is_cancelled = [&]() { return should_cancel; };
        
        result_events = registry.executeContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold", *ats, params, ctx);
        
        // Should return empty due to cancellation
        REQUIRE(result_events->getEventSeries().empty());
    }
}

// ============================================================================
// Tests: JSON Parameter Loading
// ============================================================================

TEST_CASE("V2 Container Transform: AnalogEventThresholdParams - JSON Loading", 
          "[transforms][v2][params][json]") {
    
    SECTION("Load valid JSON with all fields") {
        std::string json = R"({
            "threshold_value": 2.5,
            "direction": "negative",
            "lockout_time": 100.0
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getThresholdValue() == 2.5f);
        REQUIRE(params.getDirection() == "negative");
        REQUIRE(params.getLockoutTime() == 100.0f);
    }
    
    SECTION("Load empty JSON (uses defaults)") {
        std::string json = "{}";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getThresholdValue() == 1.0f);
        REQUIRE(params.getDirection() == "positive");
        REQUIRE(params.getLockoutTime() == 0.0f);
    }
    
    SECTION("Load with only some fields") {
        std::string json = R"({
            "threshold_value": 3.0,
            "direction": "absolute"
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        REQUIRE(result);
        auto params = result.value();
        
        REQUIRE(params.getThresholdValue() == 3.0f);
        REQUIRE(params.getDirection() == "absolute");
        REQUIRE(params.getLockoutTime() == 0.0f);  // Default
    }
    
    SECTION("Reject negative lockout time") {
        std::string json = R"({
            "lockout_time": -10.0
        })";
        
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        
        // Should fail validation
        REQUIRE_FALSE(result);
    }
    
    SECTION("JSON round-trip preserves values") {
        AnalogEventThresholdParams original;
        original.threshold_value = 1.5f;
        original.direction = "positive";
        original.lockout_time = rfl::Validator<float, rfl::Minimum<0.0f>>(50.0f);
        
        // Serialize
        std::string json = saveParametersToJson(original);
        
        // Deserialize
        auto result = loadParametersFromJson<AnalogEventThresholdParams>(json);
        REQUIRE(result);
        auto recovered = result.value();
        
        // Verify values match
        REQUIRE(recovered.getThresholdValue() == 1.5f);
        REQUIRE(recovered.getDirection() == "positive");
        REQUIRE(recovered.getLockoutTime() == 50.0f);
    }
}

// ============================================================================
// Tests: Registry Integration
// ============================================================================

TEST_CASE("V2 Container Transform: Registry Integration", 
          "[transforms][v2][registry][container]") {
    
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered as container transform") {
        REQUIRE(registry.isContainerTransform("AnalogEventThreshold"));
        REQUIRE_FALSE(registry.hasElementTransform("AnalogEventThreshold"));  // Not an element transform
    }
    
    SECTION("Can retrieve container metadata") {
        auto const* metadata = registry.getContainerMetadata("AnalogEventThreshold");
        REQUIRE(metadata != nullptr);
        REQUIRE(metadata->name == "AnalogEventThreshold");
        REQUIRE(metadata->category == "Signal Processing");
        REQUIRE(metadata->supports_cancellation == true);
    }
    
    SECTION("Can find transform by input type") {
        auto transforms = registry.getContainerTransformsForInputType(typeid(AnalogTimeSeries));
        REQUIRE_FALSE(transforms.empty());
        REQUIRE(std::find(transforms.begin(), transforms.end(), "AnalogEventThreshold") != transforms.end());
    }
}
