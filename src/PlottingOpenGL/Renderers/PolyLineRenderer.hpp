#ifndef PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace PlottingOpenGL {

/**
 * @brief Renders RenderablePolyLineBatch data using OpenGL 3.3+ compatible calls.
 * 
 * This renderer is designed for the DataViewer use case: relatively few
 * polylines (analog time series) that may have many vertices each.
 * 
 * Rendering Strategy:
 *   - Uses GL_LINE_STRIP for each polyline segment
 *   - Supports per-line colors (if provided) or a global batch color
 *   - Model matrix from the batch is combined with provided View/Projection
 * 
 * Shader Loading:
 *   - By default, uses ShaderManager with shaders from WhiskerToolbox/shaders/
 *   - Can fall back to embedded shaders if shader files are not available
 *   - Shader program name: "polyline_renderer"
 * 
 * For high-performance scenarios with 100,000+ short lines (e.g., raster plots),
 * consider using a ComputePolyLineRenderer with SSBOs instead.
 * 
 * Vertex Format:
 *   - Position: vec2 (x, y) in world space
 *   - Color: vec4 (r, g, b, a) - either from uniform or vertex attribute
 * 
 * @see CorePlotting::RenderablePolyLineBatch
 */
class PolyLineRenderer : public IBatchRenderer {
public:
    /**
     * @brief Construct a PolyLineRenderer with optional shader paths.
     * 
     * @param shader_base_path Base path to shader directory (e.g., "src/WhiskerToolbox/shaders/")
     *                         If empty, uses embedded fallback shaders.
     */
    explicit PolyLineRenderer(std::string shader_base_path = "");
    ~PolyLineRenderer() override;

    // IBatchRenderer interface
    [[nodiscard]] bool initialize() override;
    void cleanup() override;
    [[nodiscard]] bool isInitialized() const override;
    void render(glm::mat4 const & view_matrix,
                glm::mat4 const & projection_matrix) override;
    [[nodiscard]] bool hasData() const override;
    void clearData() override;

    /**
     * @brief Upload a polyline batch to GPU memory.
     * 
     * This copies the batch data to GPU buffers. The batch can be modified
     * or destroyed after this call without affecting the renderer.
     * 
     * @param batch The polyline batch to upload
     */
    void uploadData(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Set the line thickness for all lines in the batch.
     * 
     * Note: Line width support varies by OpenGL implementation and driver.
     * Wide lines (> 1.0) may not be supported on all systems.
     * 
     * @param thickness Line width in pixels
     */
    void setLineThickness(float thickness);

    /**
     * @brief Check if using ShaderManager (vs embedded fallback).
     */
    [[nodiscard]] bool isUsingShaderManager() const { return m_use_shader_manager; }

private:
    bool loadShadersFromManager();
    bool compileEmbeddedShaders();
    void setupVertexAttributes();

    std::string m_shader_base_path;
    bool m_use_shader_manager{false};
    
    // Only used when not using ShaderManager
    GLShaderProgram m_embedded_shader;
    
    GLVertexArray m_vao;
    GLBuffer m_vbo{GLBuffer::Type::Vertex};

    // Cached batch data for rendering
    std::vector<int32_t> m_line_start_indices;
    std::vector<int32_t> m_line_vertex_counts;
    std::vector<glm::vec4> m_line_colors;
    glm::vec4 m_global_color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::mat4 m_model_matrix{1.0f};
    float m_thickness{1.0f};
    int m_total_vertices{0};
    bool m_has_per_line_colors{false};

    bool m_initialized{false};
    
    // Shader program name for ShaderManager
    static constexpr char const * SHADER_PROGRAM_NAME = "polyline_renderer";
};

/**
 * @brief Embedded fallback shader source code for the polyline renderer.
 * 
 * These match the interface of WhiskerToolbox/shaders/line.vert and line.frag
 * but are embedded for cases where shader files are not available.
 */
namespace PolyLineShaders {

constexpr char const * VERTEX_SHADER = R"(
#version 410 core

layout(location = 0) in vec2 a_position;

uniform mat4 u_mvp_matrix;

void main() {
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
}
)";

constexpr char const * FRAGMENT_SHADER = R"(
#version 410 core

uniform vec4 u_color;

out vec4 FragColor;

void main() {
    FragColor = u_color;
}
)";

}// namespace PolyLineShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP
