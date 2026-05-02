/**
 * @file SVGGlyphRenderer.cpp
 * @brief `RenderableGlyphBatch` → tick / top line / circle / square / cross SVG primitives.
 */

#include "PlottingSVG/Renderers/SVGGlyphRenderer.hpp"

#include "PlottingSVG/SVGUtils.hpp"

#include <algorithm>
#include <cmath>
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
                // Tick extends ±size/2 from the glyph center in world-space Y,
                // matching the OpenGL instanced shader: world_pos = geometry * u_glyph_size + instance_pos.
                float const half_size = batch.size / 2.0f;
                glm::vec4 const bottom_vertex(pos.x, pos.y - half_size, 0.0f, 1.0f);
                glm::vec4 const top_vertex(pos.x, pos.y + half_size, 0.0f, 1.0f);
                glm::vec2 const svg_bottom =
                        transformVertexToSVG(bottom_vertex, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_top =
                        transformVertexToSVG(top_vertex, mvp, canvas_width, canvas_height);

                element << R"(<line x1=")" << svg_bottom.x << R"(" y1=")" << svg_bottom.y
                        << R"(" x2=")" << svg_top.x << R"(" y2=")" << svg_top.y
                        << R"(" stroke=")" << color_hex
                        << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::TopLine: {
                // Horizontal segment through the glyph center; ±size/2 along world X.
                float const half_size = batch.size / 2.0f;
                glm::vec4 const left_vertex(pos.x - half_size, pos.y, 0.0f, 1.0f);
                glm::vec4 const right_vertex(pos.x + half_size, pos.y, 0.0f, 1.0f);
                glm::vec2 const svg_left =
                        transformVertexToSVG(left_vertex, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_right =
                        transformVertexToSVG(right_vertex, mvp, canvas_width, canvas_height);

                element << R"(<line x1=")" << svg_left.x << R"(" y1=")" << svg_left.y
                        << R"(" x2=")" << svg_right.x << R"(" y2=")" << svg_right.y
                        << R"(" stroke=")" << color_hex
                        << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::Circle: {
                // Circle size is in screen pixels (set by widget as _widget_height / num_trials).
                element << R"(<circle cx=")" << svg_pos.x << R"(" cy=")" << svg_pos.y
                        << R"(" r=")" << batch.size / 2.0f << R"(" fill=")" << color_hex
                        << R"(" fill-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::Square: {
                // Square extends ±size/2 in both X and Y in world space.
                float const half_size = batch.size / 2.0f;
                glm::vec4 const tl_vertex(pos.x - half_size, pos.y + half_size, 0.0f, 1.0f);
                glm::vec4 const br_vertex(pos.x + half_size, pos.y - half_size, 0.0f, 1.0f);
                glm::vec2 const svg_tl =
                        transformVertexToSVG(tl_vertex, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_br =
                        transformVertexToSVG(br_vertex, mvp, canvas_width, canvas_height);
                float const w = std::abs(svg_br.x - svg_tl.x);
                float const h = std::abs(svg_br.y - svg_tl.y);
                float const min_x = std::min(svg_tl.x, svg_br.x);
                float const min_y = std::min(svg_tl.y, svg_br.y);
                element << R"(<rect x=")" << min_x << R"(" y=")" << min_y << R"(" width=")" << w
                        << R"(" height=")" << h << R"(" fill=")" << color_hex
                        << R"(" fill-opacity=")" << alpha << R"("/>)";
                break;
            }
            case CorePlotting::RenderableGlyphBatch::GlyphType::Cross: {
                // Cross: two lines through center, each ±size/2 in world space.
                float const half_size = batch.size / 2.0f;
                glm::vec4 const left_v(pos.x - half_size, pos.y, 0.0f, 1.0f);
                glm::vec4 const right_v(pos.x + half_size, pos.y, 0.0f, 1.0f);
                glm::vec4 const bot_v(pos.x, pos.y - half_size, 0.0f, 1.0f);
                glm::vec4 const top_v(pos.x, pos.y + half_size, 0.0f, 1.0f);
                glm::vec2 const svg_left =
                        transformVertexToSVG(left_v, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_right =
                        transformVertexToSVG(right_v, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_bot =
                        transformVertexToSVG(bot_v, mvp, canvas_width, canvas_height);
                glm::vec2 const svg_top =
                        transformVertexToSVG(top_v, mvp, canvas_width, canvas_height);

                element << R"(<line x1=")" << svg_left.x << R"(" y1=")" << svg_left.y
                        << R"(" x2=")" << svg_right.x << R"(" y2=")" << svg_right.y
                        << R"(" stroke=")" << color_hex
                        << R"(" stroke-width="1" stroke-opacity=")" << alpha << R"("/>)";
                elements.push_back(element.str());

                std::ostringstream element2;
                element2 << R"(<line x1=")" << svg_bot.x << R"(" y1=")" << svg_bot.y
                         << R"(" x2=")" << svg_top.x << R"(" y2=")" << svg_top.y
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
