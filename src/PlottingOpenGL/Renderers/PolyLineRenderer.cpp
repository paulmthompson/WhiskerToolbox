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
    if (!m_initialized || m_total_vertices == 0 || m_batches.empty()) {
        return;
    }

    auto * gl = GLFunctions::get();
    if (!gl) {
        return;
    }

    // Get shader program (either from ShaderManager or embedded)
    ShaderProgram * shader_program = nullptr;
    if (m_use_shader_manager) {
        shader_program = ShaderManager::instance().getProgram(SHADER_PROGRAM_NAME);
        if (!shader_program) {
            std::cerr << "[PolyLineRenderer] ShaderManager program not found" << std::endl;
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

    // Render each batch with its own model matrix
    for (auto const & batch : m_batches) {
        if (batch.total_vertices == 0) continue;

        // Compute MVP = Projection * View * Model (per-batch model matrix)
        glm::mat4 mvp = projection_matrix * view_matrix * batch.model_matrix;

        if (m_use_shader_manager) {
            shader_program->setUniform("u_mvp_matrix", mvp);
        } else {
            m_embedded_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
        }

        // Set line width (may be clamped by driver)
        gl->glLineWidth(batch.thickness);

        // Draw each polyline segment in this batch
        if (batch.has_per_line_colors && batch.line_colors.size() == batch.line_start_indices.size()) {
            // Per-line colors
            for (size_t i = 0; i < batch.line_start_indices.size(); ++i) {
                glm::vec4 const & color = batch.line_colors[i];
                if (m_use_shader_manager) {
                    auto * native = shader_program->getNativeProgram();
                    if (native) {
                        native->setUniformValue("u_color", color.r, color.g, color.b, color.a);
                    }
                } else {
                    m_embedded_shader.setUniformValue("u_color", color.r, color.g, color.b, color.a);
                }
                gl->glDrawArrays(GL_LINE_STRIP,
                                 batch.vertex_offset + batch.line_start_indices[i],
                                 batch.line_vertex_counts[i]);
            }
        } else {
            // Global color for all lines in this batch
            if (m_use_shader_manager) {
                auto * native = shader_program->getNativeProgram();
                if (native) {
                    native->setUniformValue("u_color", 
                                            batch.global_color.r,
                                            batch.global_color.g,
                                            batch.global_color.b,
                                            batch.global_color.a);
                }
            } else {
                m_embedded_shader.setUniformValue("u_color",
                                                  batch.global_color.r,
                                                  batch.global_color.g,
                                                  batch.global_color.b,
                                                  batch.global_color.a);
            }

            for (size_t i = 0; i < batch.line_start_indices.size(); ++i) {
                gl->glDrawArrays(GL_LINE_STRIP,
                                 batch.vertex_offset + batch.line_start_indices[i],
                                 batch.line_vertex_counts[i]);
            }
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
    m_batches.clear();
    m_all_vertices.clear();
    m_total_vertices = 0;
}

void PolyLineRenderer::uploadData(CorePlotting::RenderablePolyLineBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    if (batch.vertices.empty()) {
        return;
    }

    // Create new batch data
    BatchData batch_data;
    batch_data.line_start_indices = batch.line_start_indices;
    batch_data.line_vertex_counts = batch.line_vertex_counts;
    batch_data.has_per_line_colors = !batch.colors.empty();
    if (batch_data.has_per_line_colors) {
        batch_data.line_colors = batch.colors;
    }
    batch_data.global_color = batch.global_color;
    batch_data.model_matrix = batch.model_matrix;
    batch_data.thickness = batch.thickness;
    
    // Track vertex offset for this batch (in vertex count, not floats)
    batch_data.vertex_offset = m_total_vertices;
    batch_data.total_vertices = static_cast<int>(batch.vertices.size()) / 2;

    // Append vertices to combined buffer
    m_all_vertices.insert(m_all_vertices.end(), batch.vertices.begin(), batch.vertices.end());
    m_total_vertices += batch_data.total_vertices;

    // Store batch metadata
    m_batches.push_back(std::move(batch_data));

    // Upload all vertex data to GPU
    (void) m_vao.bind();
    (void) m_vbo.bind();
    m_vbo.allocate(m_all_vertices.data(),
                   static_cast<int>(m_all_vertices.size() * sizeof(float)));
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