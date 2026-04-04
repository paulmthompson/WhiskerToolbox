/**
 * @file SVGPolyLineRenderer.test.cpp
 * @brief Tests for PlottingSVG::SVGPolyLineRenderer.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/Renderers/SVGPolyLineRenderer.hpp"

#include <glm/glm.hpp>

using Catch::Matchers::ContainsSubstring;

namespace {

CorePlotting::RenderablePolyLineBatch makeSimpleBatch() {
    CorePlotting::RenderablePolyLineBatch batch;
    // Single line: (0, 0) to (1, 1) — two vertices = 4 floats
    batch.vertices = {0.0f, 0.0f, 1.0f, 1.0f};
    batch.line_start_indices = {0};
    batch.line_vertex_counts = {2};
    batch.thickness = 2.0f;
    batch.global_color = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
    batch.model_matrix = glm::mat4{1.0f};
    return batch;
}

}// namespace

TEST_CASE("SVGPolyLineRenderer returns empty for empty batch",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    CorePlotting::RenderablePolyLineBatch batch;
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.empty());
}

TEST_CASE("SVGPolyLineRenderer produces one polyline for single line batch",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    auto batch = makeSimpleBatch();
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring("<polyline"));
    REQUIRE_THAT(elements[0], ContainsSubstring("points="));
}

TEST_CASE("SVGPolyLineRenderer uses global_color for stroke",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    auto batch = makeSimpleBatch();
    batch.global_color = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="#FF0000")"));
}

TEST_CASE("SVGPolyLineRenderer uses per-line color when available",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    auto batch = makeSimpleBatch();
    batch.colors = {glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="#0000FF")"));
}

TEST_CASE("SVGPolyLineRenderer includes thickness and style attributes",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    auto batch = makeSimpleBatch();
    batch.thickness = 3.5f;
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring("stroke-width=\"3.5\""));
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke-linejoin="round")"));
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke-linecap="round")"));
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(fill="none")"));
}

TEST_CASE("SVGPolyLineRenderer skips lines with fewer than 2 vertices",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    CorePlotting::RenderablePolyLineBatch batch;
    batch.vertices = {0.0f, 0.0f};
    batch.line_start_indices = {0};
    batch.line_vertex_counts = {1};
    batch.thickness = 1.0f;
    batch.global_color = glm::vec4{1.0f};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.empty());
}

TEST_CASE("SVGPolyLineRenderer handles multiple lines in one batch",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    CorePlotting::RenderablePolyLineBatch batch;
    // Two lines: (0,0)-(1,0) and (0,1)-(1,1)
    batch.vertices = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    batch.line_start_indices = {0, 2};
    batch.line_vertex_counts = {2, 2};
    batch.thickness = 1.0f;
    batch.global_color = glm::vec4{1.0f};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 100, 100);
    REQUIRE(elements.size() == 2);
}

TEST_CASE("SVGPolyLineRenderer applies MVP transform to vertices",
          "[PlottingSVG][SVGPolyLineRenderer]") {
    CorePlotting::RenderablePolyLineBatch batch;
    // Line from (-1, 0) to (1, 0) in world space
    batch.vertices = {-1.0f, 0.0f, 1.0f, 0.0f};
    batch.line_start_indices = {0};
    batch.line_vertex_counts = {2};
    batch.thickness = 1.0f;
    batch.global_color = glm::vec4{1.0f};
    batch.model_matrix = glm::mat4{1.0f};
    glm::mat4 const I{1.0f};

    auto const elements = PlottingSVG::SVGPolyLineRenderer::render(batch, I, I, 200, 200);
    REQUIRE(elements.size() == 1);

    // With identity MVP: (-1,0) → svg(0, 100), (1,0) → svg(200, 100)
    // Points string should contain "0,100" and "200,100"
    REQUIRE_THAT(elements[0], ContainsSubstring("0,100"));
    REQUIRE_THAT(elements[0], ContainsSubstring("200,100"));
}
