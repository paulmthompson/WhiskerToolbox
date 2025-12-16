#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Interaction/IntervalDragController.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

TEST_CASE("IntervalDragController initial state", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    
    REQUIRE_FALSE(controller.isActive());
    REQUIRE_FALSE(controller.getState().is_active);
}

TEST_CASE("IntervalDragController start drag", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    
    SECTION("Start from interval edge hit succeeds") {
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
        
        bool started = controller.startDrag(hit);
        
        REQUIRE(started);
        REQUIRE(controller.isActive());
        REQUIRE(controller.getState().series_key == "intervals");
        REQUIRE(controller.getState().entity_id == EntityId{100});
        REQUIRE(controller.getState().edge == DraggedEdge::Left);
        REQUIRE(controller.getState().original_start == 50);
        REQUIRE(controller.getState().original_end == 150);
        REQUIRE(controller.getState().current_start == 50);
        REQUIRE(controller.getState().current_end == 150);
    }
    
    SECTION("Start from interval body hit fails") {
        auto hit = HitTestResult::intervalBodyHit(
            "intervals", EntityId{100}, 50, 150, 0.0f);
        
        bool started = controller.startDrag(hit);
        
        REQUIRE_FALSE(started);
        REQUIRE_FALSE(controller.isActive());
    }
    
    SECTION("Start from event hit fails") {
        auto hit = HitTestResult::eventHit("events", EntityId{1}, 0.0f, 0.0f, 0.0f);
        
        bool started = controller.startDrag(hit);
        
        REQUIRE_FALSE(started);
        REQUIRE_FALSE(controller.isActive());
    }
    
    SECTION("Start from no hit fails") {
        auto hit = HitTestResult::noHit();
        
        bool started = controller.startDrag(hit);
        
        REQUIRE_FALSE(started);
    }
}

