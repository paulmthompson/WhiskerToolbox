/**
 * @file PlotZoomProfile.test.cpp
 * @brief Unit tests for optional plot wheel-zoom profiling helpers
 */

#include "Plots/Common/PlotZoomProfile/PlotZoomProfile.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PlotZoomProfile disabled when env var unset", "[PlotZoomProfile]") {
    REQUIRE_FALSE(Neuralyzer::Plots::PlotZoomProfile::enabled());
}

TEST_CASE("PlotZoomProfile scopes are no-ops when disabled", "[PlotZoomProfile]") {
    {
        Neuralyzer::Plots::PlotZoomProfileWheelScope const wheel_scope;
        Neuralyzer::Plots::PlotZoomProfilePaintGLScope paint_scope;
        paint_scope.setRebuiltScene(true);
        Neuralyzer::Plots::PlotZoomProfileAxisPaintScope const axis_scope{"horizontal"};
    }
    REQUIRE(Neuralyzer::Plots::PlotZoomProfile::instance().pendingGeneration() == 0);
}
