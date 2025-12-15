#ifndef PLOTTINGOPENGL_RENDERERS_GLYPHRENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_GLYPHRENDERER_HPP

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

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
    GlyphRenderer();
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

private:
    bool compileShaders();
    void setupVertexAttributes();
    void createGlyphGeometry();

    GLShaderProgram m_shader;
    GLVertexArray m_vao;
    GLBuffer m_geometry_vbo{GLBuffer::Type::Vertex};// Glyph shape vertices
    GLBuffer m_instance_vbo{GLBuffer::Type::Vertex};// Per-instance positions
    GLBuffer m_color_vbo{GLBuffer::Type::Vertex};   // Per-instance colors

    // Glyph shape data
    std::vector<float> m_glyph_vertices;
    int m_glyph_vertex_count{0};

    // Instance data
    std::vector<glm::vec2> m_positions;
    std::vector<glm::vec4> m_colors;
    glm::mat4 m_model_matrix{1.0f};
    float m_glyph_size{5.0f};
    CorePlotting::RenderableGlyphBatch::GlyphType m_glyph_type{
            CorePlotting::RenderableGlyphBatch::GlyphType::Circle};
    bool m_has_per_instance_colors{false};
    glm::vec4 m_global_color{1.0f, 1.0f, 1.0f, 1.0f};

    bool m_initialized{false};
};

/**
 * @brief Shader source code for the glyph renderer.
 * 
 * Uses instanced rendering with per-instance position offset.
 */
namespace GlyphShaders {

constexpr char const * VERTEX_SHADER = R"(
#version 330 core

// Per-vertex attributes (glyph shape)
layout(location = 0) in vec2 aVertex;

// Per-instance attributes
layout(location = 1) in vec2 aInstancePosition;
layout(location = 2) in vec4 aInstanceColor;

uniform mat4 uMVP;
uniform float uGlyphSize;
uniform int uHasPerInstanceColors;

out vec4 vColor;

void main() {
    // Scale the glyph shape and offset by instance position
    vec2 worldPos = aInstancePosition + aVertex * uGlyphSize;
    gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);
    
    // For GL_POINTS (circle glyph), set point size
    gl_PointSize = uGlyphSize;
    
    // Pass color to fragment shader
    vColor = aInstanceColor;
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
 * @brief Point-based shader for circle glyphs.
 * 
 * Uses gl_PointCoord to create smooth circles from point sprites.
 */
constexpr char const * POINT_VERTEX_SHADER = R"(
#version 330 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uMVP;
uniform float uGlyphSize;

out vec4 vColor;

void main() {
    gl_Position = uMVP * vec4(aPosition, 0.0, 1.0);
    gl_PointSize = uGlyphSize;
    vColor = aColor;
}
)";

constexpr char const * POINT_FRAGMENT_SHADER = R"(
#version 330 core

in vec4 vColor;

out vec4 fragColor;

void main() {
    // Create circular point by discarding fragments outside circle
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) {
        discard;
    }
    
    // Smooth edge
    float alpha = 1.0 - smoothstep(0.4, 0.5, dist);
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)";

}// namespace GlyphShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_GLYPHRENDERER_HPP
