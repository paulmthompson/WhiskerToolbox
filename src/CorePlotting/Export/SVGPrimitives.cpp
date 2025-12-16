#include "SVGPrimitives.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace CorePlotting {

glm::vec2 transformVertexToSVG(
        glm::vec4 const & vertex,
        glm::mat4 const & mvp,
        int canvas_width,
        int canvas_height) {

    // Apply MVP transformation to get normalized device coordinates (NDC)
    glm::vec4 ndc = mvp * vertex;

    // Perform perspective divide if w != 0
    if (std::abs(ndc.w) > 1e-6f) {
        ndc /= ndc.w;
    }

    // Map NDC [-1, 1] to SVG coordinates [0, width] × [0, height]
    // Note: SVG Y-axis is inverted (top = 0, bottom = height)
    float const svg_x = static_cast<float>(canvas_width) * (ndc.x + 1.0f) / 2.0f;
    float const svg_y = static_cast<float>(canvas_height) * (1.0f - ndc.y) / 2.0f;

    return glm::vec2(svg_x, svg_y);
}

std::string colorToSVGHex(glm::vec4 const & color) {
    int const r = static_cast<int>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
    int const g = static_cast<int>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
    int const b = static_cast<int>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));

    std::ostringstream ss;
    ss << "#" << std::hex << std::uppercase << std::setfill('0')
       << std::setw(2) << r
       << std::setw(2) << g
       << std::setw(2) << b;
    return ss.str();
}

std::vector<std::string> renderPolyLineBatchToSVG(
        RenderablePolyLineBatch const & batch,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        SVGExportParams const & params) {

    std::vector<std::string> elements;

    if (batch.vertices.empty() || batch.line_start_indices.empty()) {
        return elements;
    }

    glm::mat4 const mvp = projection_matrix * view_matrix * batch.model_matrix;

    // Process each line segment
    for (size_t line_idx = 0; line_idx < batch.line_start_indices.size(); ++line_idx) {
        int const start_index = batch.line_start_indices[line_idx];
        int const vertex_count = batch.line_vertex_counts[line_idx];

        if (vertex_count < 2) {
            continue;
        }

        // Determine color for this line
        glm::vec4 color = batch.global_color;
        if (!batch.colors.empty() && line_idx < batch.colors.size()) {
            color = batch.colors[line_idx];
        }
        std::string const color_hex = colorToSVGHex(color);

        // Build points string for polyline
        std::ostringstream points;
        for (int i = 0; i < vertex_count; ++i) {
            int const vert_idx = (start_index + i) * 2;// 2 floats per vertex (x, y)
            if (vert_idx + 1 >= static_cast<int>(batch.vertices.size())) {
                break;
            }

            float const x = batch.vertices[static_cast<size_t>(vert_idx)];
            float const y = batch.vertices[static_cast<size_t>(vert_idx + 1)];

            glm::vec4 const vertex(x, y, 0.0f, 1.0f);
            glm::vec2 const svg_pos = transformVertexToSVG(vertex, mvp, params.canvas_width, params.canvas_height);

            if (i > 0) {
                points << " ";
            }
            points << svg_pos.x << "," << svg_pos.y;
        }

        // Create SVG polyline element
        std::ostringstream element;
        element << R"(<polyline points=")" << points.str()
                << R"(" fill="none" stroke=")" << color_hex
                << R"(" stroke-width=")" << batch.thickness
                << R"(" stroke-linejoin="round" stroke-linecap="round"/>)";

        elements.push_back(element.str());
    }

    return elements;
}

