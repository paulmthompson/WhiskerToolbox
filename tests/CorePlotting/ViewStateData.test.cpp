#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CoordinateTransform/ViewState.hpp"
#include "CoordinateTransform/ViewStateData.hpp"

using Catch::Matchers::WithinAbs;

// =============================================================================
// ViewStateData defaults
// =============================================================================

TEST_CASE("ViewStateData has expected defaults", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;

    CHECK(data.x_min == -500.0);
    CHECK(data.x_max ==  500.0);
    CHECK(data.y_min ==    0.0);
    CHECK(data.y_max ==  100.0);

    CHECK(data.x_zoom == 1.0);
    CHECK(data.y_zoom == 1.0);
    CHECK(data.x_pan  == 0.0);
    CHECK(data.y_pan  == 0.0);
}

// =============================================================================
// toRuntimeViewState — identity-ish conversion at zoom=1, pan=0
// =============================================================================

TEST_CASE("toRuntimeViewState: default data → valid ViewState", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;
    auto vs = CorePlotting::toRuntimeViewState(data, 800, 600);

    CHECK(vs.data_bounds_valid);
    CHECK(vs.viewport_width == 800);
    CHECK(vs.viewport_height == 600);
    CHECK(vs.padding_factor == 1.0f);

    CHECK_THAT(vs.data_bounds.min_x, WithinAbs(-500.0f, 1e-3f));
    CHECK_THAT(vs.data_bounds.max_x, WithinAbs( 500.0f, 1e-3f));
    CHECK_THAT(vs.data_bounds.min_y, WithinAbs(   0.0f, 1e-3f));
    CHECK_THAT(vs.data_bounds.max_y, WithinAbs( 100.0f, 1e-3f));

    CHECK_THAT(vs.zoom_level_x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(vs.zoom_level_y, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(vs.pan_offset_x, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(vs.pan_offset_y, WithinAbs(0.0f, 1e-5f));
}

// =============================================================================
// toRuntimeViewState — zoom pass-through
// =============================================================================

TEST_CASE("toRuntimeViewState: zoom values are passed through", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;
    data.x_zoom = 2.5;
    data.y_zoom = 3.0;

    auto vs = CorePlotting::toRuntimeViewState(data, 400, 300);

    CHECK_THAT(vs.zoom_level_x, WithinAbs(2.5f, 1e-5f));
    CHECK_THAT(vs.zoom_level_y, WithinAbs(3.0f, 1e-5f));
}

// =============================================================================
// toRuntimeViewState — pan normalization
// =============================================================================

TEST_CASE("toRuntimeViewState: pan normalization from world to ratio", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;
    data.x_min = -100.0;
    data.x_max =  100.0;   // range = 200
    data.y_min =    0.0;
    data.y_max =  200.0;   // range = 200
    data.x_zoom = 2.0;     // visible range = 100
    data.y_zoom = 4.0;     // visible range = 50

    // Pan 25 world units right → normalized = 25 / (200/2) = 0.25
    data.x_pan = 25.0;
    // Pan 10 world units up   → normalized = 10 / (200/4) = 0.2
    data.y_pan = 10.0;

    auto vs = CorePlotting::toRuntimeViewState(data, 800, 600);

    CHECK_THAT(vs.pan_offset_x, WithinAbs(0.25f, 1e-5f));
    CHECK_THAT(vs.pan_offset_y, WithinAbs(0.2f, 1e-5f));
}

// =============================================================================
// toRuntimeViewState — zero range is safe
// =============================================================================

TEST_CASE("toRuntimeViewState: zero range does not produce NaN", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;
    data.x_min = 0.0;
    data.x_max = 0.0;
    data.y_min = 50.0;
    data.y_max = 50.0;
    data.x_pan = 10.0;
    data.y_pan = 5.0;

    auto vs = CorePlotting::toRuntimeViewState(data, 100, 100);

    CHECK_THAT(vs.pan_offset_x, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(vs.pan_offset_y, WithinAbs(0.0f, 1e-5f));
}

// =============================================================================
// toRuntimeViewState — custom padding
// =============================================================================

TEST_CASE("toRuntimeViewState: custom padding factor", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;
    auto vs = CorePlotting::toRuntimeViewState(data, 800, 600, 1.1f);
    CHECK_THAT(vs.padding_factor, WithinAbs(1.1f, 1e-5f));
}

// =============================================================================
// Round-trip: toRuntimeViewState → calculateVisibleWorldBounds
// =============================================================================

TEST_CASE("Round-trip: ViewStateData at zoom=1 pan=0 → visible bounds ≈ data bounds", "[ViewStateData]")
{
    CorePlotting::ViewStateData data;
    data.x_min = -200.0;
    data.x_max =  200.0;
    data.y_min =  -50.0;
    data.y_max =   50.0;

    // Use a square viewport and padding=1 to get exact bounds
    auto vs = CorePlotting::toRuntimeViewState(data, 400, 400, 1.0f);
    auto bounds = CorePlotting::calculateVisibleWorldBounds(vs);

    CHECK_THAT(bounds.min_x, WithinAbs(-200.0f, 1e-1f));
    CHECK_THAT(bounds.max_x, WithinAbs( 200.0f, 1e-1f));
    CHECK_THAT(bounds.min_y, WithinAbs( -50.0f, 1e-1f));
    CHECK_THAT(bounds.max_y, WithinAbs(  50.0f, 1e-1f));
}
