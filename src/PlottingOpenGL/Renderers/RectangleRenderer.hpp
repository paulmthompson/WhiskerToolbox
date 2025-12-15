#ifndef PLOTTINGOPENGL_RENDERERS_RECTANGLERENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_RECTANGLERENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace PlottingOpenGL {

/**
 * @brief Renders RenderableRectangleBatch data using instanced rendering.
 * 
 * This renderer is designed for DigitalIntervalSeries visualization,
 * rendering filled rectangles (epochs, intervals, regions) efficiently.
 * 
 * Rendering Strategy:
 *   - Uses instanced rendering with a unit quad as base geometry
 *   - Each instance specifies position (x, y) and size (width, height)
 *   - Per-instance colors supported for different interval types
 *   - Optional border/outline rendering
 * 
 * The rectangle bounds are stored as glm::vec4(x, y, width, height) where
 * (x, y) is the bottom-left corner in world space.
 * 
 * @see CorePlotting::RenderableRectangleBatch
 */
class RectangleRenderer : public IBatchRenderer {
public:
    RectangleRenderer();
    ~RectangleRenderer() override;

    // IBatchRenderer interface
    [[nodiscard]] bool initialize() override;
    void cleanup() override;
    [[nodiscard]] bool isInitialized() const override;
    void render(glm::mat4 const & view_matrix,
                glm::mat4 const & projection_matrix) override;
    [[nodiscard]] bool hasData() const override;
    void clearData() override;

    /**
     * @brief Upload a rectangle batch to GPU memory.
     * 
     * @param batch The rectangle batch to upload
     */
    void uploadData(CorePlotting::RenderableRectangleBatch const & batch);

    /**
     * @brief Enable or disable border rendering.
     * 
     * When enabled, rectangles are drawn with an outline in addition
     * to the fill color.
     * 
     * @param enabled Whether to draw borders
     */
    void setBorderEnabled(bool enabled);

    /**
     * @brief Set the border color for all rectangles.
     * 
     * @param color Border color (RGBA)
     */
    void setBorderColor(glm::vec4 const & color);

    /**
     * @brief Set the border width in pixels.
     * 
     * @param width Border width
     */
    void setBorderWidth(float width);

private:
    bool compileShaders();
    void setupVertexAttributes();
    void createQuadGeometry();

    GLShaderProgram m_fill_shader;
    GLShaderProgram m_border_shader;
    GLVertexArray m_vao;
    GLBuffer m_quad_vbo{GLBuffer::Type::Vertex};  // Unit quad vertices
    GLBuffer m_bounds_vbo{GLBuffer::Type::Vertex};// Per-instance bounds
    GLBuffer m_color_vbo{GLBuffer::Type::Vertex}; // Per-instance colors

    // Instance data
    std::vector<glm::vec4> m_bounds;// {x, y, width, height}
    std::vector<glm::vec4> m_colors;
    glm::mat4 m_model_matrix{1.0f};

    // Border settings
    bool m_border_enabled{false};
    glm::vec4 m_border_color{0.0f, 0.0f, 0.0f, 1.0f};
    float m_border_width{1.0f};

    bool m_initialized{false};
};

/**
 * @brief Shader source code for the rectangle renderer.
 */
namespace RectangleShaders {

constexpr char const * VERTEX_SHADER = R"(
#version 330 core

// Per-vertex attributes (unit quad: [0,1] x [0,1])
layout(location = 0) in vec2 aVertex;

// Per-instance attributes
layout(location = 1) in vec4 aBounds;  // x, y, width, height
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec4 vColor;

void main() {
    // Scale and translate unit quad to rectangle bounds
    // aBounds.xy = bottom-left corner, aBounds.zw = width, height
    vec2 worldPos = aBounds.xy + aVertex * aBounds.zw;
    gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);
    vColor = aColor;
}
)";

constexpr char const * FRAGMENT_SHADER = R"(
#version 330 core

in vec4 vColor;

out vec4 fragColor;

void main() {
    fragColor = vColor;
}
)";

/**
 * @brief Border shader (draws outline using GL_LINE_LOOP)
 */
constexpr char const * BORDER_VERTEX_SHADER = R"(
#version 330 core

// Per-vertex attributes (unit quad corners)
layout(location = 0) in vec2 aVertex;

// Per-instance attributes
layout(location = 1) in vec4 aBounds;

uniform mat4 uMVP;

void main() {
    vec2 worldPos = aBounds.xy + aVertex * aBounds.zw;
    gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);
}
)";

constexpr char const * BORDER_FRAGMENT_SHADER = R"(
#version 330 core

uniform vec4 uBorderColor;

out vec4 fragColor;

void main() {
    fragColor = uBorderColor;
}
)";

}// namespace RectangleShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_RECTANGLERENDERER_HPP
