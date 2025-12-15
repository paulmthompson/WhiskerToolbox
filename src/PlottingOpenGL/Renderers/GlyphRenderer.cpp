#include "GlyphRenderer.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace PlottingOpenGL {

GlyphRenderer::GlyphRenderer() = default;

GlyphRenderer::~GlyphRenderer() {
    cleanup();
}

bool GlyphRenderer::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!GLFunctions::hasCurrentContext()) {
        return false;
    }

    // Compile shaders (use point-based shaders for circle glyphs)
    if (!compileShaders()) {
        return false;
    }

    // Create VAO and VBOs
    if (!m_vao.create()) {
        return false;
    }
    if (!m_geometry_vbo.create()) {
        m_vao.destroy();
        return false;
    }
    if (!m_instance_vbo.create()) {
        m_geometry_vbo.destroy();
        m_vao.destroy();
        return false;
    }
    if (!m_color_vbo.create()) {
        m_instance_vbo.destroy();
        m_geometry_vbo.destroy();
        m_vao.destroy();
        return false;
    }

    m_initialized = true;
    return true;
}

void GlyphRenderer::cleanup() {
    m_color_vbo.destroy();
    m_instance_vbo.destroy();
    m_geometry_vbo.destroy();
    m_vao.destroy();
    m_shader.destroy();
    m_initialized = false;
    clearData();
}

bool GlyphRenderer::isInitialized() const {
    return m_initialized;
}

void GlyphRenderer::render(glm::mat4 const & view_matrix,
                           glm::mat4 const & projection_matrix) {
    if (!m_initialized || m_positions.empty()) {
        return;
    }

    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    // Compute MVP = Projection * View * Model
    glm::mat4 mvp = projection_matrix * view_matrix * m_model_matrix;

    // Bind shader and set uniforms
    if (!m_shader.bind()) {
        return;
    }
    m_shader.setUniformMatrix4("uMVP", glm::value_ptr(mvp));
    m_shader.setUniformValue("uGlyphSize", m_glyph_size);

    // Bind VAO
    if (!m_vao.bind()) {
        m_shader.release();
        return;
    }

    // Enable point size program control for circle glyphs
    if (m_glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle) {
        gl->glEnable(GL_PROGRAM_POINT_SIZE);
    }

    // Draw based on glyph type
    int const instance_count = static_cast<int>(m_positions.size());

    switch (m_glyph_type) {
        case CorePlotting::RenderableGlyphBatch::GlyphType::Circle:
            // Draw as points (one point per instance)
            gl->glDrawArrays(GL_POINTS, 0, instance_count);
            break;

        case CorePlotting::RenderableGlyphBatch::GlyphType::Tick:
            // Draw vertical lines (2 vertices per tick, instanced)
            glExtra->glDrawArraysInstanced(GL_LINES, 0, m_glyph_vertex_count, instance_count);
            break;

        case CorePlotting::RenderableGlyphBatch::GlyphType::Square:
            // Draw quads as triangle strip (4 vertices)
            glExtra->glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, m_glyph_vertex_count, instance_count);
            break;

        case CorePlotting::RenderableGlyphBatch::GlyphType::Cross:
            // Draw two perpendicular lines (4 vertices)
            glExtra->glDrawArraysInstanced(GL_LINES, 0, m_glyph_vertex_count, instance_count);
            break;
    }

    if (m_glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle) {
        gl->glDisable(GL_PROGRAM_POINT_SIZE);
    }

    // Cleanup
    m_vao.release();
    m_shader.release();
}

bool GlyphRenderer::hasData() const {
    return !m_positions.empty();
}

void GlyphRenderer::clearData() {
    m_positions.clear();
    m_colors.clear();
    m_glyph_vertices.clear();
    m_glyph_vertex_count = 0;
    m_has_per_instance_colors = false;
    m_model_matrix = glm::mat4{1.0f};
}

void GlyphRenderer::uploadData(CorePlotting::RenderableGlyphBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    clearData();

    if (batch.positions.empty()) {
        return;
    }

    // Store batch properties
    m_glyph_type = batch.glyph_type;
    m_glyph_size = batch.size;
    m_model_matrix = batch.model_matrix;
    m_positions = batch.positions;
    m_has_per_instance_colors = !batch.colors.empty();

    // Prepare colors - if no per-instance colors, create a buffer with global color
    if (m_has_per_instance_colors) {
        m_colors = batch.colors;
    } else {
        // Create uniform color array for all instances
        m_colors.resize(batch.positions.size(), glm::vec4{1.0f, 1.0f, 1.0f, 1.0f});
    }

    // Create glyph geometry based on type
    createGlyphGeometry();

    // Setup vertex attributes based on glyph type
    setupVertexAttributes();
}

