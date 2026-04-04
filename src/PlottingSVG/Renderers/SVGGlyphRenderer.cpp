/**
 * @file SVGGlyphRenderer.cpp
 * @brief `RenderableGlyphBatch` → tick / circle / square / cross SVG primitives.
 */

#include "PlottingSVG/Renderers/SVGGlyphRenderer.hpp"

#include "PlottingSVG/SVGUtils.hpp"

#include <sstream>

namespace PlottingSVG {

std::vector<std::string>
SVGGlyphRenderer::render(CorePlotting::RenderableGlyphBatch const & batch,
                         glm::mat4 const & view,
                         glm::mat4 const & projection,
                         int canvas_width,
                         int canvas_height) {
    std::vector<std::string> elements;

    if (batch.positions.empty()) {
        return elements;
    }

    glm::mat4 const mvp = projection * view * batch.model_matrix;

    for (size_t i = 0; i < batch.positions.size(); ++i) {
        glm::vec2 const & pos = batch.positions[i];

        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        if (!batch.colors.empty() && i < batch.colors.size()) {
            color = batch.colors[i];
        }
        std::string const color_hex = colorToSVGHex(color);
        float const alpha = color.a;

        glm::vec4 const vertex(pos.x, pos.y, 0.0f, 1.0f);
        glm::vec2 const svg_pos =
                transformVertexToSVG(vertex, mvp, canvas_width, canvas_height);

        std::ostringstream element;

        switch (batch.glyph_type) {
            case CorePlotting::RenderableGlyphBatch::GlyphType::Tick: {
                glm::vec4 const bottom_vertex(pos.x, -1.0f, 0.0f, 1.0f);
                glm::vec4 const top_vertex(pos.x, 1.0f, 0.0f, 1.0f);
                glm::vec2 const svg_bottom =
                        transformVertexToSVG(bottom_vertex, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_top =
                        transformVertexToSVG(top_vertex, mvp, canvas_width, canvas_height);

                element << R"(<line x1=")" << svg_bottom.x << R"(" y1=")" << svg_bottom.y
                        << R"(" x2=")" << svg_top.x << R"(" y2=")" << svg_top.y
                        << R"(" stroke=")" << color_hex << R"(" stroke-width=")" << batch.size
                        << R"(" stroke-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::Circle: {
                element << R"(<circle cx=")" << svg_pos.x << R"(" cy=")" << svg_pos.y
                        << R"(" r=")" << batch.size / 2.0f << R"(" fill=")" << color_hex
                        << R"(" fill-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::Square: {
                float const half_size = batch.size / 2.0f;
                element << R"(<rect x=")" << (svg_pos.x - half_size) << R"(" y=")"
                        << (svg_pos.y - half_size) << R"(" width=")" << batch.size
                        << R"(" height=")" << batch.size << R"(" fill=")" << color_hex
                        << R"(" fill-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::Cross: {
                float const half_size = batch.size / 2.0f;
                element << R"(<line x1=")" << (svg_pos.x - half_size) << R"(" y1=")" << svg_pos.y
                        << R"(" x2=")" << (svg_pos.x + half_size) << R"(" y2=")" << svg_pos.y
                        << R"(" stroke=")" << color_hex
                        << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                elements.push_back(element.str());

                std::ostringstream element2;
                element2 << R"(<line x1=")" << svg_pos.x << R"(" y1=")" << (svg_pos.y - half_size)
                         << R"(" x2=")" << svg_pos.x << R"(" y2=")" << (svg_pos.y + half_size)
                         << R"(" stroke=")" << color_hex
                         << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                elements.push_back(element2.str());
                continue;
            }
        }

        elements.push_back(element.str());
    }

    return elements;
}

}// namespace PlottingSVG
