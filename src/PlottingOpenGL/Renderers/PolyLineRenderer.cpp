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
    m_gpu_buffer_capacity = 0;
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

    // Query current viewport size for geometry shader line width calculation
    GLint viewport[4];
    gl->glGetIntegerv(GL_VIEWPORT, viewport);
    auto const viewport_width = static_cast<float>(viewport[2]);
    auto const viewport_height = static_cast<float>(viewport[3]);

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

    // Render each batch with its own MVP and color
    for (auto const & batch: m_batches) {
        // Compute MVP = Projection * View * Model for this batch
        glm::mat4 mvp = projection_matrix * view_matrix * batch.model_matrix;

        if (m_use_shader_manager) {
            shader_program->setUniform("u_mvp_matrix", mvp);
            shader_program->setUniform("u_view_start_sample", batch.view_start_time);
            auto * native = shader_program->getNativeProgram();
            if (native) {
                native->setUniformValue("u_line_width", batch.thickness);
                native->setUniformValue("u_viewport_size", viewport_width, viewport_height);
            }
        } else {
            m_embedded_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
            m_embedded_shader.setUniformValue("u_view_start_sample", batch.view_start_time);
            m_embedded_shader.setUniformValue("u_line_width", batch.thickness);
            m_embedded_shader.setUniformValue("u_viewport_size", viewport_width, viewport_height);
        }

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

void PolyLineRenderer::uploadBatches(std::span<CorePlotting::RenderablePolyLineBatch const> batches) {
    if (!m_initialized) {
        return;
    }

    clearData();

    if (batches.empty()) {
        return;
    }

    size_t total_floats = 0;
    for (auto const & b: batches) {
        total_floats += b.vertices.size();
    }

    if (total_floats == 0) {
        return;
    }

    m_all_vertices.reserve(total_floats);
    m_batches.reserve(batches.size());

    for (auto const & batch: batches) {
        if (batch.vertices.empty()) {
            continue;
        }

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
        batch_data.view_start_time = batch.view_start_time;
        batch_data.is_integer_time = batch.is_integer_time;

        batch_data.vertex_offset = m_total_vertices;
        batch_data.total_vertices = static_cast<int>(batch.vertices.size()) / 2;

        m_all_vertices.insert(m_all_vertices.end(), batch.vertices.begin(), batch.vertices.end());
        m_total_vertices += batch_data.total_vertices;

        m_batches.push_back(std::move(batch_data));
    }

    size_t const required_bytes = m_all_vertices.size() * sizeof(float);

    (void) m_vao.bind();
    (void) m_vbo.bind();

    // Reallocate with growth headroom if needed
    if (required_bytes > m_gpu_buffer_capacity || m_gpu_buffer_capacity == 0) {
        size_t const desired_capacity = std::max(required_bytes * 2, static_cast<size_t>(1024 * 1024));
        m_vbo.allocate(nullptr, static_cast<int>(desired_capacity));
        m_gpu_buffer_capacity = desired_capacity;
    }

    // Single-pass sub-buffer write into persistent VBO
    m_vbo.write(0, m_all_vertices.data(), static_cast<int>(required_bytes));
    m_last_uploaded_bytes = required_bytes;
    m_was_last_upload_incremental = false;

    m_vbo.release();
    m_vao.release();
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
    batch_data.view_start_time = batch.view_start_time;
    batch_data.is_integer_time = batch.is_integer_time;

    // Track vertex offset for this batch (in vertex count, not floats)
    batch_data.vertex_offset = m_total_vertices;
    batch_data.total_vertices = static_cast<int>(batch.vertices.size()) / 2;

    // Append vertices to combined buffer
    m_all_vertices.insert(m_all_vertices.end(), batch.vertices.begin(), batch.vertices.end());
    m_total_vertices += batch_data.total_vertices;

    // Store batch metadata
    m_batches.push_back(std::move(batch_data));

    // Upload to persistent GPU buffer
    size_t const required_bytes = m_all_vertices.size() * sizeof(float);

    (void) m_vao.bind();
    (void) m_vbo.bind();

    if (required_bytes > m_gpu_buffer_capacity || m_gpu_buffer_capacity == 0) {
        size_t const desired_capacity = std::max(required_bytes * 2, static_cast<size_t>(1024 * 1024));
        m_vbo.allocate(nullptr, static_cast<int>(desired_capacity));
        m_gpu_buffer_capacity = desired_capacity;
    }

    m_vbo.write(0, m_all_vertices.data(), static_cast<int>(required_bytes));
    m_last_uploaded_bytes = required_bytes;
    m_was_last_upload_incremental = false;

    m_vbo.release();
    m_vao.release();
}

void PolyLineRenderer::setLineThickness(float thickness) {
    m_thickness = thickness;
}

bool PolyLineRenderer::loadShadersFromManager() {
    std::string const vertex_path = m_shader_base_path + "wide_line.vert";
    std::string const fragment_path = m_shader_base_path + "wide_line.frag";
    std::string const geometry_path = m_shader_base_path + "wide_line.geom";

    return ShaderManager::instance().loadProgram(
            SHADER_PROGRAM_NAME,
            vertex_path,
            fragment_path,
            geometry_path,
            ShaderSourceType::FileSystem);
}

bool PolyLineRenderer::compileEmbeddedShaders() {
    return m_embedded_shader.createFromSource(PolyLineShaders::VERTEX_SHADER,
                                              PolyLineShaders::GEOMETRY_SHADER,
                                              PolyLineShaders::FRAGMENT_SHADER);
}

void PolyLineRenderer::setupVertexAttributes() {
    auto * gl_extra = GLFunctions::getExtra();
    if (!gl_extra) {
        return;
    }

    (void) m_vao.bind();
    (void) m_vbo.bind();

    // Location 0: int a_time_index (1 int, stride = 2 * sizeof(float), offset = 0)
    gl_extra->glVertexAttribIPointer(0, 1, GL_INT, 2 * sizeof(float), nullptr);
    gl_extra->glEnableVertexAttribArray(0);

    // Location 1: float a_value (1 float, stride = 2 * sizeof(float), offset = sizeof(float))
    gl_extra->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void *>(sizeof(float)));
    gl_extra->glEnableVertexAttribArray(1);

    m_vbo.release();
    m_vao.release();
}

}// namespace PlottingOpenGL