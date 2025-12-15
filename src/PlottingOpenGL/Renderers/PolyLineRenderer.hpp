#ifndef PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

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
    PolyLineRenderer();
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

private:
    bool compileShaders();
    void setupVertexAttributes();

    GLShaderProgram m_shader;
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
};

/**
 * @brief Shader source code for the polyline renderer.
 * 
 * These are embedded as string literals for simplicity. For more complex
 * shader management or hot-reloading, consider using the ShaderManager.
 */
namespace PolyLineShaders {

constexpr char const * VERTEX_SHADER = R"(
#version 330 core

layout(location = 0) in vec2 aPosition;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);
}
)";

constexpr char const * FRAGMENT_SHADER = R"(
#version 330 core

uniform vec4 uColor;

out vec4 fragColor;

void main() {
    fragColor = uColor;
}
)";

}// namespace PolyLineShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP
