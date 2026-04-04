/**
 * @file SVGGlyphRenderer.test.cpp
 * @brief Tests for PlottingSVG::SVGGlyphRenderer (tick, circle, square, cross).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/Renderers/SVGGlyphRenderer.hpp"

#include <glm/glm.hpp>

using Catch::Matchers::ContainsSubstring;
using GlyphType = CorePlotting::RenderableGlyphBatch::GlyphType;

namespace {

CorePlotting::RenderableGlyphBatch makeSingleGlyphBatch(GlyphType type) {
    CorePlotting::RenderableGlyphBatch batch;
    batch.positions = {glm::vec2{0.0f, 0.0f}};
    batch.colors = {glm::vec4{1.0f, 0.0f, 0.0f, 0.8f}};
    batch.glyph_type = type;
    batch.size = 10.0f;
    batch.model_matrix = glm::mat4{1.0f};
    return batch;
}

}// namespace

TEST_CASE("SVGGlyphRenderer returns empty for empty batch",
          "[PlottingSVG][SVGGlyphRenderer]") {
    CorePlotting::RenderableGlyphBatch batch;
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.empty());
}

TEST_CASE("SVGGlyphRenderer Tick produces line element",
          "[PlottingSVG][SVGGlyphRenderer]") {
    auto batch = makeSingleGlyphBatch(GlyphType::Tick);
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring("<line"));
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="#FF0000")"));
    REQUIRE_THAT(elements[0], ContainsSubstring("stroke-opacity=\"0.8\""));
}

TEST_CASE("SVGGlyphRenderer Tick draws from y=-1 to y=+1 in world space",
          "[PlottingSVG][SVGGlyphRenderer]") {
    auto batch = makeSingleGlyphBatch(GlyphType::Tick);
    glm::mat4 const I{1.0f};
    int const W = 100;
    int const H = 100;

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, W, H);
    REQUIRE(elements.size() == 1);

    // y=-1 → svg_y = 100*(1-(-1))/2 = 100 (bottom)
    // y=+1 → svg_y = 100*(1-1)/2 = 0 (top)
    // x=0 for both → svg_x = 100*(0+1)/2 = 50
    REQUIRE_THAT(elements[0], ContainsSubstring("y1=\"100\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("y2=\"0\""));
}

TEST_CASE("SVGGlyphRenderer Circle produces circle element",
          "[PlottingSVG][SVGGlyphRenderer]") {
    auto batch = makeSingleGlyphBatch(GlyphType::Circle);
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring("<circle"));
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(fill="#FF0000")"));
    REQUIRE_THAT(elements[0], ContainsSubstring("fill-opacity=\"0.8\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("r=\"5\""));// size/2 = 10/2 = 5
}

TEST_CASE("SVGGlyphRenderer Square produces rect element",
          "[PlottingSVG][SVGGlyphRenderer]") {
    auto batch = makeSingleGlyphBatch(GlyphType::Square);
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring("<rect"));
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(fill="#FF0000")"));
    REQUIRE_THAT(elements[0], ContainsSubstring("width=\"10\""));// batch.size
    REQUIRE_THAT(elements[0], ContainsSubstring("height=\"10\""));
}

TEST_CASE("SVGGlyphRenderer Cross produces two line elements",
          "[PlottingSVG][SVGGlyphRenderer]") {
    auto batch = makeSingleGlyphBatch(GlyphType::Cross);
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 2);
    REQUIRE_THAT(elements[0], ContainsSubstring("<line"));
    REQUIRE_THAT(elements[1], ContainsSubstring("<line"));
    // Both have the color
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="#FF0000")"));
    REQUIRE_THAT(elements[1], ContainsSubstring(R"(stroke="#FF0000")"));
}

TEST_CASE("SVGGlyphRenderer uses white when no per-glyph color",
          "[PlottingSVG][SVGGlyphRenderer]") {
    CorePlotting::RenderableGlyphBatch batch;
    batch.positions = {glm::vec2{0.0f, 0.0f}};
    // No colors set
    batch.glyph_type = GlyphType::Circle;
    batch.size = 5.0f;
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(fill="#FFFFFF")"));
}

TEST_CASE("SVGGlyphRenderer multiple glyphs produces correct count",
          "[PlottingSVG][SVGGlyphRenderer]") {
    CorePlotting::RenderableGlyphBatch batch;
    batch.positions = {
            glm::vec2{-0.5f, 0.0f},
            glm::vec2{0.0f, 0.0f},
            glm::vec2{0.5f, 0.0f}};
    batch.glyph_type = GlyphType::Circle;
    batch.size = 4.0f;
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 3);
}

TEST_CASE("SVGGlyphRenderer Cross with multiple glyphs produces double elements",
          "[PlottingSVG][SVGGlyphRenderer]") {
    CorePlotting::RenderableGlyphBatch batch;
    batch.positions = {glm::vec2{0.0f, 0.0f}, glm::vec2{0.5f, 0.5f}};
    batch.glyph_type = GlyphType::Cross;
    batch.size = 6.0f;
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 4);// 2 glyphs × 2 lines each
}

TEST_CASE("SVGGlyphRenderer Circle position at origin with identity MVP",
          "[PlottingSVG][SVGGlyphRenderer]") {
    auto batch = makeSingleGlyphBatch(GlyphType::Circle);
    glm::mat4 const I{1.0f};
    int const W = 200;
    int const H = 200;

    auto const elements = PlottingSVG::SVGGlyphRenderer::render(batch, I, I, W, H);
    REQUIRE(elements.size() == 1);
    // (0,0) → svg (100, 100) with 200x200 canvas
    REQUIRE_THAT(elements[0], ContainsSubstring("cx=\"100\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("cy=\"100\""));
}
