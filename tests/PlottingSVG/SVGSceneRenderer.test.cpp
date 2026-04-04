/**
 * @file SVGSceneRenderer.test.cpp
 * @brief Tests for PlottingSVG::SVGSceneRenderer full-scene rendering.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/Decorations/SVGScalebar.hpp"
#include "PlottingSVG/SVGSceneRenderer.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

TEST_CASE("SVGSceneRenderer render with no scene produces valid SVG",
          "[PlottingSVG][SVGSceneRenderer]") {
    PlottingSVG::SVGSceneRenderer const renderer;
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, StartsWith("<?xml"));
    REQUIRE_THAT(svg, ContainsSubstring("<svg"));
    REQUIRE_THAT(svg, ContainsSubstring("</svg>"));
}

TEST_CASE("SVGSceneRenderer default canvas is 1920x1080",
          "[PlottingSVG][SVGSceneRenderer]") {
    PlottingSVG::SVGSceneRenderer const renderer;
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring("width=\"1920\""));
    REQUIRE_THAT(svg, ContainsSubstring("height=\"1080\""));
}

TEST_CASE("SVGSceneRenderer setCanvasSize changes dimensions",
          "[PlottingSVG][SVGSceneRenderer]") {
    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setCanvasSize(800, 600);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring("width=\"800\""));
    REQUIRE_THAT(svg, ContainsSubstring("height=\"600\""));
}

TEST_CASE("SVGSceneRenderer setBackgroundColor changes fill",
          "[PlottingSVG][SVGSceneRenderer]") {
    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setBackgroundColor("#123456");
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring(R"(fill="#123456")"));
}

TEST_CASE("SVGSceneRenderer renders glyph batches into glyphs layer",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    CorePlotting::RenderableGlyphBatch glyph_batch;
    glyph_batch.positions = {glm::vec2{0.0f, 0.0f}};
    glyph_batch.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    glyph_batch.size = 5.0f;
    glyph_batch.model_matrix = glm::mat4{1.0f};
    scene.glyph_batches.push_back(glyph_batch);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(100, 100);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring(R"(<g id="glyphs">)"));
    REQUIRE_THAT(svg, ContainsSubstring("<circle"));
}

TEST_CASE("SVGSceneRenderer renders polyline batches into polylines layer",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    CorePlotting::RenderablePolyLineBatch line_batch;
    line_batch.vertices = {-1.0f, 0.0f, 1.0f, 0.0f};
    line_batch.line_start_indices = {0};
    line_batch.line_vertex_counts = {2};
    line_batch.thickness = 1.0f;
    line_batch.global_color = glm::vec4{1.0f};
    line_batch.model_matrix = glm::mat4{1.0f};
    scene.poly_line_batches.push_back(line_batch);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(100, 100);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring(R"(<g id="polylines">)"));
    REQUIRE_THAT(svg, ContainsSubstring("<polyline"));
}

TEST_CASE("SVGSceneRenderer renders rectangle batches into rectangles layer",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    CorePlotting::RenderableRectangleBatch rect_batch;
    rect_batch.bounds = {glm::vec4{-0.5f, -0.5f, 1.0f, 1.0f}};
    rect_batch.colors = {glm::vec4{0.0f, 1.0f, 0.0f, 0.5f}};
    rect_batch.model_matrix = glm::mat4{1.0f};
    scene.rectangle_batches.push_back(rect_batch);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(100, 100);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring(R"(<g id="rectangles">)"));
    REQUIRE_THAT(svg, ContainsSubstring("<rect"));
}

TEST_CASE("SVGSceneRenderer layer order is rectangles then polylines then glyphs",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    CorePlotting::RenderableRectangleBatch rect_batch;
    rect_batch.bounds = {glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
    rect_batch.model_matrix = glm::mat4{1.0f};
    scene.rectangle_batches.push_back(rect_batch);

    CorePlotting::RenderablePolyLineBatch line_batch;
    line_batch.vertices = {-1.0f, 0.0f, 1.0f, 0.0f};
    line_batch.line_start_indices = {0};
    line_batch.line_vertex_counts = {2};
    line_batch.thickness = 1.0f;
    line_batch.global_color = glm::vec4{1.0f};
    line_batch.model_matrix = glm::mat4{1.0f};
    scene.poly_line_batches.push_back(line_batch);

    CorePlotting::RenderableGlyphBatch glyph_batch;
    glyph_batch.positions = {glm::vec2{0.0f, 0.0f}};
    glyph_batch.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    glyph_batch.size = 5.0f;
    glyph_batch.model_matrix = glm::mat4{1.0f};
    scene.glyph_batches.push_back(glyph_batch);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(100, 100);
    std::string const svg = renderer.render();

    auto const pos_rect = svg.find(R"(id="rectangles")");
    auto const pos_line = svg.find(R"(id="polylines")");
    auto const pos_glyph = svg.find(R"(id="glyphs")");

    REQUIRE(pos_rect != std::string::npos);
    REQUIRE(pos_line != std::string::npos);
    REQUIRE(pos_glyph != std::string::npos);
    REQUIRE(pos_rect < pos_line);
    REQUIRE(pos_line < pos_glyph);
}

TEST_CASE("SVGSceneRenderer decorations appear in decorations layer",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(1000, 500);
    renderer.addDecoration(
            std::make_unique<PlottingSVG::SVGScalebar>(100, 0.0f, 1000.0f));
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, ContainsSubstring(R"(<g id="decorations">)"));
    REQUIRE_THAT(svg, ContainsSubstring("<line"));// scalebar bar
    REQUIRE_THAT(svg, ContainsSubstring("<text"));// scalebar label
}

TEST_CASE("SVGSceneRenderer no decoration layer when no decorations added",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, !ContainsSubstring(R"(id="decorations")"));
}

TEST_CASE("SVGSceneRenderer clearDecorations removes decorations",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(1000, 500);
    renderer.addDecoration(
            std::make_unique<PlottingSVG::SVGScalebar>(50, 0.0f, 500.0f));
    renderer.clearDecorations();
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, !ContainsSubstring(R"(id="decorations")"));
}

TEST_CASE("SVGSceneRenderer addDecoration ignores nullptr",
          "[PlottingSVG][SVGSceneRenderer]") {
    PlottingSVG::SVGSceneRenderer renderer;
    renderer.addDecoration(nullptr);
    std::string const svg = renderer.render();

    REQUIRE_THAT(svg, !ContainsSubstring(R"(id="decorations")"));
}

TEST_CASE("SVGSceneRenderer renderToFile writes to disk",
          "[PlottingSVG][SVGSceneRenderer]") {
    std::filesystem::path const temp_path =
            std::filesystem::temp_directory_path() / "plottingsvg_test_output.svg";

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setCanvasSize(100, 100);
    bool const ok = renderer.renderToFile(temp_path);

    REQUIRE(ok);
    REQUIRE(std::filesystem::exists(temp_path));

    std::ifstream in(temp_path);
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    REQUIRE_THAT(content, StartsWith("<?xml"));
    REQUIRE_THAT(content, ContainsSubstring("</svg>"));

    std::filesystem::remove(temp_path);
}

TEST_CASE("SVGSceneRenderer renderToFile returns false for invalid path",
          "[PlottingSVG][SVGSceneRenderer]") {
    PlottingSVG::SVGSceneRenderer const renderer;
    bool const ok = renderer.renderToFile("/nonexistent_dir_12345/output.svg");
    REQUIRE_FALSE(ok);
}

TEST_CASE("SVGSceneRenderer render is deterministic",
          "[PlottingSVG][SVGSceneRenderer]") {
    CorePlotting::RenderableScene scene;
    scene.view_matrix = glm::mat4{1.0f};
    scene.projection_matrix = glm::mat4{1.0f};

    CorePlotting::RenderableGlyphBatch batch;
    batch.positions = {glm::vec2{0.0f, 0.0f}, glm::vec2{0.5f, 0.5f}};
    batch.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    batch.size = 5.0f;
    batch.model_matrix = glm::mat4{1.0f};
    scene.glyph_batches.push_back(batch);

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(scene);
    renderer.setCanvasSize(100, 100);

    std::string const svg1 = renderer.render();
    std::string const svg2 = renderer.render();
    REQUIRE(svg1 == svg2);
}