TEST_CASE("IntervalDragController update drag - left edge", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    auto hit = HitTestResult::intervalEdgeHit(
        "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
    controller.startDrag(hit);
    
    SECTION("Move left edge earlier") {
        bool changed = controller.updateDrag(30.0f);
        
        REQUIRE(changed);
        REQUIRE(controller.getState().current_start == 30);
        REQUIRE(controller.getState().current_end == 150); // unchanged
    }
    
    SECTION("Move left edge later") {
        bool changed = controller.updateDrag(80.0f);
        
        REQUIRE(changed);
        REQUIRE(controller.getState().current_start == 80);
        REQUIRE(controller.getState().current_end == 150);
    }
    
    SECTION("Move left edge to same position returns false") {
        controller.updateDrag(50.0f);
        bool changed = controller.updateDrag(50.0f);
        
        REQUIRE_FALSE(changed);
    }
}

TEST_CASE("IntervalDragController update drag - right edge", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    auto hit = HitTestResult::intervalEdgeHit(
        "intervals", EntityId{100}, false, 50, 150, 150.0f, 1.0f);
    controller.startDrag(hit);
    
    SECTION("Move right edge later") {
        bool changed = controller.updateDrag(200.0f);
        
        REQUIRE(changed);
        REQUIRE(controller.getState().current_start == 50); // unchanged
        REQUIRE(controller.getState().current_end == 200);
    }
    
    SECTION("Move right edge earlier") {
        bool changed = controller.updateDrag(100.0f);
        
        REQUIRE(changed);
        REQUIRE(controller.getState().current_end == 100);
    }
}

TEST_CASE("IntervalDragController minimum width constraint", "[CorePlotting][IntervalDragController]") {
    IntervalDragConfig config;
    config.min_width = 10;
    IntervalDragController controller(config);
    
    auto hit = HitTestResult::intervalEdgeHit(
        "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
    controller.startDrag(hit);
    
    SECTION("Cannot drag left edge past minimum width") {
        controller.updateDrag(145.0f); // Would result in width of 5
        
        // Should be clamped to maintain min_width of 10
        REQUIRE(controller.getState().current_start == 140);
        REQUIRE(controller.getState().current_end == 150);
    }
}

TEST_CASE("IntervalDragController edge swap behavior", "[CorePlotting][IntervalDragController]") {
    SECTION("Without edge swap allowed - clamps at boundary") {
        IntervalDragConfig config;
        config.min_width = 1;
        config.allow_edge_swap = false;
        IntervalDragController controller(config);
        
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
        controller.startDrag(hit);
        
        // Try to drag left edge past right edge
        controller.updateDrag(200.0f);
        
        // Should clamp to min_width from end
        REQUIRE(controller.getState().current_start == 149);
        REQUIRE(controller.getState().edge == DraggedEdge::Left);
    }
    
    SECTION("With edge swap allowed - swaps edges") {
        IntervalDragConfig config;
        config.min_width = 1;
        config.allow_edge_swap = true;
        IntervalDragController controller(config);
        
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
        controller.startDrag(hit);
        
        // Try to drag left edge past right edge
        controller.updateDrag(200.0f);
        
        // Edge should swap, and we're now dragging right edge
        REQUIRE(controller.getState().edge == DraggedEdge::Right);
        REQUIRE(controller.getState().current_start == 150);
        REQUIRE(controller.getState().current_end == 200);
    }
}

TEST_CASE("IntervalDragController time bounds constraint", "[CorePlotting][IntervalDragController]") {
    IntervalDragConfig config;
    config.min_time = 0;
    config.max_time = 1000;
    config.min_width = 1;
    IntervalDragController controller(config);
    
    auto hit = HitTestResult::intervalEdgeHit(
        "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
    controller.startDrag(hit);
    
    SECTION("Cannot drag past max_time") {
        // Change to right edge
        auto hit_right = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, false, 50, 150, 150.0f, 1.0f);
        IntervalDragController controller2(config);
        controller2.startDrag(hit_right);
        
        controller2.updateDrag(1100.0f);
        
        REQUIRE(controller2.getState().current_end == 1000);
    }
    
    SECTION("Cannot drag past min_time") {
        controller.updateDrag(-50.0f);
        
        REQUIRE(controller.getState().current_start == 0);
    }
}

TEST_CASE("IntervalDragController finish drag", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    auto hit = HitTestResult::intervalEdgeHit(
        "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
    controller.startDrag(hit);
    controller.updateDrag(30.0f);
    
    SECTION("Finish returns final state and resets") {
        auto final_state = controller.finishDrag();
        
        REQUIRE(final_state.current_start == 30);
        REQUIRE(final_state.current_end == 150);
        REQUIRE(final_state.original_start == 50);
        REQUIRE(final_state.hasChanged());
        
        REQUIRE_FALSE(controller.isActive());
    }
}

TEST_CASE("IntervalDragController cancel drag", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    auto hit = HitTestResult::intervalEdgeHit(
        "intervals", EntityId{100}, true, 50, 150, 50.0f, 1.0f);
    controller.startDrag(hit);
    controller.updateDrag(30.0f);
    
    SECTION("Cancel returns state with original bounds for restoration") {
        auto cancelled_state = controller.cancelDrag();
        
        // current_start/end should be restored to original values
        REQUIRE(cancelled_state.current_start == 50);
        REQUIRE(cancelled_state.current_end == 150);
        REQUIRE(cancelled_state.original_start == 50);
        REQUIRE_FALSE(cancelled_state.hasChanged());
        
        REQUIRE_FALSE(controller.isActive());
    }
}

TEST_CASE("IntervalDragController update when inactive", "[CorePlotting][IntervalDragController]") {
    IntervalDragController controller;
    
    bool changed = controller.updateDrag(100.0f);
    
    REQUIRE_FALSE(changed);
    REQUIRE_FALSE(controller.isActive());
}

TEST_CASE("IntervalDragState hasChanged", "[CorePlotting][IntervalDragController]") {
    SECTION("No change") {
        IntervalDragState state;
        state.original_start = 50;
        state.original_end = 150;
        state.current_start = 50;
        state.current_end = 150;
        
        REQUIRE_FALSE(state.hasChanged());
    }
    
    SECTION("Start changed") {
        IntervalDragState state;
        state.original_start = 50;
        state.original_end = 150;
        state.current_start = 30;
        state.current_end = 150;
        
        REQUIRE(state.hasChanged());
    }
    
    SECTION("End changed") {
        IntervalDragState state;
        state.original_start = 50;
        state.original_end = 150;
        state.current_start = 50;
        state.current_end = 200;
        
        REQUIRE(state.hasChanged());
    }
}
