#include "GlyphRenderer.hpp"

#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>

namespace PlottingOpenGL {

GlyphRenderer::GlyphRenderer(std::string shader_base_path)
    : m_shader_base_path(std::move(shader_base_path)) {
}

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

    // For now, always use embedded shaders (ShaderManager path not implemented for instanced)
    if (!compileEmbeddedShaders()) {
        return false;
    }

    // Create VAOs for both rendering modes
    if (!m_point_vao.create()) {
        return false;
    }
    if (!m_instanced_vao.create()) {
        m_point_vao.destroy();
        return false;
    }
    
    // Create VBOs
    if (!m_geometry_vbo.create()) {
        m_instanced_vao.destroy();
        m_point_vao.destroy();
        return false;
    }
    if (!m_instance_vbo.create()) {
        m_geometry_vbo.destroy();
        m_instanced_vao.destroy();
        m_point_vao.destroy();
        return false;
    }
    if (!m_color_vbo.create()) {
        m_instance_vbo.destroy();
        m_geometry_vbo.destroy();
        m_instanced_vao.destroy();
        m_point_vao.destroy();
        return false;
    }

    // Setup vertex attributes for both VAOs
    setupPointVertexAttributes();
    setupInstancedVertexAttributes();

    m_initialized = true;
    return true;
}

void GlyphRenderer::cleanup() {
    m_color_vbo.destroy();
    m_instance_vbo.destroy();
    m_geometry_vbo.destroy();
    m_instanced_vao.destroy();
    m_point_vao.destroy();
    m_point_shader.destroy();
    m_instanced_shader.destroy();
    m_initialized = false;
    clearData();
}

bool GlyphRenderer::isInitialized() const {
    return m_initialized;
}

void GlyphRenderer::render(glm::mat4 const & view_matrix,
                           glm::mat4 const & projection_matrix) {
    if (!m_initialized || m_batches.empty()) {
        return;
    }

    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    // Track current glyph type for geometry recreation
    auto current_geometry_type = m_current_glyph_type;

    // Render each batch separately (each may have different model matrix and glyph type)
    for (auto const & batch : m_batches) {
        if (batch.positions.empty()) continue;

        // Compute MVP = Projection * View * Model (per-batch model matrix)
        glm::mat4 mvp = projection_matrix * view_matrix * batch.model_matrix;

        // Choose shader and VAO based on glyph type
        bool is_point_mode = (batch.glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle);

        if (is_point_mode) {
            // Use point shader and VAO
            if (!m_point_shader.bind()) {
                continue;
            }
            m_point_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
            m_point_shader.setUniformValue("u_point_size", batch.glyph_size);

            if (!m_point_vao.bind()) {
                m_point_shader.release();
                continue;
            }

            // For GL_POINTS, upload positions to location 0 and colors to location 1
            (void) m_instance_vbo.bind();
            m_instance_vbo.allocate(batch.positions.data(),
                                    static_cast<int>(batch.positions.size() * sizeof(glm::vec2)));
            m_instance_vbo.release();

            (void) m_color_vbo.bind();
            m_color_vbo.allocate(batch.colors.data(),
                                 static_cast<int>(batch.colors.size() * sizeof(glm::vec4)));
            m_color_vbo.release();

            gl->glEnable(GL_PROGRAM_POINT_SIZE);
            gl->glDrawArrays(GL_POINTS, 0, static_cast<int>(batch.positions.size()));
            gl->glDisable(GL_PROGRAM_POINT_SIZE);

            m_point_vao.release();
            m_point_shader.release();
        } else {
            // Use instanced shader and VAO
            if (!m_instanced_shader.bind()) {
                continue;
            }
            m_instanced_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
            m_instanced_shader.setUniformValue("u_glyph_size", batch.glyph_size);

            // Recreate geometry if glyph type changed
            if (batch.glyph_type != current_geometry_type) {
                m_current_glyph_type = batch.glyph_type;
                createGlyphGeometry();
                
                // Re-upload geometry to VBO
                if (!m_glyph_vertices.empty()) {
                    (void) m_geometry_vbo.bind();
                    m_geometry_vbo.allocate(m_glyph_vertices.data(),
                                            static_cast<int>(m_glyph_vertices.size() * sizeof(float)));
                    m_geometry_vbo.release();
                }
                current_geometry_type = batch.glyph_type;
            }

            if (!m_instanced_vao.bind()) {
                m_instanced_shader.release();
                continue;
            }

            // Upload instance positions
            (void) m_instance_vbo.bind();
            m_instance_vbo.allocate(batch.positions.data(),
                                    static_cast<int>(batch.positions.size() * sizeof(glm::vec2)));
            m_instance_vbo.release();

            // Upload instance colors
            (void) m_color_vbo.bind();
            m_color_vbo.allocate(batch.colors.data(),
                                 static_cast<int>(batch.colors.size() * sizeof(glm::vec4)));
            m_color_vbo.release();

            // Draw based on glyph type
            int const instance_count = static_cast<int>(batch.positions.size());

            switch (batch.glyph_type) {
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

                default:
                    break;
            }

            m_instanced_vao.release();
            m_instanced_shader.release();
        }
    }
}

bool GlyphRenderer::hasData() const {
    return !m_batches.empty();
}

