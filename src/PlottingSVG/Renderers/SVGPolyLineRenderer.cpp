/**
 * @file SVGPolyLineRenderer.cpp
 * @brief `RenderablePolyLineBatch` → `<polyline>` SVG elements.
 */

#include "PlottingSVG/Renderers/SVGPolyLineRenderer.hpp"

#include "PlottingSVG/SVGUtils.hpp"

#include <sstream>

namespace PlottingSVG {

std::vector<std::string>
SVGPolyLineRenderer::render(CorePlotting::RenderablePolyLineBatch const & batch,
                            glm::mat4 const & view,
                            glm::mat4 const & projection,
                            int canvas_width,
                            int canvas_height) {
    std::vector<std::string> elements;

    if (batch.vertices.empty() || batch.line_start_indices.empty()) {
        return elements;
    }

    glm::mat4 const mvp = projection * view * batch.model_matrix;

    for (size_t line_idx = 0; line_idx < batch.line_start_indices.size(); ++line_idx) {
        int const start_index = batch.line_start_indices[line_idx];
        int const vertex_count = batch.line_vertex_counts[line_idx];

        if (vertex_count < 2) {
            continue;
        }

        glm::vec4 color = batch.global_color;
        if (!batch.colors.empty() && line_idx < batch.colors.size()) {
            color = batch.colors[line_idx];
        }
        std::string const color_hex = colorToSVGHex(color);

        std::ostringstream points;
        for (int i = 0; i < vertex_count; ++i) {
            int const vert_idx = (start_index + i) * 2;
            if (vert_idx + 1 >= static_cast<int>(batch.vertices.size())) {
                break;
            }

            float const x = batch.vertices[static_cast<size_t>(vert_idx)];
            float const y = batch.vertices[static_cast<size_t>(vert_idx) + 1U];

            glm::vec4 const vertex(x, y, 0.0f, 1.0f);
            glm::vec2 const svg_pos =
                    transformVertexToSVG(vertex, mvp, canvas_width, canvas_height);

            if (i > 0) {
                points << ' ';
            }
            points << svg_pos.x << ',' << svg_pos.y;
        }

        std::ostringstream element;
        element << R"(<polyline points=")" << points.str() << R"(" fill="none" stroke=")" << color_hex
                << R"(" stroke-width=")" << batch.thickness
                << R"(" stroke-linejoin="round" stroke-linecap="round"/>)";
        elements.push_back(element.str());
    }

    return elements;
}

}// namespace PlottingSVG
