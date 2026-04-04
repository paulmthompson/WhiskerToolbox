/**
 * @file SVGExport.cpp
 * @brief Flat SVG document and scalebar assembly (DataViewer export path).
 */

#include "PlottingSVG/SVGExport.hpp"

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
    std::vector<std::string> elements;

    int const padding = 50;
    int const bar_height = 4;
    int const tick_height = 8;
    float const tick_half = 0.5F * static_cast<float>(tick_height);

    float const time_span = time_range.end - time_range.start;
    float const time_to_pixel = static_cast<float>(params.canvas_width) / time_span;
    float const bar_width_pixels = static_cast<float>(scalebar_length) * time_to_pixel;

    float const bar_x = static_cast<float>(params.canvas_width) - bar_width_pixels - static_cast<float>(padding);
    float const bar_y = static_cast<float>(params.canvas_height) - static_cast<float>(padding);

    std::ostringstream bar;
    bar << R"(<line x1=")" << bar_x << R"(" y1=")" << bar_y << R"(" x2=")" << (bar_x + bar_width_pixels)
        << R"(" y2=")" << bar_y << R"(" stroke="#000000" stroke-width=")" << bar_height
        << R"(" stroke-linecap="butt"/>)";
    elements.push_back(bar.str());

    std::ostringstream left_tick;
    left_tick << R"(<line x1=")" << bar_x << R"(" y1=")" << (bar_y - tick_half) << R"(" x2=")" << bar_x
              << R"(" y2=")" << (bar_y + tick_half) << R"(" stroke="#000000" stroke-width="2"/>)";
    elements.push_back(left_tick.str());

    std::ostringstream right_tick;
    right_tick << R"(<line x1=")" << (bar_x + bar_width_pixels) << R"(" y1=")" << (bar_y - tick_half)
               << R"(" x2=")" << (bar_x + bar_width_pixels) << R"(" y2=")" << (bar_y + tick_half)
               << R"(" stroke="#000000" stroke-width="2"/>)";
    elements.push_back(right_tick.str());

    float const label_x = bar_x + bar_width_pixels / 2.0f;
    float const label_y = bar_y - 10.0f;
    std::ostringstream label;
    label << R"(<text x=")" << label_x << R"(" y=")" << label_y
          << R"(" font-family="Arial, sans-serif" font-size="14" fill="#000000" text-anchor="middle">)"
          << scalebar_length << R"(</text>)";
    elements.push_back(label.str());

    return elements;
}

}// namespace PlottingSVG
