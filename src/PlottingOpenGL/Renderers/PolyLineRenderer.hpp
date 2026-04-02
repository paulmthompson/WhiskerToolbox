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
     * Appends the batch's vertex data to an internal combined buffer and
     * immediately re-uploads the entire buffer to the VBO via `glBufferData`.
     * Batch metadata (topology, colors, model matrix) is stored CPU-side.
     *
     * Each call **appends** — call clearData() between frames. Because the
     * full combined vertex buffer is re-allocated and re-uploaded on every call,
     * repeated calls within a frame are O(N²) in total bytes uploaded to the GPU.
     *
     * An empty `batch.vertices` is handled gracefully (early return).
     * Per-line colors (`batch.colors`) whose size does not match
     * `batch.line_start_indices.size()` fall back silently to the global color in
     * render() — no precondition is required for that field.
     *
     * The batch can be modified or destroyed after this call without affecting
     * the renderer.
     *
     * @param batch The polyline batch to upload
     *
     * @pre initialize() must have been called and returned true before the first call
     *      (enforcement: runtime_check — early return if !m_initialized)
     * @pre A valid OpenGL context must be current on the calling thread
     *      (enforcement: none) [CRITICAL]
     *      — m_vbo.allocate() is called immediately (not deferred); unlike
     *      RectangleRenderer, GPU upload happens at uploadData time.
     * @pre batch.vertices.size() % 2 == 0 (vertex buffer must contain complete xy pairs)
     *      (enforcement: none) [IMPORTANT]
     *      — an odd count silently drops the trailing float; batch.total_vertices is
     *      computed as vertices.size() / 2, so the partial vertex is ignored but the
     *      discarded float is still uploaded to the VBO.
     * @pre batch.line_start_indices.size() == batch.line_vertex_counts.size()
     *      (topology arrays must be parallel) (enforcement: none) [IMPORTANT]
     *      — both are iterated in parallel in render() without a size guard.
     * @pre For each i: batch.line_start_indices[i] + batch.line_vertex_counts[i]
     *      <= batch.vertices.size() / 2 (no index exceeds the vertex buffer)
     *      (enforcement: none) [CRITICAL]
     *      — render() passes these directly to glDrawArrays; an out-of-range index
     *      causes the GPU to read beyond the VBO (undefined driver behaviour).
     * @pre batch.thickness > 0 (required by glLineWidth in render())
     *      (enforcement: none) [LOW]
     * @pre m_all_vertices.size() * sizeof(float) + batch.vertices.size() * sizeof(float)
     *      <= INT_MAX (narrowing cast to int for m_vbo.allocate())
     *      (enforcement: none) [LOW]
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

    // Cached batch data for rendering - supports multiple batches
    struct BatchData {
        std::vector<int32_t> line_start_indices;
        std::vector<int32_t> line_vertex_counts;
        std::vector<glm::vec4> line_colors;
        glm::vec4 global_color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::mat4 model_matrix{1.0f};
        float thickness{1.0f};
        int vertex_offset{0};  // Offset in VBO for this batch
        int total_vertices{0};
        bool has_per_line_colors{false};
    };
    std::vector<BatchData> m_batches;
    std::vector<float> m_all_vertices;  // Combined vertex data for all batches
    int m_total_vertices{0};
    
    // Default line thickness (used by setLineThickness API for backwards compatibility)
    float m_thickness{1.0f};

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
