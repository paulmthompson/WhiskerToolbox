#ifndef PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_POLYLINERENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <span>
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
     * uploads the data to the VBO via persistent buffer management (`glBufferSubData`).
     * Batch metadata (topology, colors, model matrix) is stored CPU-side.
     *
     * @param batch The polyline batch to upload
     */
    void uploadData(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Upload multiple polyline batches to GPU memory in a single combined pass.
     *
     * Pre-allocates combined buffer memory and uploads all vertex data in a single
     * GPU buffer write (using glBufferSubData when persistent capacity allows).
     *
     * @param batches Span of batches to upload
     */
    void uploadBatches(std::span<CorePlotting::RenderablePolyLineBatch const> batches);

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

    /**
     * @brief Number of bytes uploaded during the last upload call.
     */
    [[nodiscard]] size_t getLastUploadedBytes() const { return m_last_uploaded_bytes; }

    /**
     * @brief Whether the last upload was fulfilled via incremental sub-buffer updates.
     */
    [[nodiscard]] bool wasLastUploadIncremental() const { return m_was_last_upload_incremental; }

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

    // Persistent GPU buffer allocation in bytes
    size_t m_gpu_buffer_capacity{0};
    size_t m_last_uploaded_bytes{0};
    bool m_was_last_upload_incremental{false};

    // Cached batch data for rendering - supports multiple batches
    struct BatchData {
        std::vector<int32_t> line_start_indices;
        std::vector<int32_t> line_vertex_counts;
        std::vector<glm::vec4> line_colors;
        glm::vec4 global_color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::mat4 model_matrix{1.0f};
        float thickness{1.0f};
        int32_t view_start_time{0};
        bool is_integer_time{false};
        int vertex_offset{0};// Offset in VBO for this batch
        int total_vertices{0};
        bool has_per_line_colors{false};
    };
    std::vector<BatchData> m_batches;
    std::vector<float> m_all_vertices;// Combined vertex data for all batches
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

layout(location = 0) in int a_time_index;
layout(location = 1) in float a_value;

uniform int u_view_start_sample;
uniform mat4 u_mvp_matrix;

void main() {
    int rel_sample = a_time_index - u_view_start_sample;
    gl_Position = u_mvp_matrix * vec4(float(rel_sample), a_value, 0.0, 1.0);
}
)";

constexpr char const * GEOMETRY_SHADER = R"(
#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float u_line_width;
uniform vec2 u_viewport_size;

void main() {
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;

    vec2 sp0 = p0 * u_viewport_size * 0.5;
    vec2 sp1 = p1 * u_viewport_size * 0.5;

    vec2 dir = sp1 - sp0;
    float len = length(dir);
    if (len < 0.001) return;
    dir /= len;
    vec2 perp = vec2(-dir.y, dir.x);

    vec2 offset_ndc = perp * u_line_width / u_viewport_size;

    gl_Position = vec4(p0 + offset_ndc, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p0 - offset_ndc, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p1 + offset_ndc, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p1 - offset_ndc, 0.0, 1.0);
    EmitVertex();
    EndPrimitive();
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