void GlyphRenderer::clearData() {
    m_batches.clear();
    m_all_positions.clear();
    m_all_colors.clear();
    m_glyph_vertices.clear();
    m_glyph_vertex_count = 0;
}

void GlyphRenderer::uploadData(CorePlotting::RenderableGlyphBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    if (batch.positions.empty()) {
        return;
    }

    // Create new batch data
    BatchData batch_data;
    batch_data.glyph_type = batch.glyph_type;
    batch_data.glyph_size = batch.size;
    batch_data.model_matrix = batch.model_matrix;
    batch_data.positions = batch.positions;
    batch_data.has_per_instance_colors = !batch.colors.empty();

    // Prepare colors - if no per-instance colors, create a buffer with global color
    if (batch_data.has_per_instance_colors) {
        batch_data.colors = batch.colors;
    } else {
        // Create uniform color array for all instances
        batch_data.colors.resize(batch.positions.size(), glm::vec4{1.0f, 1.0f, 1.0f, 1.0f});
    }

    // Store batch
    m_batches.push_back(std::move(batch_data));

    // Create glyph geometry if needed for non-Circle glyph types
    bool needs_geometry = (batch.glyph_type != CorePlotting::RenderableGlyphBatch::GlyphType::Circle);
    bool geometry_type_changed = (m_current_glyph_type != batch.glyph_type);
    
    if (needs_geometry && (m_glyph_vertices.empty() || geometry_type_changed)) {
        m_current_glyph_type = batch.glyph_type;
        createGlyphGeometry();
        
        // Upload geometry to VBO
        if (!m_glyph_vertices.empty()) {
            (void) m_instanced_vao.bind();
            (void) m_geometry_vbo.bind();
            m_geometry_vbo.allocate(m_glyph_vertices.data(),
                                    static_cast<int>(m_glyph_vertices.size() * sizeof(float)));
            m_geometry_vbo.release();
            m_instanced_vao.release();
        }
    }
}

void GlyphRenderer::setGlyphSize(float size) {
    m_glyph_size = size;
}

bool GlyphRenderer::loadShadersFromManager() {
    // Not implemented for instanced shaders yet
    return false;
}

bool GlyphRenderer::compileEmbeddedShaders() {
    // Compile point shader for GL_POINTS (circles)
    if (!m_point_shader.createFromSource(GlyphShaders::POINT_VERTEX_SHADER,
                                         GlyphShaders::POINT_FRAGMENT_SHADER)) {
        std::cerr << "[GlyphRenderer] Failed to compile point shader" << std::endl;
        return false;
    }

    // Compile instanced shader for ticks, squares, crosses
    if (!m_instanced_shader.createFromSource(GlyphShaders::INSTANCED_VERTEX_SHADER,
                                              GlyphShaders::INSTANCED_FRAGMENT_SHADER)) {
        std::cerr << "[GlyphRenderer] Failed to compile instanced shader" << std::endl;
        m_point_shader.destroy();
        return false;
    }

    return true;
}

void GlyphRenderer::createGlyphGeometry() {
    m_glyph_vertices.clear();

    // Glyph shapes are defined in normalized coordinates [-0.5, 0.5]
    // They will be scaled by u_glyph_size in the shader

    switch (m_current_glyph_type) {
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
    // Deprecated - use setupPointVertexAttributes() and setupInstancedVertexAttributes()
    setupPointVertexAttributes();
    setupInstancedVertexAttributes();
}

void GlyphRenderer::setupPointVertexAttributes() {
    auto * gl = GLFunctions::get();
    if (!gl) {
        return;
    }

    (void) m_point_vao.bind();

    // For GL_POINTS mode:
    // - location 0: position (vec2) from instance_vbo
    // - location 1: color (vec4) from color_vbo
    
    (void) m_instance_vbo.bind();
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    gl->glEnableVertexAttribArray(0);
    m_instance_vbo.release();

    (void) m_color_vbo.bind();
    gl->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    gl->glEnableVertexAttribArray(1);
    m_color_vbo.release();

    m_point_vao.release();
}

void GlyphRenderer::setupInstancedVertexAttributes() {
    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    (void) m_instanced_vao.bind();

    // For instanced mode:
    // - location 0: geometry vertex (vec2) from geometry_vbo - per-vertex
    // - location 1: instance position (vec2) from instance_vbo - per-instance
    // - location 2: instance color (vec4) from color_vbo - per-instance
    
    // Geometry attribute (per-vertex)
    (void) m_geometry_vbo.bind();
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);
    glExtra->glVertexAttribDivisor(0, 0); // Per-vertex (not instanced)
    m_geometry_vbo.release();

    // Instance position attribute (per-instance)
    (void) m_instance_vbo.bind();
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    gl->glEnableVertexAttribArray(1);
    glExtra->glVertexAttribDivisor(1, 1); // Per-instance
    m_instance_vbo.release();

    // Instance color attribute (per-instance)
    (void) m_color_vbo.bind();
    gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    gl->glEnableVertexAttribArray(2);
    glExtra->glVertexAttribDivisor(2, 1); // Per-instance
    m_color_vbo.release();

    m_instanced_vao.release();
}

}// namespace PlottingOpenGL
