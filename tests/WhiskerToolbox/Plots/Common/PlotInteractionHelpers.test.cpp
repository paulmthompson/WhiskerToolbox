#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// =============================================================================
// Test view state types that mirror the real widget view states
// =============================================================================

namespace {

/// Mimics EventPlotViewState / LinePlotViewState / HeatmapViewState
struct TestViewState {
    double x_min = -500.0;
    double x_max = 500.0;
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double x_pan = 0.0;
    double y_pan = 0.0;
};

/// Mimics ScatterPlotViewState (no x_min/x_max)
struct TestMinimalViewState {
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double x_pan = 0.0;
    double y_pan = 0.0;
};

/// Mimics the State objects (EventPlotState, LinePlotState, etc.)
struct TestState {
    float last_pan_x = 0.0f;
    float last_pan_y = 0.0f;
    double last_x_zoom = 1.0;
    double last_y_zoom = 1.0;

    void setPan(float x, float y)
    {
        last_pan_x = x;
        last_pan_y = y;
    }
    void setXZoom(double z) { last_x_zoom = z; }
    void setYZoom(double z) { last_y_zoom = z; }
};

} // namespace

// =============================================================================
// Concept checks (compile-time)
// =============================================================================

static_assert(WhiskerToolbox::Plots::ViewStateLike<TestViewState>);
static_assert(WhiskerToolbox::Plots::ViewStateLike<TestMinimalViewState>);
static_assert(WhiskerToolbox::Plots::ZoomPanSettable<TestState>);

// =============================================================================
// screenToWorld tests
// =============================================================================

TEST_CASE("screenToWorld center of widget returns world origin for symmetric projection",
          "[PlotInteractionHelpers]")
{
    // Symmetric ortho: [-100, 100] x [-50, 50]
    glm::mat4 proj = glm::ortho(-100.0f, 100.0f, -50.0f, 50.0f, -1.0f, 1.0f);
    int const width = 800;
    int const height = 600;

    QPoint center(width / 2, height / 2);
    QPointF world = WhiskerToolbox::Plots::screenToWorld(proj, width, height, center);

    REQUIRE_THAT(world.x(), Catch::Matchers::WithinAbs(0.0, 0.5));
    REQUIRE_THAT(world.y(), Catch::Matchers::WithinAbs(0.0, 0.5));
}

TEST_CASE("screenToWorld top-left corner maps to (left, top)",
          "[PlotInteractionHelpers]")
{
    glm::mat4 proj = glm::ortho(-100.0f, 100.0f, -50.0f, 50.0f, -1.0f, 1.0f);
    int const width = 800;
    int const height = 600;

    QPoint top_left(0, 0);
    QPointF world = WhiskerToolbox::Plots::screenToWorld(proj, width, height, top_left);

    REQUIRE_THAT(world.x(), Catch::Matchers::WithinAbs(-100.0, 1.0));
    REQUIRE_THAT(world.y(), Catch::Matchers::WithinAbs(50.0, 1.0));
}

// =============================================================================
// worldToScreen tests
// =============================================================================

TEST_CASE("worldToScreen origin maps to center of widget",
          "[PlotInteractionHelpers]")
{
    glm::mat4 proj = glm::ortho(-100.0f, 100.0f, -50.0f, 50.0f, -1.0f, 1.0f);
    int const width = 800;
    int const height = 600;

    QPoint screen = WhiskerToolbox::Plots::worldToScreen(proj, width, height, 0.0f, 0.0f);

    REQUIRE(screen.x() == width / 2);
    REQUIRE(screen.y() == height / 2);
}

// =============================================================================
// computeOrthoProjection tests
// =============================================================================

TEST_CASE("computeOrthoProjection with no zoom or pan matches manual ortho",
          "[PlotInteractionHelpers]")
{
    TestViewState vs;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;
    vs.x_pan = 0.0;
    vs.y_pan = 0.0;

    float const x_range = 1000.0f;  // -500 to 500
    float const x_center = 0.0f;
    float const y_range = 2.0f;     // -1 to 1
    float const y_center = 0.0f;

    glm::mat4 result = WhiskerToolbox::Plots::computeOrthoProjection(
        vs, x_range, x_center, y_range, y_center);
    glm::mat4 expected = glm::ortho(-500.0f, 500.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            REQUIRE_THAT(static_cast<double>(result[c][r]),
                         Catch::Matchers::WithinAbs(static_cast<double>(expected[c][r]), 1e-5));
        }
    }
}

