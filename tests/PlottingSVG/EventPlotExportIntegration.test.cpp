/**
 * @file EventPlotExportIntegration.test.cpp
 * @brief Integration tests for the EventPlot SVG export pipeline.
 *
 * These tests exercise the same code path as EventPlotOpenGLWidget::exportToSVG()
 * without requiring an OpenGL context:
 *   SceneBuilder → RenderableScene → SVGSceneRenderer → SVG string
 *
 * Known events are placed at specific world positions, transformed through a
 * real orthographic projection, and the resulting SVG is checked for correct
 * glyph coordinates.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "Entity/EntityId.hpp"
#include "PlottingSVG/SVGSceneRenderer.hpp"
#include "PlottingSVG/SVGUtils.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <regex>
#include <string>

using Catch::Matchers::ContainsSubstring;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Reproduce the projection that EventPlotOpenGLWidget::updateMatrices() builds.
glm::mat4 buildEventPlotProjection(CorePlotting::ViewStateData const & vs) {
    auto const x_range = static_cast<float>(vs.x_max - vs.x_min);
    float const x_center = static_cast<float>(vs.x_min + vs.x_max) / 2.0f;
    auto const y_range = static_cast<float>(vs.y_max - vs.y_min);
    float const y_center = static_cast<float>(vs.y_min + vs.y_max) / 2.0f;

    auto const x_zoom = static_cast<float>(vs.x_zoom);
    auto const y_zoom = static_cast<float>(vs.y_zoom);
    float const safe_x_zoom = (x_zoom > 0.0f) ? x_zoom : 1.0f;
    float const safe_y_zoom = (y_zoom > 0.0f) ? y_zoom : 1.0f;

    float const zoomed_x_range = x_range / safe_x_zoom;
    float const zoomed_y_range = y_range / safe_y_zoom;

    auto const pan_x = static_cast<float>(vs.x_pan);
    auto const pan_y = static_cast<float>(vs.y_pan);

    float const left = x_center - zoomed_x_range / 2.0f + pan_x;
    float const right = x_center + zoomed_x_range / 2.0f + pan_x;
    float const bottom = y_center - zoomed_y_range / 2.0f + pan_y;
    float const top = y_center + zoomed_y_range / 2.0f + pan_y;

    return glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
}

/// Build a RenderableScene mimicking EventPlotOpenGLWidget::rebuildScene().
///
/// Places known glyphs at explicit world positions using SceneBuilder, then
/// sets the view/projection matrices on the resulting scene (the fix for the
/// identity-matrix bug).
CorePlotting::RenderableScene buildTestScene(
        std::vector<std::pair<float, float>> const & glyph_world_positions,
        CorePlotting::ViewStateData const & view_state,
        CorePlotting::GlyphStyle const & style = {}) {

    CorePlotting::SceneBuilder builder;

    // Set bounds matching how EventPlotOpenGLWidget does it
    BoundingBox const bounds{
            static_cast<float>(view_state.x_min), -1.0f,
            static_cast<float>(view_state.x_max), 1.0f};
    builder.setBounds(bounds);

    // Create MappedElements at the given world positions
    std::vector<CorePlotting::MappedElement> elements;
    elements.reserve(glyph_world_positions.size());
    for (size_t i = 0; i < glyph_world_positions.size(); ++i) {
        auto const & [x, y] = glyph_world_positions[i];
        elements.emplace_back(x, y, EntityId{static_cast<uint64_t>(i + 1)});
    }

    builder.addGlyphs("test_events", std::move(elements), style);

    CorePlotting::RenderableScene scene = builder.build();

    // The fix: set the correct matrices (matches the EventPlotOpenGLWidget fix).
    scene.view_matrix = glm::mat4(1.0f);
    scene.projection_matrix = buildEventPlotProjection(view_state);
    return scene;
}

/// Extract all cx="..." values from SVG <circle> elements.
std::vector<float> extractCircleCx(std::string const & svg) {
    std::vector<float> values;
    std::regex const re(R"_re_(cx="([^"]+)")_re_");
    auto it = std::sregex_iterator(svg.begin(), svg.end(), re);
    auto const end = std::sregex_iterator();
    for (; it != end; ++it) {
        values.push_back(std::stof((*it)[1].str()));
    }
    return values;
}

/// Extract all cy="..." values from SVG <circle> elements.
std::vector<float> extractCircleCy(std::string const & svg) {
    std::vector<float> values;
    std::regex const re(R"_re_(cy="([^"]+)")_re_");
    auto it = std::sregex_iterator(svg.begin(), svg.end(), re);
    auto const end = std::sregex_iterator();
    for (; it != end; ++it) {
        values.push_back(std::stof((*it)[1].str()));
    }
    return values;
}

/// Extract (y1, y2) pairs from SVG <line> elements.
struct LineSVGEndpoints {
    float y1;
    float y2;
};
std::vector<LineSVGEndpoints> extractLineY(std::string const & svg) {
    std::vector<LineSVGEndpoints> values;
    std::regex const re(R"_re_(<line[^>]*y1="([^"]+)"[^>]*y2="([^"]+)")_re_");
    auto it = std::sregex_iterator(svg.begin(), svg.end(), re);
    auto const end = std::sregex_iterator();
    for (; it != end; ++it) {
        values.push_back({std::stof((*it)[1].str()), std::stof((*it)[2].str())});
    }
    return values;
}

/// Extract x1 values from SVG <line> elements.
std::vector<float> extractLineX1(std::string const & svg) {
    std::vector<float> values;
    std::regex const re(R"_re_(<line[^>]*x1="([^"]+)")_re_");
    auto it = std::sregex_iterator(svg.begin(), svg.end(), re);
    auto const end = std::sregex_iterator();
    for (; it != end; ++it) {
        values.push_back(std::stof((*it)[1].str()));
    }
    return values;
}

}// namespace

// ===========================================================================
// Tests
// ===========================================================================

TEST_CASE("EventPlot export pipeline: identity matrices produce incorrect coordinates",
          "[PlottingSVG][EventPlotExport][regression]") {
    // This test demonstrates the bug: if the scene has identity matrices
    // (as was the case before the fix), glyph positions are wrong.

    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    // Place a glyph at world origin (0, 0)
    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 5.0f;
    style.color = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};

    CorePlotting::RenderableScene bad_scene;
    {
        CorePlotting::SceneBuilder builder;
        builder.setBounds(BoundingBox{-100.0f, -1.0f, 100.0f, 1.0f});
        std::vector<CorePlotting::MappedElement> elements;
        elements.emplace_back(0.0f, 0.0f, EntityId{1});
        builder.addGlyphs("test", std::move(elements), style);
        bad_scene = builder.build();
        // BUG: matrices left as identity — NOT setting projection
    }

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(bad_scene);
    renderer.setCanvasSize(800, 600);
    // Render but don't check — the origin glyph is coincidentally correct.
    // We verify non-origin below.
    static_cast<void>(renderer.render());

    // With identity matrices, a non-origin glyph will be wildly off-canvas.
    CorePlotting::RenderableScene bad_scene2;
    {
        CorePlotting::SceneBuilder builder;
        builder.setBounds(BoundingBox{-100.0f, -1.0f, 100.0f, 1.0f});
        std::vector<CorePlotting::MappedElement> elements;
        elements.emplace_back(50.0f, 0.0f, EntityId{1});
        builder.addGlyphs("test", std::move(elements), style);
        bad_scene2 = builder.build();
        // Still identity matrices
    }

    PlottingSVG::SVGSceneRenderer renderer2;
    renderer2.setScene(bad_scene2);
    renderer2.setCanvasSize(800, 600);
    std::string const svg2 = renderer2.render();

    auto const cx_values = extractCircleCx(svg2);
    REQUIRE(!cx_values.empty());

    // With identity matrices, world x=50 maps to NDC x=50 (way off screen)
    // so svg_x = 800 * (50 + 1) / 2 = 20400 — clearly wrong
    // This should NOT be near 600 (the correct value with proper projection)
    REQUIRE(cx_values[0] > 1000.0f);// confirms the bug: wildly off-canvas
}

TEST_CASE("EventPlot export pipeline: correct projection maps origin to canvas center",
          "[PlottingSVG][EventPlotExport]") {
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    constexpr int canvas_w = 800;
    constexpr int canvas_h = 600;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 5.0f;
    style.color = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};

    CorePlotting::RenderableScene const scene =
            buildTestScene({{0.0f, 0.0f}}, view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(canvas_w, canvas_h);
    std::string const svg = renderer.render();

    auto const cx_values = extractCircleCx(svg);
    auto const cy_values = extractCircleCy(svg);
    REQUIRE(cx_values.size() == 1);
    REQUIRE(cy_values.size() == 1);

    // World (0, 0) should map to canvas center (400, 300)
    REQUIRE(std::abs(cx_values[0] - 400.0f) < 1.0f);
    REQUIRE(std::abs(cy_values[0] - 300.0f) < 1.0f);
}

TEST_CASE("EventPlot export pipeline: glyph at x_max maps to right edge",
          "[PlottingSVG][EventPlotExport]") {
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    constexpr int canvas_w = 800;
    constexpr int canvas_h = 600;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 5.0f;
    style.color = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};

    // Place glyph at +100 (right edge of data range)
    CorePlotting::RenderableScene const scene =
            buildTestScene({{100.0f, 0.0f}}, view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(canvas_w, canvas_h);
    std::string const svg = renderer.render();

    auto const cx_values = extractCircleCx(svg);
    REQUIRE(cx_values.size() == 1);

    // x_max maps to right edge = canvas_w (800)
    REQUIRE(std::abs(cx_values[0] - static_cast<float>(canvas_w)) < 1.0f);
}

TEST_CASE("EventPlot export pipeline: multiple events at known positions",
          "[PlottingSVG][EventPlotExport]") {
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    constexpr int canvas_w = 1000;
    constexpr int canvas_h = 500;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 4.0f;
    style.color = glm::vec4{0.0f, 1.0f, 0.0f, 1.0f};

    // Events at -50, 0, +50 world X, all at Y=0
    CorePlotting::RenderableScene const scene = buildTestScene(
            {{-50.0f, 0.0f}, {0.0f, 0.0f}, {50.0f, 0.0f}},
            view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(canvas_w, canvas_h);
    std::string const svg = renderer.render();

    auto const cx_values = extractCircleCx(svg);
    REQUIRE(cx_values.size() == 3);

    // x=-50 → NDC = -0.5 → canvas_x = 1000 * (-0.5 + 1) / 2 = 250
    REQUIRE(std::abs(cx_values[0] - 250.0f) < 1.0f);
    // x=0 → NDC = 0 → canvas_x = 500
    REQUIRE(std::abs(cx_values[1] - 500.0f) < 1.0f);
    // x=50 → NDC = 0.5 → canvas_x = 750
    REQUIRE(std::abs(cx_values[2] - 750.0f) < 1.0f);
}

TEST_CASE("EventPlot export pipeline: events at different Y positions",
          "[PlottingSVG][EventPlotExport]") {
    // Simulate 2 rows: events at +0.5 (upper) and -0.5 (lower) in world Y
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    constexpr int canvas_w = 800;
    constexpr int canvas_h = 600;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 5.0f;
    style.color = glm::vec4{1.0f, 0.5f, 0.0f, 1.0f};

    // Place one event in upper half (Y=+0.5) and one in lower (Y=-0.5)
    CorePlotting::RenderableScene const scene = buildTestScene(
            {{0.0f, 0.5f}, {0.0f, -0.5f}},
            view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(canvas_w, canvas_h);
    std::string const svg = renderer.render();

    auto const cy_values = extractCircleCy(svg);
    REQUIRE(cy_values.size() == 2);

    // In SVG, Y increases downward. Higher world Y → lower SVG y.
    // So Y=+0.5 should produce a smaller SVG cy than Y=-0.5
    REQUIRE(cy_values[0] < cy_values[1]);

    // Y=+0.5 → NDC=0.5 → svg_y = 600 * (1 - 0.5) / 2 = 150
    REQUIRE(std::abs(cy_values[0] - 150.0f) < 1.0f);
    // Y=-0.5 → NDC=-0.5 → svg_y = 600 * (1 - (-0.5)) / 2 = 450
    REQUIRE(std::abs(cy_values[1] - 450.0f) < 1.0f);
}

TEST_CASE("EventPlot export pipeline: SVG has valid document structure",
          "[PlottingSVG][EventPlotExport]") {
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -50.0;
    view_state.x_max = 50.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;
    style.size = 8.0f;
    style.color = glm::vec4{0.2f, 0.4f, 0.8f, 1.0f};

    CorePlotting::RenderableScene const scene =
            buildTestScene({{-25.0f, 0.0f}, {25.0f, 0.0f}}, view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(1920, 1080);
    renderer.setBackgroundColor("#FAFAFA");
    std::string const svg = renderer.render();

    // Valid SVG structure
    REQUIRE_THAT(svg, Catch::Matchers::StartsWith("<?xml"));
    REQUIRE_THAT(svg, ContainsSubstring("<svg"));
    REQUIRE_THAT(svg, ContainsSubstring("</svg>"));
    REQUIRE_THAT(svg, ContainsSubstring("width=\"1920\""));
    REQUIRE_THAT(svg, ContainsSubstring("height=\"1080\""));
    REQUIRE_THAT(svg, ContainsSubstring(R"(fill="#FAFAFA")"));

    // Tick glyphs produce <line> elements, not <circle>
    REQUIRE_THAT(svg, ContainsSubstring("<line"));
    REQUIRE_THAT(svg, ContainsSubstring(R"(<g id="glyphs">)"));
}

TEST_CASE("EventPlot export pipeline: glyph color is encoded in SVG",
          "[PlottingSVG][EventPlotExport]") {
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -10.0;
    view_state.x_max = 10.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 6.0f;
    // Pure red (1, 0, 0) → #FF0000
    style.color = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};

    CorePlotting::RenderableScene const scene =
            buildTestScene({{0.0f, 0.0f}}, view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(400, 300);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring("#FF0000"));
}

TEST_CASE("EventPlot export pipeline: zoom changes glyph positions",
          "[PlottingSVG][EventPlotExport]") {
    constexpr int canvas_w = 800;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    style.size = 5.0f;

    // No zoom: event at x=50 with range [-100, 100]
    CorePlotting::ViewStateData vs_no_zoom;
    vs_no_zoom.x_min = -100.0;
    vs_no_zoom.x_max = 100.0;
    vs_no_zoom.y_min = -1.0;
    vs_no_zoom.y_max = 1.0;

    auto scene1 = buildTestScene({{50.0f, 0.0f}}, vs_no_zoom, style);
    PlottingSVG::SVGSceneRenderer r1;
    r1.setScene(scene1);
    r1.setCanvasSize(canvas_w, 600);
    auto cx1 = extractCircleCx(r1.render());
    REQUIRE(cx1.size() == 1);

    // 2x zoom: same event but zoomed in
    CorePlotting::ViewStateData vs_2x_zoom = vs_no_zoom;
    vs_2x_zoom.x_zoom = 2.0;

    auto scene2 = buildTestScene({{50.0f, 0.0f}}, vs_2x_zoom, style);
    PlottingSVG::SVGSceneRenderer r2;
    r2.setScene(scene2);
    r2.setCanvasSize(canvas_w, 600);
    auto cx2 = extractCircleCx(r2.render());
    REQUIRE(cx2.size() == 1);

    // At 1x zoom: x=50 in [-100,100] → NDC=0.5 → canvas 600
    // At 2x zoom: visible range is [-50,50], so x=50 is at right edge → canvas 800
    REQUIRE(std::abs(cx1[0] - 600.0f) < 1.0f);
    REQUIRE(std::abs(cx2[0] - static_cast<float>(canvas_w)) < 1.0f);
}

TEST_CASE("EventPlot export pipeline: tick glyphs span ±size/2 around row center",
          "[PlottingSVG][EventPlotExport]") {
    // Simulate a 4-trial raster: each tick should span row_height = 2/4 = 0.5 world units.
    // With size=1.0 (full row height), tick = ±0.25 from row center.
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    constexpr int canvas_w = 800;
    constexpr int canvas_h = 600;
    constexpr int num_trials = 4;
    float const row_height = 2.0f / static_cast<float>(num_trials);// 0.5

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;
    // size = gd.size * row_height, where gd.size=1.0 → size=0.5 (world units)
    style.size = 1.0f * row_height;
    style.color = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};

    // Place ticks at 4 row centers: evenly spaced in [-1,+1].
    // Row centers are at +0.75, +0.25, -0.25, -0.75.
    std::vector<std::pair<float, float>> positions;
    for (int i = 0; i < num_trials; ++i) {
        float const row_center = 1.0f - row_height / 2.0f - static_cast<float>(i) * row_height;
        positions.emplace_back(0.0f, row_center);
    }

    CorePlotting::RenderableScene const scene = buildTestScene(positions, view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(canvas_w, canvas_h);
    std::string const svg = renderer.render();

    auto const line_endpoints = extractLineY(svg);
    REQUIRE(line_endpoints.size() == static_cast<size_t>(num_trials));

    for (int i = 0; i < num_trials; ++i) {
        float const row_center = 1.0f - row_height / 2.0f - static_cast<float>(i) * row_height;
        float const half_size = style.size / 2.0f;

        // Bottom of tick in world Y = row_center - half_size
        // Top of tick in world Y = row_center + half_size
        // SVG y = canvas_h * (1 - ndc_y) / 2  where ndc_y = world_y (ortho maps [-1,1]→[-1,1])
        float const expected_svg_y1 =
                static_cast<float>(canvas_h) * (1.0f - (row_center - half_size)) / 2.0f;
        float const expected_svg_y2 =
                static_cast<float>(canvas_h) * (1.0f - (row_center + half_size)) / 2.0f;

        auto const & ep = line_endpoints[static_cast<size_t>(i)];
        REQUIRE(std::abs(ep.y1 - expected_svg_y1) < 1.0f);
        REQUIRE(std::abs(ep.y2 - expected_svg_y2) < 1.0f);

        // Tick height in SVG pixels should be approximately row_height * canvas_h / 2
        float const expected_svg_height = static_cast<float>(canvas_h) * style.size / 2.0f;
        REQUIRE(std::abs(std::abs(ep.y1 - ep.y2) - expected_svg_height) < 1.0f);
    }
}

TEST_CASE("EventPlot export pipeline: tick X positions map correctly",
          "[PlottingSVG][EventPlotExport]") {
    CorePlotting::ViewStateData view_state;
    view_state.x_min = -100.0;
    view_state.x_max = 100.0;
    view_state.y_min = -1.0;
    view_state.y_max = 1.0;

    constexpr int canvas_w = 1000;
    constexpr int canvas_h = 500;

    CorePlotting::GlyphStyle style;
    style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;
    style.size = 0.5f;
    style.color = glm::vec4{0.0f, 1.0f, 0.0f, 1.0f};

    // Ticks at x=-50, 0, +50 all in same row (y=0)
    CorePlotting::RenderableScene const scene = buildTestScene(
            {{-50.0f, 0.0f}, {0.0f, 0.0f}, {50.0f, 0.0f}},
            view_state, style);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(canvas_w, canvas_h);
    std::string const svg = renderer.render();

    auto const x1_values = extractLineX1(svg);
    REQUIRE(x1_values.size() == 3);

    // x=-50 → NDC=-0.5 → svg_x = 1000 * (-0.5+1)/2 = 250
    REQUIRE(std::abs(x1_values[0] - 250.0f) < 1.0f);
    // x=0 → svg_x = 500
    REQUIRE(std::abs(x1_values[1] - 500.0f) < 1.0f);
    // x=50 → svg_x = 750
    REQUIRE(std::abs(x1_values[2] - 750.0f) < 1.0f);
}