std::vector<std::string> renderGlyphBatchToSVG(
        RenderableGlyphBatch const & batch,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        SVGExportParams const & params) {

    std::vector<std::string> elements;

    if (batch.positions.empty()) {
        return elements;
    }

    glm::mat4 const mvp = projection_matrix * view_matrix * batch.model_matrix;

    for (size_t i = 0; i < batch.positions.size(); ++i) {
        glm::vec2 const & pos = batch.positions[i];

        // Determine color for this glyph
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        if (!batch.colors.empty() && i < batch.colors.size()) {
            color = batch.colors[i];
        }
        std::string const color_hex = colorToSVGHex(color);
        float const alpha = color.a;

        // Transform position to SVG coordinates
        glm::vec4 const vertex(pos.x, pos.y, 0.0f, 1.0f);
        glm::vec2 const svg_pos = transformVertexToSVG(vertex, mvp, params.canvas_width, params.canvas_height);

        std::ostringstream element;

        switch (batch.glyph_type) {
            case RenderableGlyphBatch::GlyphType::Tick: {
                // Vertical line tick
                // Transform points at y=-1 and y=1 to get the full tick height
                glm::vec4 const bottom_vertex(pos.x, -1.0f, 0.0f, 1.0f);
                glm::vec4 const top_vertex(pos.x, 1.0f, 0.0f, 1.0f);
                glm::vec2 const svg_bottom = transformVertexToSVG(bottom_vertex, mvp, params.canvas_width, params.canvas_height);
                glm::vec2 const svg_top = transformVertexToSVG(top_vertex, mvp, params.canvas_width, params.canvas_height);

                element << R"(<line x1=")" << svg_bottom.x << R"(" y1=")" << svg_bottom.y
                        << R"(" x2=")" << svg_top.x << R"(" y2=")" << svg_top.y
                        << R"(" stroke=")" << color_hex
                        << R"(" stroke-width=")" << batch.size
                        << R"(" stroke-opacity=")" << alpha << R"("/>)";
                break;
            }
            case RenderableGlyphBatch::GlyphType::Circle: {
                element << R"(<circle cx=")" << svg_pos.x << R"(" cy=")" << svg_pos.y
                        << R"(" r=")" << batch.size / 2.0f
                        << R"(" fill=")" << color_hex
                        << R"(" fill-opacity=")" << alpha << R"("/>)";
                break;
            }
            case RenderableGlyphBatch::GlyphType::Square: {
                float const half_size = batch.size / 2.0f;
                element << R"(<rect x=")" << (svg_pos.x - half_size) << R"(" y=")" << (svg_pos.y - half_size)
                        << R"(" width=")" << batch.size << R"(" height=")" << batch.size
                        << R"(" fill=")" << color_hex
                        << R"(" fill-opacity=")" << alpha << R"("/>)";
                break;
            }
            case RenderableGlyphBatch::GlyphType::Cross: {
                float const half_size = batch.size / 2.0f;
                // Horizontal line
                element << R"(<line x1=")" << (svg_pos.x - half_size) << R"(" y1=")" << svg_pos.y
                        << R"(" x2=")" << (svg_pos.x + half_size) << R"(" y2=")" << svg_pos.y
                        << R"(" stroke=")" << color_hex
                        << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                elements.push_back(element.str());

                // Vertical line (separate element)
                std::ostringstream element2;
                element2 << R"(<line x1=")" << svg_pos.x << R"(" y1=")" << (svg_pos.y - half_size)
                         << R"(" x2=")" << svg_pos.x << R"(" y2=")" << (svg_pos.y + half_size)
                         << R"(" stroke=")" << color_hex
                         << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                elements.push_back(element2.str());
                continue;// Skip the regular push_back below
            }
        }

        elements.push_back(element.str());
    }

    return elements;
}

std::vector<std::string> renderRectangleBatchToSVG(
        RenderableRectangleBatch const & batch,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        SVGExportParams const & params) {

    std::vector<std::string> elements;

    if (batch.bounds.empty()) {
        return elements;
    }

    glm::mat4 const mvp = projection_matrix * view_matrix * batch.model_matrix;

    for (size_t i = 0; i < batch.bounds.size(); ++i) {
        glm::vec4 const & rect_bounds = batch.bounds[i];
        float const x = rect_bounds.x;
        float const y = rect_bounds.y;
        float const width = rect_bounds.z;
        float const height = rect_bounds.w;

        // Determine color for this rectangle
        glm::vec4 color{1.0f, 1.0f, 1.0f, 0.5f};
        if (!batch.colors.empty() && i < batch.colors.size()) {
            color = batch.colors[i];
        }
        std::string const color_hex = colorToSVGHex(color);
        float const alpha = color.a;

        // Transform rectangle corners to SVG coordinates
        glm::vec4 const bottom_left(x, y, 0.0f, 1.0f);
        glm::vec4 const top_right(x + width, y + height, 0.0f, 1.0f);

        glm::vec2 const svg_bottom_left = transformVertexToSVG(bottom_left, mvp, params.canvas_width, params.canvas_height);
        glm::vec2 const svg_top_right = transformVertexToSVG(top_right, mvp, params.canvas_width, params.canvas_height);

        // Calculate SVG rectangle parameters
        float const svg_x = std::min(svg_bottom_left.x, svg_top_right.x);
        float const svg_y = std::min(svg_bottom_left.y, svg_top_right.y);
        float const svg_width = std::abs(svg_top_right.x - svg_bottom_left.x);
        float const svg_height = std::abs(svg_top_right.y - svg_bottom_left.y);

        std::ostringstream element;
        element << R"(<rect x=")" << svg_x << R"(" y=")" << svg_y
                << R"(" width=")" << svg_width << R"(" height=")" << svg_height
                << R"(" fill=")" << color_hex
                << R"(" fill-opacity=")" << alpha << R"(" stroke="none"/>)";

        elements.push_back(element.str());
    }

    return elements;
}

