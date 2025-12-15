#include "RectangleRenderer.hpp"

#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>

namespace PlottingOpenGL {

RectangleRenderer::RectangleRenderer(std::string shader_base_path)
    : m_shader_base_path(std::move(shader_base_path)) {
}

RectangleRenderer::~RectangleRenderer() {
    cleanup();
}

bool RectangleRenderer::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!GLFunctions::hasCurrentContext()) {
        return false;
    }

    // Try to load shaders from ShaderManager first
    // Note: Rectangle shaders are specialized, we use embedded shaders
    // unless external shader files are explicitly provided
    if (!m_shader_base_path.empty()) {
        if (loadShadersFromManager()) {
            m_use_shader_manager = true;
        } else {
            std::cerr << "[RectangleRenderer] Failed to load shaders from ShaderManager, "
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
    if (!m_quad_vbo.create()) {
        m_vao.destroy();
        return false;
    }
    if (!m_bounds_vbo.create()) {
        m_quad_vbo.destroy();
        m_vao.destroy();
        return false;
    }
    if (!m_color_vbo.create()) {
        m_bounds_vbo.destroy();
        m_quad_vbo.destroy();
        m_vao.destroy();
        return false;
    }

    // Create unit quad geometry
    createQuadGeometry();

    m_initialized = true;
    return true;
}

void RectangleRenderer::cleanup() {
    m_color_vbo.destroy();
    m_bounds_vbo.destroy();
    m_quad_vbo.destroy();
    m_vao.destroy();
    if (!m_use_shader_manager) {
        m_embedded_fill_shader.destroy();
        m_embedded_border_shader.destroy();
    }
    m_initialized = false;
    clearData();
}

bool RectangleRenderer::isInitialized() const {
    return m_initialized;
}

void RectangleRenderer::render(glm::mat4 const & view_matrix,
                               glm::mat4 const & projection_matrix) {
    if (!m_initialized || m_bounds.empty()) {
        return;
    }

    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    // Compute MVP = Projection * View * Model
    glm::mat4 mvp = projection_matrix * view_matrix * m_model_matrix;

    int const instance_count = static_cast<int>(m_bounds.size());

    // Enable blending for transparent rectangles
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Draw filled rectangles ---
    ShaderProgram * fill_shader = nullptr;
    if (m_use_shader_manager) {
        fill_shader = ShaderManager::instance().getProgram(FILL_SHADER_NAME);
        if (!fill_shader) {
            std::cerr << "[RectangleRenderer] Fill shader program not found" << std::endl;
            return;
        }
        fill_shader->use();
        fill_shader->setUniform("u_mvp_matrix", mvp);
    } else {
        if (!m_embedded_fill_shader.bind()) {
            return;
        }
        m_embedded_fill_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
    }

    if (!m_vao.bind()) {
        if (!m_use_shader_manager) {
            m_embedded_fill_shader.release();
        }
        return;
    }

    // Draw instanced quads (4 vertices per quad, as triangle strip)
    glExtra->glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);

    if (!m_use_shader_manager) {
        m_embedded_fill_shader.release();
    }

    // --- Draw borders (optional) ---
    if (m_border_enabled) {
        ShaderProgram * border_shader = nullptr;
        if (m_use_shader_manager) {
            border_shader = ShaderManager::instance().getProgram(BORDER_SHADER_NAME);
            if (!border_shader) {
                m_vao.release();
                return;
            }
            border_shader->use();
            border_shader->setUniform("u_mvp_matrix", mvp);
            auto * native = border_shader->getNativeProgram();
            if (native) {
                native->setUniformValue("u_border_color",
                                        m_border_color.r,
                                        m_border_color.g,
                                        m_border_color.b,
                                        m_border_color.a);
            }
        } else {
            if (!m_embedded_border_shader.bind()) {
                m_vao.release();
                return;
            }
            m_embedded_border_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
            m_embedded_border_shader.setUniformValue("u_border_color",
                                                     m_border_color.r,
                                                     m_border_color.g,
                                                     m_border_color.b,
                                                     m_border_color.a);
        }

        gl->glLineWidth(m_border_width);

        // Draw each rectangle border as LINE_LOOP
        // We need to switch to the border quad vertices (corners in order)
        for (int i = 0; i < instance_count; ++i) {
            // For LINE_LOOP, we need 4 corner vertices
            // We'll draw each rectangle separately
            glExtra->glDrawArraysInstanced(GL_LINE_LOOP, 4, 4, 1);
        }

        if (!m_use_shader_manager) {
            m_embedded_border_shader.release();
        }
    }

    m_vao.release();
    gl->glDisable(GL_BLEND);
}

