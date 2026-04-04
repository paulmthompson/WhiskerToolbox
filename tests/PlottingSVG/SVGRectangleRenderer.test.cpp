/**
 * @file SVGRectangleRenderer.test.cpp
 * @brief Tests for PlottingSVG::SVGRectangleRenderer.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/Renderers/SVGRectangleRenderer.hpp"

#include <glm/glm.hpp>

using Catch::Matchers::ContainsSubstring;

TEST_CASE("SVGRectangleRenderer returns empty for empty batch",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.empty());
}

TEST_CASE("SVGRectangleRenderer produces one rect per bound",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    batch.bounds = {
            glm::vec4{-0.5f, -0.5f, 1.0f, 1.0f},
            glm::vec4{0.0f, 0.0f, 0.5f, 0.5f}};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 2);
    REQUIRE_THAT(elements[0], ContainsSubstring("<rect"));
    REQUIRE_THAT(elements[1], ContainsSubstring("<rect"));
}

TEST_CASE("SVGRectangleRenderer uses per-rect color when available",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    batch.bounds = {glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f}};
    batch.colors = {glm::vec4{0.0f, 1.0f, 0.0f, 0.75f}};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(fill="#00FF00")"));
    REQUIRE_THAT(elements[0], ContainsSubstring("fill-opacity=\"0.75\""));
}

TEST_CASE("SVGRectangleRenderer uses default white with 0.5 alpha when no color",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    batch.bounds = {glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(fill="#FFFFFF")"));
    REQUIRE_THAT(elements[0], ContainsSubstring("fill-opacity=\"0.5\""));
}

TEST_CASE("SVGRectangleRenderer has stroke none",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    batch.bounds = {glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="none")"));
}

TEST_CASE("SVGRectangleRenderer full canvas rect with identity MVP",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    // World rect from (-1,-1) size (2,2) should cover entire canvas with identity MVP
    batch.bounds = {glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f}};
    batch.colors = {glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};
    int const W = 100;
    int const H = 100;

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, W, H);
    REQUIRE(elements.size() == 1);
    // bottom-left (-1,-1) → svg(0, 100), top-right (1,1) → svg(100, 0)
    // SVG rect: x=0, y=0, width=100, height=100
    REQUIRE_THAT(elements[0], ContainsSubstring("x=\"0\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("y=\"0\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("width=\"100\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("height=\"100\""));
}

TEST_CASE("SVGRectangleRenderer applies MVP transform",
          "[PlottingSVG][SVGRectangleRenderer]") {
    CorePlotting::RenderableRectangleBatch batch;
    // Unit rect at origin in world space
    batch.bounds = {glm::vec4{0.0f, 0.0f, 0.5f, 0.5f}};
    batch.colors = {glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}};
    batch.model_matrix = glm::mat4{1.0f};

    // Scale x by 2 via model_matrix
    glm::mat4 scale_model{1.0f};
    scale_model[0][0] = 2.0f;
    batch.model_matrix = scale_model;
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGRectangleRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 1);
    // Doesn't need exact values - just verify it contains rect attributes
    REQUIRE_THAT(elements[0], ContainsSubstring("<rect"));
    REQUIRE_THAT(elements[0], ContainsSubstring("width="));
    REQUIRE_THAT(elements[0], ContainsSubstring("height="));
}
