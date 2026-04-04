/**
 * @file SVGRectangleRenderer.cpp
 * @brief `RenderableRectangleBatch` → `<rect>` SVG elements.
 */

#include "PlottingSVG/Renderers/SVGRectangleRenderer.hpp"

#include "PlottingSVG/SVGUtils.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace PlottingSVG {

std::vector<std::string>
SVGRectangleRenderer::render(CorePlotting::RenderableRectangleBatch const & batch,
                             glm::mat4 const & view,
                             glm::mat4 const & projection,
                             int canvas_width,
                             int canvas_height) {
    std::vector<std::string> elements;

    if (batch.bounds.empty()) {
        return elements;
    }

    glm::mat4 const mvp = projection * view * batch.model_matrix;

    for (size_t i = 0; i < batch.bounds.size(); ++i) {
        glm::vec4 const & rect_bounds = batch.bounds[i];
        float const x = rect_bounds.x;
        float const y = rect_bounds.y;
        float const width = rect_bounds.z;
        float const height = rect_bounds.w;

        glm::vec4 color{1.0f, 1.0f, 1.0f, 0.5f};
        if (!batch.colors.empty() && i < batch.colors.size()) {
            color = batch.colors[i];
        }
        std::string const color_hex = colorToSVGHex(color);
        float const alpha = color.a;

        glm::vec4 const bottom_left(x, y, 0.0f, 1.0f);
        glm::vec4 const top_right(x + width, y + height, 0.0f, 1.0f);

        glm::vec2 const svg_bottom_left =
                transformVertexToSVG(bottom_left, mvp, canvas_width, canvas_height);
        glm::vec2 const svg_top_right =
                transformVertexToSVG(top_right, mvp, canvas_width, canvas_height);

        float const svg_x = std::min(svg_bottom_left.x, svg_top_right.x);
        float const svg_y = std::min(svg_bottom_left.y, svg_top_right.y);
        float const svg_width = std::abs(svg_top_right.x - svg_bottom_left.x);
        float const svg_height = std::abs(svg_top_right.y - svg_bottom_left.y);

        std::ostringstream element;
        element << R"(<rect x=")" << svg_x << R"(" y=")" << svg_y << R"(" width=")" << svg_width
                << R"(" height=")" << svg_height << R"(" fill=")" << color_hex
                << R"(" fill-opacity=")" << alpha << R"(" stroke="none"/>)";
        elements.push_back(element.str());
    }

    return elements;
}

}// namespace PlottingSVG
