#include "PolyLineRenderer.hpp"

#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>

namespace PlottingOpenGL {

PolyLineRenderer::PolyLineRenderer(std::string shader_base_path)
    : m_shader_base_path(std::move(shader_base_path)) {
}

PolyLineRenderer::~PolyLineRenderer() {
    cleanup();
}

bool PolyLineRenderer::initialize() {
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
            std::cerr << "[PolyLineRenderer] Failed to load shaders from ShaderManager, "
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

    // Create VAO and VBO
    if (!m_vao.create()) {
        return false;
    }
    if (!m_vbo.create()) {
        m_vao.destroy();
        return false;
    }

    // Setup vertex attributes
    setupVertexAttributes();

    m_initialized = true;
    return true;
}

void PolyLineRenderer::cleanup() {
    m_vbo.destroy();
    m_vao.destroy();
    if (!m_use_shader_manager) {
        m_embedded_shader.destroy();
    }
    m_initialized = false;
    clearData();
}

bool PolyLineRenderer::isInitialized() const {
    return m_initialized;
}

void PolyLineRenderer::render(glm::mat4 const & view_matrix,
                              glm::mat4 const & projection_matrix) {
    if (!m_initialized || m_total_vertices == 0) {
        return;
    }

    auto * gl = GLFunctions::get();
    if (!gl) {
        return;
    }

    // Compute MVP = Projection * View * Model
    glm::mat4 mvp = projection_matrix * view_matrix * m_model_matrix;

    // Get shader program (either from ShaderManager or embedded)
    ShaderProgram * shader_program = nullptr;
    if (m_use_shader_manager) {
        shader_program = ShaderManager::instance().getProgram(SHADER_PROGRAM_NAME);
        if (!shader_program) {
            std::cerr << "[PolyLineRenderer] ShaderManager program not found" << std::endl;
            return;
        }
        shader_program->use();
        shader_program->setUniform("u_mvp_matrix", mvp);
    } else {
        if (!m_embedded_shader.bind()) {
            return;
        }
        m_embedded_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
    }

    // Bind VAO
    if (!m_vao.bind()) {
        if (!m_use_shader_manager) {
            m_embedded_shader.release();
        }
        return;
    }

    // Set line width (may be clamped by driver)
    gl->glLineWidth(m_thickness);

    // Draw each polyline segment
    if (m_has_per_line_colors && m_line_colors.size() == m_line_start_indices.size()) {
        // Per-line colors
        for (size_t i = 0; i < m_line_start_indices.size(); ++i) {
            glm::vec4 const & color = m_line_colors[i];
            if (m_use_shader_manager) {
                // ShaderProgram doesn't have a vec4 overload, use native program
                auto * native = shader_program->getNativeProgram();
                if (native) {
                    native->setUniformValue("u_color", color.r, color.g, color.b, color.a);
                }
            } else {
                m_embedded_shader.setUniformValue("u_color", color.r, color.g, color.b, color.a);
            }
            gl->glDrawArrays(GL_LINE_STRIP,
                             m_line_start_indices[i],
                             m_line_vertex_counts[i]);
        }
    } else {
        // Global color for all lines
        if (m_use_shader_manager) {
            auto * native = shader_program->getNativeProgram();
            if (native) {
                native->setUniformValue("u_color", 
                                        m_global_color.r,
                                        m_global_color.g,
                                        m_global_color.b,
                                        m_global_color.a);
            }
        } else {
            m_embedded_shader.setUniformValue("u_color",
                                              m_global_color.r,
                                              m_global_color.g,
                                              m_global_color.b,
                                              m_global_color.a);
        }

        for (size_t i = 0; i < m_line_start_indices.size(); ++i) {
            gl->glDrawArrays(GL_LINE_STRIP,
                             m_line_start_indices[i],
                             m_line_vertex_counts[i]);
        }
    }

    // Cleanup
    m_vao.release();
    if (!m_use_shader_manager) {
        m_embedded_shader.release();
    }
}

bool PolyLineRenderer::hasData() const {
    return m_total_vertices > 0;
}

void PolyLineRenderer::clearData() {
    m_line_start_indices.clear();
    m_line_vertex_counts.clear();
    m_line_colors.clear();
    m_total_vertices = 0;
    m_has_per_line_colors = false;
    m_model_matrix = glm::mat4{1.0f};
    m_global_color = glm::vec4{1.0f};
}

void PolyLineRenderer::uploadData(CorePlotting::RenderablePolyLineBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    // Clear previous data
    clearData();

    if (batch.vertices.empty()) {
        return;
    }

    // Copy topology data
    m_line_start_indices = batch.line_start_indices;
    m_line_vertex_counts = batch.line_vertex_counts;

    // Copy color data
    m_has_per_line_colors = !batch.colors.empty();
    if (m_has_per_line_colors) {
        m_line_colors = batch.colors;
    }
    m_global_color = batch.global_color;

    // Copy transform data
    m_model_matrix = batch.model_matrix;
    m_thickness = batch.thickness;

    // Calculate total vertices (vertices are {x, y} pairs)
    m_total_vertices = static_cast<int>(batch.vertices.size()) / 2;

    // Upload vertex data to GPU
    (void) m_vao.bind();
    (void) m_vbo.bind();
    m_vbo.allocate(batch.vertices.data(),
                   static_cast<int>(batch.vertices.size() * sizeof(float)));
    m_vbo.release();
    m_vao.release();
}

void PolyLineRenderer::setLineThickness(float thickness) {
    m_thickness = thickness;
}

bool PolyLineRenderer::loadShadersFromManager() {
    std::string const vertex_path = m_shader_base_path + "line.vert";
    std::string const fragment_path = m_shader_base_path + "line.frag";
    
    return ShaderManager::instance().loadProgram(
        SHADER_PROGRAM_NAME,
        vertex_path,
        fragment_path,
        "",  // No geometry shader
        ShaderSourceType::FileSystem
    );
}

bool PolyLineRenderer::compileEmbeddedShaders() {
    return m_embedded_shader.createFromSource(PolyLineShaders::VERTEX_SHADER,
                                              PolyLineShaders::FRAGMENT_SHADER);
}

void PolyLineRenderer::setupVertexAttributes() {
    auto * gl = GLFunctions::get();
    if (!gl) {
        return;
    }

    (void) m_vao.bind();
    (void) m_vbo.bind();

    // Position attribute: vec2 at location 0
    // Stride = 2 floats (x, y)
    // Offset = 0
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);

    m_vbo.release();
    m_vao.release();
}

}// namespace PlottingOpenGL