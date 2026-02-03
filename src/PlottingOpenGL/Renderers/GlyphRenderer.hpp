#ifndef PLOTTINGOPENGL_RENDERERS_GLYPHRENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_GLYPHRENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace PlottingOpenGL {

/**
 * @brief Renders RenderableGlyphBatch data using instanced rendering.
 * 
 * This renderer is designed for event markers, raster plot ticks, and
 * point data visualization. It uses GPU instancing to efficiently render
 * thousands of identical glyphs at different positions.
 * 
 * Rendering Strategy:
 *   - Uses instanced rendering (glDrawArraysInstanced)
 *   - Each glyph type (circle, square, tick, cross) has a predefined shape
 *   - Position and color can vary per instance
 *   - Model matrix from the batch positions the entire batch in world space
 * 
 * Shader Loading:
 *   - By default, uses ShaderManager with shaders from WhiskerToolbox/shaders/
 *   - Can fall back to embedded shaders if shader files are not available
 *   - Shader program name: "glyph_renderer"
 * 
 * Supported Glyph Types:
 *   - Circle: Point primitive (GL_POINTS) with gl_PointSize
 *   - Square: Quad rendered as two triangles
 *   - Tick: Vertical line segment
 *   - Cross: Two perpendicular line segments
 * 
 * @see CorePlotting::RenderableGlyphBatch
 */
class GlyphRenderer : public IBatchRenderer {
public:
    /**
     * @brief Construct a GlyphRenderer with optional shader paths.
     * 
     * @param shader_base_path Base path to shader directory (e.g., "src/WhiskerToolbox/shaders/")
     *                         If empty, uses embedded fallback shaders.
     */
    explicit GlyphRenderer(std::string shader_base_path = "");
    ~GlyphRenderer() override;

    // IBatchRenderer interface
    [[nodiscard]] bool initialize() override;
    void cleanup() override;
    [[nodiscard]] bool isInitialized() const override;
    void render(glm::mat4 const & view_matrix,
                glm::mat4 const & projection_matrix) override;
    [[nodiscard]] bool hasData() const override;
    void clearData() override;

    /**
     * @brief Upload a glyph batch to GPU memory.
     * 
     * @param batch The glyph batch to upload
     */
    void uploadData(CorePlotting::RenderableGlyphBatch const & batch);

    /**
     * @brief Set the glyph size in pixels (for point sprites) or world units.
     * 
     * @param size The size of each glyph
     */
    void setGlyphSize(float size);

    /**
     * @brief Check if using ShaderManager (vs embedded fallback).
     */
    [[nodiscard]] bool isUsingShaderManager() const { return m_use_shader_manager; }

private:
    bool loadShadersFromManager();
    bool compileEmbeddedShaders();
    void setupVertexAttributes();
    void setupPointVertexAttributes();  // For GL_POINTS mode
    void setupInstancedVertexAttributes();  // For instanced mode
    void createGlyphGeometry();

    std::string m_shader_base_path;
    bool m_use_shader_manager{false};
    
    // Embedded shaders: point shader for circles, instanced for other glyphs
    GLShaderProgram m_point_shader;      // For GL_POINTS (circles)
    GLShaderProgram m_instanced_shader;  // For instanced rendering (tick, square, cross)
    
    // Separate VAOs for different rendering modes
    GLVertexArray m_point_vao;       // For GL_POINTS mode
    GLVertexArray m_instanced_vao;   // For instanced rendering mode
    
    GLBuffer m_geometry_vbo{GLBuffer::Type::Vertex};// Glyph shape vertices
    GLBuffer m_instance_vbo{GLBuffer::Type::Vertex};// Per-instance positions
    GLBuffer m_color_vbo{GLBuffer::Type::Vertex};   // Per-instance colors

    // Glyph shape data
    std::vector<float> m_glyph_vertices;
    int m_glyph_vertex_count{0};

    // Multi-batch support: store per-batch instance data
    struct BatchData {
        std::vector<glm::vec2> positions;
        std::vector<glm::vec4> colors;
        glm::mat4 model_matrix{1.0f};
        float glyph_size{5.0f};
        CorePlotting::RenderableGlyphBatch::GlyphType glyph_type{
                CorePlotting::RenderableGlyphBatch::GlyphType::Circle};
        bool has_per_instance_colors{false};
    };
    std::vector<BatchData> m_batches;

    // Combined instance data for GPU upload
    std::vector<glm::vec2> m_all_positions;
    std::vector<glm::vec4> m_all_colors;
    
    // Default glyph settings (for setGlyphSize API and geometry creation)
    float m_glyph_size{5.0f};
    CorePlotting::RenderableGlyphBatch::GlyphType m_current_glyph_type{
            CorePlotting::RenderableGlyphBatch::GlyphType::Circle};

    bool m_initialized{false};
    
    // Shader program name for ShaderManager
    static constexpr char const * SHADER_PROGRAM_NAME = "glyph_renderer";
};

/**
 * @brief Embedded fallback shader source code for the glyph renderer.
 * 
 * These shaders support both GL_POINTS mode (for circles) and instanced
 * rendering mode (for ticks, squares, crosses).
 * 
 * Attribute layout for instanced rendering:
 *   - location 0: geometry vertex (per-vertex)
 *   - location 1: instance position (per-instance)
 *   - location 2: instance color (per-instance)
 * 
 * For GL_POINTS mode, position data is uploaded to location 0 directly.
 */
namespace GlyphShaders {

/**
 * @brief Vertex shader for instanced glyph rendering (ticks, squares, crosses)
 * 
 * This shader combines per-vertex geometry with per-instance position offsets.
 */
constexpr char const * INSTANCED_VERTEX_SHADER = R"(
#version 410 core

layout (location = 0) in vec2 a_geometry;       // Per-vertex glyph shape
layout (location = 1) in vec2 a_instance_pos;   // Per-instance position
layout (location = 2) in vec4 a_instance_color; // Per-instance color

uniform mat4 u_mvp_matrix;
uniform float u_glyph_size;

out vec4 v_color;

void main() {
    // Scale geometry by glyph size and add instance position
    vec2 world_pos = a_geometry * u_glyph_size + a_instance_pos;
    gl_Position = u_mvp_matrix * vec4(world_pos, 0.0, 1.0);
    v_color = a_instance_color;
}
)";

/**
 * @brief Fragment shader for instanced glyphs
 */
constexpr char const * INSTANCED_FRAGMENT_SHADER = R"(
#version 410 core

in vec4 v_color;

out vec4 FragColor;

void main() {
    FragColor = v_color;
}
)";

/**
 * @brief Vertex shader for GL_POINTS mode (circles)
 * 
 * This shader is used when rendering circle glyphs as point sprites.
 */
constexpr char const * POINT_VERTEX_SHADER = R"(
#version 410 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec4 a_color;

uniform mat4 u_mvp_matrix;
uniform float u_point_size;

out vec4 v_color;

void main() {
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
    gl_PointSize = u_point_size;
    v_color = a_color;
}
)";

constexpr char const * POINT_FRAGMENT_SHADER = R"(
#version 410 core

in vec4 v_color;

out vec4 FragColor;

void main() {
    // Create circular point by discarding fragments outside circle
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) {
        discard;
    }
    
    // Smooth edge
    float alpha = 1.0 - smoothstep(0.4, 0.5, dist);
    FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
)";

}// namespace GlyphShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_GLYPHRENDERER_HPP
