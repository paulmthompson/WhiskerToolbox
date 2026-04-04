/**
 * @file SVGSceneRenderer.cpp
 * @brief Scene-level SVG export orchestration (stubs completed in roadmap 1.3–1.5).
 */

#include "PlottingSVG/SVGSceneRenderer.hpp"

#include "PlottingSVG/Renderers/SVGGlyphRenderer.hpp"
#include "PlottingSVG/Renderers/SVGPolyLineRenderer.hpp"
#include "PlottingSVG/Renderers/SVGRectangleRenderer.hpp"
#include "PlottingSVG/SVGDocument.hpp"

#include <fstream>
#include <utility>

namespace PlottingSVG {

SVGSceneRenderer::SVGSceneRenderer() = default;

void SVGSceneRenderer::setScene(CorePlotting::RenderableScene const & scene) {
    _scene = &scene;
}

void SVGSceneRenderer::setCanvasSize(int width, int height) {
    _canvas_width = width;
    _canvas_height = height;
}

void SVGSceneRenderer::setBackgroundColor(std::string const & hex_color) {
    _background_hex = hex_color;
}

void SVGSceneRenderer::addDecoration(std::unique_ptr<SVGDecoration> decoration) {
    if (decoration != nullptr) {
        _decorations.push_back(std::move(decoration));
    }
}

void SVGSceneRenderer::clearDecorations() {
    _decorations.clear();
}

std::string SVGSceneRenderer::render() const {
    SVGDocument document(_canvas_width, _canvas_height);
    document.setBackground(_background_hex);

    if (_scene == nullptr) {
        return document.build();
    }

    glm::mat4 const view = _scene->view_matrix;
    glm::mat4 const projection = _scene->projection_matrix;

    for (CorePlotting::RenderableRectangleBatch const & batch: _scene->rectangle_batches) {
        document.addElements(
                "rectangles",
                SVGRectangleRenderer::render(batch, view, projection, _canvas_width, _canvas_height));
    }
    for (CorePlotting::RenderablePolyLineBatch const & batch: _scene->poly_line_batches) {
        document.addElements(
                "polylines",
                SVGPolyLineRenderer::render(batch, view, projection, _canvas_width, _canvas_height));
    }
    for (CorePlotting::RenderableGlyphBatch const & batch: _scene->glyph_batches) {
        document.addElements(
                "glyphs",
                SVGGlyphRenderer::render(batch, view, projection, _canvas_width, _canvas_height));
    }

    std::vector<std::string> decoration_elements;
    for (std::unique_ptr<SVGDecoration> const & decoration: _decorations) {
        std::vector<std::string> const part =
                decoration->render(_canvas_width, _canvas_height);
        decoration_elements.insert(decoration_elements.end(), part.begin(), part.end());
    }
    if (!decoration_elements.empty()) {
        document.addElements("decorations", decoration_elements);
    }

    return document.build();
}

bool SVGSceneRenderer::renderToFile(std::filesystem::path const & path) const {
    std::string const svg = render();
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << svg;
    return static_cast<bool>(out);
}

}// namespace PlottingSVG
