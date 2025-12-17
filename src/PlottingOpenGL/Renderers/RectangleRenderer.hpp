#ifndef PLOTTINGOPENGL_RENDERERS_RECTANGLERENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_RECTANGLERENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <string>
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
 * Shader Loading:
 *   - By default, uses embedded shaders (no external shader file equivalent)
 *   - Can optionally use ShaderManager if shader_base_path is provided
 *   - Shader program names: "rectangle_fill_renderer", "rectangle_border_renderer"
 * 
 * The rectangle bounds are stored as glm::vec4(x, y, width, height) where
 * (x, y) is the bottom-left corner in world space.
 * 
 * @see CorePlotting::RenderableRectangleBatch
 */
class RectangleRenderer : public IBatchRenderer {
public:
    /**
     * @brief Construct a RectangleRenderer with optional shader paths.
     * 
     * @param shader_base_path Base path to shader directory (e.g., "src/WhiskerToolbox/shaders/")
     *                         If empty, uses embedded fallback shaders.
     */
    explicit RectangleRenderer(std::string shader_base_path = "");
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

    /**
     * @brief Check if using ShaderManager (vs embedded fallback).
     */
    [[nodiscard]] bool isUsingShaderManager() const { return m_use_shader_manager; }

private:
    bool loadShadersFromManager();
    bool compileEmbeddedShaders();
    void setupVertexAttributes();
    void createQuadGeometry();

    std::string m_shader_base_path;
    bool m_use_shader_manager{false};
    
    // Only used when not using ShaderManager
    GLShaderProgram m_embedded_fill_shader;
    GLShaderProgram m_embedded_border_shader;
    
    GLVertexArray m_vao;
    GLBuffer m_quad_vbo{GLBuffer::Type::Vertex};  // Unit quad vertices
    GLBuffer m_bounds_vbo{GLBuffer::Type::Vertex};// Per-instance bounds
    GLBuffer m_color_vbo{GLBuffer::Type::Vertex}; // Per-instance colors

    // Multi-batch support
    struct BatchData {
        std::vector<glm::vec4> bounds;  // {x, y, width, height}
        std::vector<glm::vec4> colors;
        glm::mat4 model_matrix{1.0f};
    };
    std::vector<BatchData> m_batches;

    // Border settings
    bool m_border_enabled{false};
    glm::vec4 m_border_color{0.0f, 0.0f, 0.0f, 1.0f};
    float m_border_width{1.0f};

    bool m_initialized{false};
    
    // Shader program names for ShaderManager
    static constexpr char const * FILL_SHADER_NAME = "rectangle_fill_renderer";
    static constexpr char const * BORDER_SHADER_NAME = "rectangle_border_renderer";
};

/**
 * @brief Embedded fallback shader source code for the rectangle renderer.
 */
namespace RectangleShaders {

constexpr char const * VERTEX_SHADER = R"(
#version 410 core

// Per-vertex attributes (unit quad: [0,1] x [0,1])
layout(location = 0) in vec2 a_vertex;

// Per-instance attributes
layout(location = 1) in vec4 a_bounds;  // x, y, width, height
layout(location = 2) in vec4 a_color;

uniform mat4 u_mvp_matrix;

out vec4 v_color;

void main() {
    // Scale and translate unit quad to rectangle bounds
    // a_bounds.xy = bottom-left corner, a_bounds.zw = width, height
    vec2 worldPos = a_bounds.xy + a_vertex * a_bounds.zw;
    gl_Position = u_mvp_matrix * vec4(worldPos, 0.0, 1.0);
    v_color = a_color;
}
)";

constexpr char const * FRAGMENT_SHADER = R"(
#version 410 core

in vec4 v_color;

out vec4 FragColor;

void main() {
    FragColor = v_color;
}
)";

/**
 * @brief Border shader (draws outline using GL_LINE_LOOP)
 */
constexpr char const * BORDER_VERTEX_SHADER = R"(
#version 410 core

// Per-vertex attributes (unit quad corners)
layout(location = 0) in vec2 a_vertex;

// Per-instance attributes
layout(location = 1) in vec4 a_bounds;

uniform mat4 u_mvp_matrix;

void main() {
    vec2 worldPos = a_bounds.xy + a_vertex * a_bounds.zw;
    gl_Position = u_mvp_matrix * vec4(worldPos, 0.0, 1.0);
}
)";

constexpr char const * BORDER_FRAGMENT_SHADER = R"(
#version 410 core

uniform vec4 u_border_color;

out vec4 FragColor;

void main() {
    FragColor = u_border_color;
}
)";

}// namespace RectangleShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_RECTANGLERENDERER_HPP