std::vector<std::string> renderSceneToSVG(
        RenderableScene const & scene,
        SVGExportParams const & params) {

    std::vector<std::string> elements;

    // Render rectangles first (background/intervals)
    for (auto const & batch: scene.rectangle_batches) {
        auto batch_elements = renderRectangleBatchToSVG(batch, scene.view_matrix, scene.projection_matrix, params);
        elements.insert(elements.end(), batch_elements.begin(), batch_elements.end());
    }

    // Render polylines (analog series)
    for (auto const & batch: scene.poly_line_batches) {
        auto batch_elements = renderPolyLineBatchToSVG(batch, scene.view_matrix, scene.projection_matrix, params);
        elements.insert(elements.end(), batch_elements.begin(), batch_elements.end());
    }

    // Render glyphs last (events on top)
    for (auto const & batch: scene.glyph_batches) {
        auto batch_elements = renderGlyphBatchToSVG(batch, scene.view_matrix, scene.projection_matrix, params);
        elements.insert(elements.end(), batch_elements.begin(), batch_elements.end());
    }

    return elements;
}

std::string buildSVGDocument(
        RenderableScene const & scene,
        SVGExportParams const & params) {

    std::ostringstream doc;

    // SVG header
    doc << R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)" << "\n";
    doc << R"(<svg width=")" << params.canvas_width << R"(" height=")" << params.canvas_height
        << R"(" viewBox="0 0 )" << params.canvas_width << " " << params.canvas_height
        << R"(" xmlns="http://www.w3.org/2000/svg" version="1.1">)" << "\n";
    doc << R"(  <desc>WhiskerToolbox DataViewer Export</desc>)" << "\n";
    doc << R"(  <rect width="100%" height="100%" fill=")" << params.background_color << R"("/>)" << "\n";

    // Render all elements
    auto elements = renderSceneToSVG(scene, params);
    for (auto const & element: elements) {
        doc << "  " << element << "\n";
    }

    // SVG footer
    doc << "</svg>";

    return doc.str();
}

std::vector<std::string> createScalebarSVG(
        int scalebar_length,
        float time_range_start,
        float time_range_end,
        SVGExportParams const & params) {

    std::vector<std::string> elements;

    // Scalebar configuration
    int const padding = 50;   // pixels from edges
    int const bar_height = 4; // pixels thick
    int const tick_height = 8;// pixels

    // Calculate the scalebar width in pixels based on time range
    float const time_range = time_range_end - time_range_start;
    float const time_to_pixel = static_cast<float>(params.canvas_width) / time_range;
    float const bar_width_pixels = static_cast<float>(scalebar_length) * time_to_pixel;

    // Position in bottom-right corner
    float const bar_x = static_cast<float>(params.canvas_width) - bar_width_pixels - static_cast<float>(padding);
    float const bar_y = static_cast<float>(params.canvas_height) - static_cast<float>(padding);

    // Main horizontal bar
    std::ostringstream bar;
    bar << R"(<line x1=")" << bar_x << R"(" y1=")" << bar_y
        << R"(" x2=")" << (bar_x + bar_width_pixels) << R"(" y2=")" << bar_y
        << R"(" stroke="#000000" stroke-width=")" << bar_height << R"(" stroke-linecap="butt"/>)";
    elements.push_back(bar.str());

    // Left tick
    std::ostringstream left_tick;
    left_tick << R"(<line x1=")" << bar_x << R"(" y1=")" << (bar_y - tick_height / 2)
              << R"(" x2=")" << bar_x << R"(" y2=")" << (bar_y + tick_height / 2)
              << R"(" stroke="#000000" stroke-width="2"/>)";
    elements.push_back(left_tick.str());

    // Right tick
    std::ostringstream right_tick;
    right_tick << R"(<line x1=")" << (bar_x + bar_width_pixels) << R"(" y1=")" << (bar_y - tick_height / 2)
               << R"(" x2=")" << (bar_x + bar_width_pixels) << R"(" y2=")" << (bar_y + tick_height / 2)
               << R"(" stroke="#000000" stroke-width="2"/>)";
    elements.push_back(right_tick.str());

    // Label
    float const label_x = bar_x + bar_width_pixels / 2.0f;
    float const label_y = bar_y - 10.0f;
    std::ostringstream label;
    label << R"(<text x=")" << label_x << R"(" y=")" << label_y
          << R"(" font-family="Arial, sans-serif" font-size="14" fill="#000000" text-anchor="middle">)"
          << scalebar_length << R"(</text>)";
    elements.push_back(label.str());

    return elements;
}

}// namespace CorePlotting