TEST_CASE("computeOrthoProjection with 2x zoom halves visible range",
          "[PlotInteractionHelpers]")
{
    TestViewState vs;
    vs.x_zoom = 2.0;
    vs.y_zoom = 2.0;
    vs.x_pan = 0.0;
    vs.y_pan = 0.0;

    float const x_range = 1000.0f;
    float const x_center = 0.0f;
    float const y_range = 2.0f;
    float const y_center = 0.0f;

    glm::mat4 result = WhiskerToolbox::Plots::computeOrthoProjection(
        vs, x_range, x_center, y_range, y_center);
    // 2x zoom: visible range = 1000/2 = 500, so [-250, 250] x [-0.5, 0.5]
    glm::mat4 expected = glm::ortho(-250.0f, 250.0f, -0.5f, 0.5f, -1.0f, 1.0f);

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            REQUIRE_THAT(static_cast<double>(result[c][r]),
                         Catch::Matchers::WithinAbs(static_cast<double>(expected[c][r]), 1e-5));
        }
    }
}

TEST_CASE("computeOrthoProjection works with MinimalViewState (no x_min/x_max)",
          "[PlotInteractionHelpers]")
{
    TestMinimalViewState vs;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;

    // Caller provides ranges directly
    glm::mat4 result = WhiskerToolbox::Plots::computeOrthoProjection(
        vs, 200.0f, 50.0f, 100.0f, 50.0f);
    glm::mat4 expected = glm::ortho(-50.0f, 150.0f, 0.0f, 100.0f, -1.0f, 1.0f);

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            REQUIRE_THAT(static_cast<double>(result[c][r]),
                         Catch::Matchers::WithinAbs(static_cast<double>(expected[c][r]), 1e-5));
        }
    }
}

// =============================================================================
// handlePanning tests
// =============================================================================

TEST_CASE("handlePanning converts pixel drag to world-space pan",
          "[PlotInteractionHelpers]")
{
    TestState state;
    TestViewState vs;
    vs.x_pan = 0.0;
    vs.y_pan = 0.0;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;

    // Widget is 1000px wide, data range is 1000 units => 1 unit/pixel
    // Drag 10 pixels right => pan left by 10 units
    WhiskerToolbox::Plots::handlePanning(
        state, vs,
        10, 0,       // delta_x, delta_y
        1000.0f,     // x_range
        2.0f,        // y_range
        1000, 500);  // widget_width, widget_height

    REQUIRE_THAT(static_cast<double>(state.last_pan_x),
                 Catch::Matchers::WithinAbs(-10.0, 0.01));
    REQUIRE_THAT(static_cast<double>(state.last_pan_y),
                 Catch::Matchers::WithinAbs(0.0, 0.01));
}

TEST_CASE("handlePanning respects zoom level",
          "[PlotInteractionHelpers]")
{
    TestState state;
    TestViewState vs;
    vs.x_pan = 0.0;
    vs.y_pan = 0.0;
    vs.x_zoom = 2.0;  // 2x zoom => world_per_pixel halved
    vs.y_zoom = 1.0;

    // 1000px wide, 1000 unit range, but 2x zoom => 0.5 units/pixel
    // Drag 10px => pan 5 units
    WhiskerToolbox::Plots::handlePanning(
        state, vs,
        10, 0,
        1000.0f, 2.0f,
        1000, 500);

    REQUIRE_THAT(static_cast<double>(state.last_pan_x),
                 Catch::Matchers::WithinAbs(-5.0, 0.01));
}

// =============================================================================
// handleZoom tests
// =============================================================================

TEST_CASE("handleZoom x-only by default",
          "[PlotInteractionHelpers]")
{
    TestState state;
    TestViewState vs;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;

    WhiskerToolbox::Plots::handleZoom(state, vs, 1.0f, false, false);

    REQUIRE_THAT(state.last_x_zoom, Catch::Matchers::WithinAbs(1.1, 0.01));
    REQUIRE_THAT(state.last_y_zoom, Catch::Matchers::WithinAbs(1.0, 0.01));
}

TEST_CASE("handleZoom y-only when y_only is true",
          "[PlotInteractionHelpers]")
{
    TestState state;
    TestViewState vs;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;

    WhiskerToolbox::Plots::handleZoom(state, vs, 1.0f, true, false);

    REQUIRE_THAT(state.last_x_zoom, Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(state.last_y_zoom, Catch::Matchers::WithinAbs(1.1, 0.01));
}

TEST_CASE("handleZoom both axes when both_axes is true",
          "[PlotInteractionHelpers]")
{
    TestState state;
    TestViewState vs;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;

    WhiskerToolbox::Plots::handleZoom(state, vs, 1.0f, false, true);

    REQUIRE_THAT(state.last_x_zoom, Catch::Matchers::WithinAbs(1.1, 0.01));
    REQUIRE_THAT(state.last_y_zoom, Catch::Matchers::WithinAbs(1.1, 0.01));
}

TEST_CASE("handleZoom negative delta zooms out",
          "[PlotInteractionHelpers]")
{
    TestState state;
    TestViewState vs;
    vs.x_zoom = 1.0;
    vs.y_zoom = 1.0;

    WhiskerToolbox::Plots::handleZoom(state, vs, -1.0f, false, false);

    REQUIRE(state.last_x_zoom < 1.0);
}