void GlyphRenderer::setGlyphSize(float size) {
    m_glyph_size = size;
}

bool GlyphRenderer::compileShaders() {
    // Use point-based shaders that work for both point sprites and instanced geometry
    return m_shader.createFromSource(GlyphShaders::POINT_VERTEX_SHADER,
                                     GlyphShaders::POINT_FRAGMENT_SHADER);
}

void GlyphRenderer::createGlyphGeometry() {
    m_glyph_vertices.clear();

    // Glyph shapes are defined in normalized coordinates [-0.5, 0.5]
    // They will be scaled by uGlyphSize in the shader

    switch (m_glyph_type) {
        case CorePlotting::RenderableGlyphBatch::GlyphType::Circle:
            // For circles, we use GL_POINTS, no geometry needed
            m_glyph_vertex_count = 1;
            break;

        case CorePlotting::RenderableGlyphBatch::GlyphType::Tick:
            // Vertical line from -0.5 to 0.5
            m_glyph_vertices = {
                    0.0f, -0.5f,// Bottom
                    0.0f, 0.5f  // Top
            };
            m_glyph_vertex_count = 2;
            break;

        case CorePlotting::RenderableGlyphBatch::GlyphType::Square:
            // Quad as triangle strip
            m_glyph_vertices = {
                    -0.5f, -0.5f,// Bottom-left
                    0.5f, -0.5f, // Bottom-right
                    -0.5f, 0.5f, // Top-left
                    0.5f, 0.5f   // Top-right
            };
            m_glyph_vertex_count = 4;
            break;

        case CorePlotting::RenderableGlyphBatch::GlyphType::Cross:
            // Two perpendicular lines
            m_glyph_vertices = {
                    // Horizontal line
                    -0.5f, 0.0f,
                    0.5f, 0.0f,
                    // Vertical line
                    0.0f, -0.5f,
                    0.0f, 0.5f};
            m_glyph_vertex_count = 4;
            break;
    }
}

void GlyphRenderer::setupVertexAttributes() {
    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    (void) m_vao.bind();

    if (m_glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle) {
        // For circle (point) glyphs, use position directly as attribute 0
        // and color as attribute 1 (interleaved in same buffer or separate buffers)

        // Upload positions to instance VBO
        (void) m_instance_vbo.bind();
        m_instance_vbo.allocate(m_positions.data(),
                                static_cast<int>(m_positions.size() * sizeof(glm::vec2)));

        // Position attribute (location = 0)
        gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
        gl->glEnableVertexAttribArray(0);
        m_instance_vbo.release();

        // Upload colors to color VBO
        (void) m_color_vbo.bind();
        m_color_vbo.allocate(m_colors.data(),
                             static_cast<int>(m_colors.size() * sizeof(glm::vec4)));

        // Color attribute (location = 1)
        gl->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
        gl->glEnableVertexAttribArray(1);
        m_color_vbo.release();

    } else {
        // For other glyphs, use instanced rendering

        // Upload glyph geometry to geometry VBO
        if (!m_glyph_vertices.empty()) {
            (void) m_geometry_vbo.bind();
            m_geometry_vbo.allocate(m_glyph_vertices.data(),
                                    static_cast<int>(m_glyph_vertices.size() * sizeof(float)));

            // Vertex attribute (location = 0) - per-vertex shape
            gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            gl->glEnableVertexAttribArray(0);
            m_geometry_vbo.release();
        }

        // Upload instance positions
        (void) m_instance_vbo.bind();
        m_instance_vbo.allocate(m_positions.data(),
                                static_cast<int>(m_positions.size() * sizeof(glm::vec2)));

        // Instance position attribute (location = 1) - per-instance
        gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
        gl->glEnableVertexAttribArray(1);
        glExtra->glVertexAttribDivisor(1, 1);// Advance once per instance
        m_instance_vbo.release();

        // Upload instance colors
        (void) m_color_vbo.bind();
        m_color_vbo.allocate(m_colors.data(),
                             static_cast<int>(m_colors.size() * sizeof(glm::vec4)));

        // Instance color attribute (location = 2) - per-instance
        gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
        gl->glEnableVertexAttribArray(2);
        glExtra->glVertexAttribDivisor(2, 1);// Advance once per instance
        m_color_vbo.release();
    }

    m_vao.release();
}

}// namespace PlottingOpenGL
