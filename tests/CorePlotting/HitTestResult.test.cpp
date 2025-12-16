#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Interaction/HitTestResult.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

TEST_CASE("HitTestResult default construction", "[CorePlotting][HitTestResult]") {
    HitTestResult result;
    
    REQUIRE(result.hit_type == HitType::None);
    REQUIRE_FALSE(result.hasHit());
    REQUIRE_FALSE(result.hasEntityId());
    REQUIRE_FALSE(result.isIntervalHit());
    REQUIRE_FALSE(result.isIntervalEdge());
    REQUIRE_FALSE(result.isDiscrete());
    REQUIRE(result.series_key.empty());
}

TEST_CASE("HitTestResult factory methods", "[CorePlotting][HitTestResult]") {
    SECTION("noHit()") {
        auto result = HitTestResult::noHit();
        REQUIRE_FALSE(result.hasHit());
    }
    
    SECTION("eventHit()") {
        auto result = HitTestResult::eventHit("events", EntityId{42}, 1.5f, 100.0f, 0.5f);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.series_key == "events");
        REQUIRE(result.hasEntityId());
        REQUIRE(result.entity_id.value() == EntityId{42});
        REQUIRE_THAT(result.distance, WithinAbs(1.5f, 0.001f));
        REQUIRE_THAT(result.world_x, WithinAbs(100.0f, 0.001f));
        REQUIRE_THAT(result.world_y, WithinAbs(0.5f, 0.001f));
        REQUIRE(result.isDiscrete());
        REQUIRE_FALSE(result.isIntervalHit());
    }
    
    SECTION("intervalBodyHit()") {
        auto result = HitTestResult::intervalBodyHit("intervals", EntityId{100}, 50, 150, 0.0f);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::IntervalBody);
        REQUIRE(result.isIntervalHit());
        REQUIRE_FALSE(result.isIntervalEdge());
        REQUIRE(result.interval_start.value() == 50);
        REQUIRE(result.interval_end.value() == 150);
        REQUIRE(result.isDiscrete());
    }
    
    SECTION("intervalEdgeHit() - left edge") {
        auto result = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 2.0f);
        
        REQUIRE(result.hit_type == HitType::IntervalEdgeLeft);
        REQUIRE(result.isIntervalHit());
        REQUIRE(result.isIntervalEdge());
        REQUIRE_THAT(result.world_x, WithinAbs(50.0f, 0.001f));
    }
    
    SECTION("intervalEdgeHit() - right edge") {
        auto result = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, false, 50, 150, 150.0f, 2.0f);
        
        REQUIRE(result.hit_type == HitType::IntervalEdgeRight);
        REQUIRE(result.isIntervalEdge());
    }
    
    SECTION("analogSeriesHit()") {
        auto result = HitTestResult::analogSeriesHit("analog1", 100.0f, 0.5f);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::AnalogSeries);
        REQUIRE(result.series_key == "analog1");
        REQUIRE_FALSE(result.hasEntityId());
        REQUIRE_FALSE(result.isDiscrete());
    }
    
    SECTION("pointHit()") {
        auto result = HitTestResult::pointHit("points", EntityId{99}, 25.0f, 0.3f, 0.5f);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::Point);
        REQUIRE(result.hasEntityId());
        REQUIRE(result.isDiscrete());
    }
}

TEST_CASE("HitTestResult comparison", "[CorePlotting][HitTestResult]") {
    auto near = HitTestResult::eventHit("s1", EntityId{1}, 1.0f, 0.0f, 0.0f);
    auto far = HitTestResult::eventHit("s2", EntityId{2}, 5.0f, 0.0f, 0.0f);
    
    REQUIRE(near.isCloserThan(far));
    REQUIRE_FALSE(far.isCloserThan(near));
}