bool RectangleRenderer::hasData() const {
    return !m_bounds.empty();
}

void RectangleRenderer::clearData() {
    m_bounds.clear();
    m_colors.clear();
    m_model_matrix = glm::mat4{1.0f};
}

void RectangleRenderer::uploadData(CorePlotting::RenderableRectangleBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    clearData();

    if (batch.bounds.empty()) {
        return;
    }

    // Store batch properties
    m_bounds = batch.bounds;
    m_model_matrix = batch.model_matrix;

    // Prepare colors
    if (!batch.colors.empty()) {
        m_colors = batch.colors;
    } else {
        // Default to white if no colors provided
        m_colors.resize(batch.bounds.size(), glm::vec4{1.0f, 1.0f, 1.0f, 0.5f});
    }

    // Setup vertex attributes
    setupVertexAttributes();
}

void RectangleRenderer::setBorderEnabled(bool enabled) {
    m_border_enabled = enabled;
}

void RectangleRenderer::setBorderColor(glm::vec4 const & color) {
    m_border_color = color;
}

void RectangleRenderer::setBorderWidth(float width) {
    m_border_width = width;
}

bool RectangleRenderer::loadShadersFromManager() {
    // Rectangle shaders are specialized and not in the standard shader directory
    // For now, we always use embedded shaders unless specific files are created
    // This is a placeholder for future shader file support
    return false;
}

bool RectangleRenderer::compileEmbeddedShaders() {
    // Compile fill shader
    if (!m_embedded_fill_shader.createFromSource(RectangleShaders::VERTEX_SHADER,
                                                 RectangleShaders::FRAGMENT_SHADER)) {
        return false;
    }

    // Compile border shader
    if (!m_embedded_border_shader.createFromSource(RectangleShaders::BORDER_VERTEX_SHADER,
                                                   RectangleShaders::BORDER_FRAGMENT_SHADER)) {
        return false;
    }

    return true;
}

void RectangleRenderer::createQuadGeometry() {
    // Combine both sets of vertices
    static float const all_vertices[] = {
            // Triangle strip vertices (0-3)
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            // Line loop vertices (4-7)
            0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

    (void) m_vao.bind();
    (void) m_quad_vbo.bind();
    m_quad_vbo.allocate(all_vertices, sizeof(all_vertices));
    m_quad_vbo.release();
    m_vao.release();
}

void RectangleRenderer::setupVertexAttributes() {
    auto * gl = GLFunctions::get();
    auto * glExtra = GLFunctions::getExtra();
    if (!gl || !glExtra) {
        return;
    }

    (void) m_vao.bind();

    // Bind quad geometry VBO
    (void) m_quad_vbo.bind();

    // Vertex position attribute (location = 0) - per-vertex
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);
    m_quad_vbo.release();

    // Upload instance bounds
    (void) m_bounds_vbo.bind();
    m_bounds_vbo.allocate(m_bounds.data(),
                          static_cast<int>(m_bounds.size() * sizeof(glm::vec4)));

    // Bounds attribute (location = 1) - per-instance
    gl->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    gl->glEnableVertexAttribArray(1);
    glExtra->glVertexAttribDivisor(1, 1);// Advance once per instance
    m_bounds_vbo.release();

    // Upload instance colors
    (void) m_color_vbo.bind();
    m_color_vbo.allocate(m_colors.data(),
                         static_cast<int>(m_colors.size() * sizeof(glm::vec4)));

    // Color attribute (location = 2) - per-instance
    gl->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    gl->glEnableVertexAttribArray(2);
    glExtra->glVertexAttribDivisor(2, 1);// Advance once per instance
    m_color_vbo.release();

    m_vao.release();
}

}// namespace PlottingOpenGL
