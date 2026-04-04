/**
 * @file SVGExport.cpp
 * @brief Flat SVG document and scalebar assembly (DataViewer export path).
 */

#include "PlottingSVG/SVGExport.hpp"

#include "PlottingSVG/Decorations/SVGScalebar.hpp"
#include "PlottingSVG/Renderers/SVGGlyphRenderer.hpp"
#include "PlottingSVG/Renderers/SVGPolyLineRenderer.hpp"
#include "PlottingSVG/Renderers/SVGRectangleRenderer.hpp"

#include <sstream>

namespace PlottingSVG {

std::vector<std::string> renderSceneToSVG(
        CorePlotting::RenderableScene const & scene,
        SVGExportParams const & params) {
    std::vector<std::string> elements;

    for (CorePlotting::RenderableRectangleBatch const & batch: scene.rectangle_batches) {
        std::vector<std::string> const batch_elements = SVGRectangleRenderer::render(
                batch,
                scene.view_matrix,
                scene.projection_matrix,
                params.canvas_width,
                params.canvas_height);
        elements.insert(elements.end(), batch_elements.begin(), batch_elements.end());
    }

    for (CorePlotting::RenderablePolyLineBatch const & batch: scene.poly_line_batches) {
        std::vector<std::string> const batch_elements = SVGPolyLineRenderer::render(
                batch,
                scene.view_matrix,
                scene.projection_matrix,
                params.canvas_width,
                params.canvas_height);
        elements.insert(elements.end(), batch_elements.begin(), batch_elements.end());
    }

    for (CorePlotting::RenderableGlyphBatch const & batch: scene.glyph_batches) {
        std::vector<std::string> const batch_elements = SVGGlyphRenderer::render(
                batch,
                scene.view_matrix,
                scene.projection_matrix,
                params.canvas_width,
                params.canvas_height);
        elements.insert(elements.end(), batch_elements.begin(), batch_elements.end());
    }

    return elements;
}

std::string buildSVGDocument(
        CorePlotting::RenderableScene const & scene,
        SVGExportParams const & params) {
    std::ostringstream doc;

    doc << R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)" << '\n';
    doc << R"(<svg width=")" << params.canvas_width << R"(" height=")" << params.canvas_height
        << R"(" viewBox="0 0 )" << params.canvas_width << ' ' << params.canvas_height
        << R"(" xmlns="http://www.w3.org/2000/svg" version="1.1">)" << '\n';
    doc << R"(  <desc>WhiskerToolbox DataViewer Export</desc>)" << '\n';
    doc << R"(  <rect width="100%" height="100%" fill=")" << params.background_color << R"("/>)" << '\n';

    std::vector<std::string> const elements = renderSceneToSVG(scene, params);
    for (std::string const & element: elements) {
        doc << "  " << element << '\n';
    }

    doc << "</svg>";
    return doc.str();
}

std::vector<std::string> createScalebarSVG(
        int scalebar_length,
        ScalebarTimeRange time_range,
        SVGExportParams const & params) {
    SVGScalebar const scalebar(scalebar_length, time_range.start, time_range.end);
    return scalebar.render(params.canvas_width, params.canvas_height);
}

}// namespace PlottingSVG
