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

    // Try to load shaders from ShaderManager first
    if (!m_shader_base_path.empty()) {
        if (loadShadersFromManager()) {
            m_use_shader_manager = true;
        } else {
            std::cerr << "[GlyphRenderer] Failed to load shaders from ShaderManager, "
                      << "falling back to embedded shaders" << std::endl;
            if (!compileEmbeddedShaders()) {
                return false;
            }
        }
    } else {
        // No shader path provided, use embedded shaders
        if (!compileEmbeddedShaders()) {
            return false;
        }
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
    if (!m_use_shader_manager) {
        m_embedded_shader.destroy();
    }
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

    // Get shader program (either from ShaderManager or embedded)
    ShaderProgram * shader_program = nullptr;
    if (m_use_shader_manager) {
        shader_program = ShaderManager::instance().getProgram(SHADER_PROGRAM_NAME);
        if (!shader_program) {
            std::cerr << "[GlyphRenderer] ShaderManager program not found" << std::endl;
            return;
        }
        shader_program->use();
    } else {
        if (!m_embedded_shader.bind()) {
            return;
        }
    }

    // Bind VAO
    if (!m_vao.bind()) {
        if (!m_use_shader_manager) {
            m_embedded_shader.release();
        }
        return;
    }

    // Render each batch separately (each may have different model matrix and glyph type)
    for (auto const & batch : m_batches) {
        if (batch.positions.empty()) continue;

        // Compute MVP = Projection * View * Model (per-batch model matrix)
        glm::mat4 mvp = projection_matrix * view_matrix * batch.model_matrix;

        if (m_use_shader_manager) {
            shader_program->setUniform("u_mvp_matrix", mvp);
            auto * native = shader_program->getNativeProgram();
            if (native) {
                native->setUniformValue("u_point_size", batch.glyph_size);
            }
        } else {
            m_embedded_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
            m_embedded_shader.setUniformValue("u_point_size", batch.glyph_size);
        }

        // Upload positions for this batch
        (void) m_instance_vbo.bind();
        m_instance_vbo.allocate(batch.positions.data(),
                                static_cast<int>(batch.positions.size() * sizeof(glm::vec2)));
        m_instance_vbo.release();

        // Upload colors for this batch
        (void) m_color_vbo.bind();
        m_color_vbo.allocate(batch.colors.data(),
                             static_cast<int>(batch.colors.size() * sizeof(glm::vec4)));
        m_color_vbo.release();

        // Enable point size program control for circle glyphs
        if (batch.glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle) {
            gl->glEnable(GL_PROGRAM_POINT_SIZE);
        }

        // Draw based on glyph type
        int const instance_count = static_cast<int>(batch.positions.size());

        switch (batch.glyph_type) {
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

        if (batch.glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle) {
            gl->glDisable(GL_PROGRAM_POINT_SIZE);
        }
    }

    // Cleanup
    m_vao.release();
    if (!m_use_shader_manager) {
        m_embedded_shader.release();
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

    // Create glyph geometry if needed (for non-point glyph types)
    if (m_glyph_vertices.empty()) {
        createGlyphGeometry();
    }

    // Setup vertex attributes
    setupVertexAttributes();
}

void GlyphRenderer::setGlyphSize(float size) {
    m_glyph_size = size;
}

bool GlyphRenderer::loadShadersFromManager() {
    // The existing point shaders use a slightly different interface (with group_id support)
    // For basic glyph rendering, we create a simplified version
    // TODO: Consider creating dedicated glyph shaders or adapting the existing point shaders
    std::string const vertex_path = m_shader_base_path + "point.vert";
    std::string const fragment_path = m_shader_base_path + "point.frag";
    
    return ShaderManager::instance().loadProgram(
        SHADER_PROGRAM_NAME,
        vertex_path,
        fragment_path,
        "",  // No geometry shader
        ShaderSourceType::FileSystem
    );
}

bool GlyphRenderer::compileEmbeddedShaders() {
    // Use point-based shaders that work for both point sprites and instanced geometry
    return m_embedded_shader.createFromSource(GlyphShaders::POINT_VERTEX_SHADER,
                                              GlyphShaders::POINT_FRAGMENT_SHADER);
}

void GlyphRenderer::createGlyphGeometry() {
    m_glyph_vertices.clear();

    // Glyph shapes are defined in normalized coordinates [-0.5, 0.5]
    // They will be scaled by uGlyphSize in the shader

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
    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    (void) m_vao.bind();

    // For circle (point) glyphs, setup position as attribute 0 and color as attribute 1
    // For other glyphs, use instanced rendering with geometry VBO
    // Since we support per-batch glyph types, we set up all attributes but only use
    // the appropriate ones during rendering
    
    // Setup geometry VBO for non-circle glyphs
    if (!m_glyph_vertices.empty()) {
        (void) m_geometry_vbo.bind();
        m_geometry_vbo.allocate(m_glyph_vertices.data(),
                                static_cast<int>(m_glyph_vertices.size() * sizeof(float)));

        // Vertex attribute (location = 0) - per-vertex shape
        gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        gl->glEnableVertexAttribArray(0);
        m_geometry_vbo.release();
    }

    // Setup instance VBO for positions (will be uploaded per-batch in render)
    (void) m_instance_vbo.bind();
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
    gl->glEnableVertexAttribArray(1);
    glExtra->glVertexAttribDivisor(1, 1);// Advance once per instance
    m_instance_vbo.release();

    // Setup color VBO (will be uploaded per-batch in render)
    (void) m_color_vbo.bind();
    gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    gl->glEnableVertexAttribArray(2);
    glExtra->glVertexAttribDivisor(2, 1);// Advance once per instance
    m_color_vbo.release();

    m_vao.release();
}

}// namespace PlottingOpenGL