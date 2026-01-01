#include "StreamingPolyLineRenderer.hpp"

#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "PolyLineRenderer.hpp"  // For embedded shader source

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace PlottingOpenGL {

StreamingPolyLineRenderer::StreamingPolyLineRenderer(std::string shader_base_path,
                                                     float capacity_multiplier)
    : m_shader_base_path(std::move(shader_base_path)),
      m_capacity_multiplier(capacity_multiplier) {
}

StreamingPolyLineRenderer::~StreamingPolyLineRenderer() {
    cleanup();
}

bool StreamingPolyLineRenderer::initialize() {
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
            std::cerr << "[StreamingPolyLineRenderer] Failed to load shaders from ShaderManager, "
                      << "falling back to embedded shaders" << std::endl;
            if (!compileEmbeddedShaders()) {
                return false;
            }
        }
    } else {
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

void StreamingPolyLineRenderer::cleanup() {
    m_vbo.destroy();
    m_vao.destroy();
    if (!m_use_shader_manager) {
        m_embedded_shader.destroy();
    }
    m_initialized = false;
    clearData();
}

bool StreamingPolyLineRenderer::isInitialized() const {
    return m_initialized;
}

void StreamingPolyLineRenderer::render(glm::mat4 const & view_matrix,
                                       glm::mat4 const & projection_matrix) {
    if (!m_initialized || !m_cached_batch.valid || m_cached_batch.vertices.empty()) {
        return;
    }

    auto render_start = std::chrono::high_resolution_clock::now();

    auto * gl = GLFunctions::get();
    if (!gl) {
        return;
    }

    // Get shader program
    ShaderProgram * shader_program = nullptr;
    if (m_use_shader_manager) {
        shader_program = ShaderManager::instance().getProgram(SHADER_PROGRAM_NAME);
        if (!shader_program) {
            std::cerr << "[StreamingPolyLineRenderer] ShaderManager program not found" << std::endl;
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

    // Compute MVP = Projection * View * Model
    glm::mat4 mvp = projection_matrix * view_matrix * m_cached_batch.model_matrix;

    if (m_use_shader_manager) {
        shader_program->setUniform("u_mvp_matrix", mvp);
        auto * native = shader_program->getNativeProgram();
        if (native) {
            native->setUniformValue("u_color",
                                    m_cached_batch.global_color.r,
                                    m_cached_batch.global_color.g,
                                    m_cached_batch.global_color.b,
                                    m_cached_batch.global_color.a);
        }
    } else {
        m_embedded_shader.setUniformMatrix4("u_mvp_matrix", glm::value_ptr(mvp));
        m_embedded_shader.setUniformValue("u_color",
                                          m_cached_batch.global_color.r,
                                          m_cached_batch.global_color.g,
                                          m_cached_batch.global_color.b,
                                          m_cached_batch.global_color.a);
    }

    // Set line width
    gl->glLineWidth(m_cached_batch.thickness);

    // Draw each polyline segment
    for (size_t i = 0; i < m_cached_batch.line_start_indices.size(); ++i) {
        gl->glDrawArrays(GL_LINE_STRIP,
                         m_cached_batch.line_start_indices[i],
                         m_cached_batch.line_vertex_counts[i]);
    }

    // Cleanup
    m_vao.release();
    if (!m_use_shader_manager) {
        m_embedded_shader.release();
    }

    if (m_timing_enabled) {
        auto render_end = std::chrono::high_resolution_clock::now();
        m_timing_stats.last_render_time = std::chrono::duration_cast<std::chrono::microseconds>(
                render_end - render_start);
        m_timing_stats.last_total_time = m_timing_stats.last_upload_time + m_timing_stats.last_render_time;

        // Update moving average
        double const alpha = 0.1;  // Smoothing factor
        m_timing_stats.avg_render_time_us = alpha * static_cast<double>(m_timing_stats.last_render_time.count()) +
                                            (1.0 - alpha) * m_timing_stats.avg_render_time_us;
    }
}

bool StreamingPolyLineRenderer::hasData() const {
    return m_cached_batch.valid && !m_cached_batch.vertices.empty();
}

void StreamingPolyLineRenderer::clearData() {
    m_cached_batch = CachedBatchData{};
    m_dirty_regions.clear();
    m_pending_vertices.clear();
    m_gpu_buffer_used = 0;
    // Note: We don't reset m_gpu_buffer_capacity - keep the allocated buffer
}

void StreamingPolyLineRenderer::updateData(CorePlotting::RenderablePolyLineBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    auto upload_start = std::chrono::high_resolution_clock::now();
    m_total_updates++;

    // Check if incremental update is possible
    if (computeDirtyRegions(batch)) {
        // Incremental update
        updateGPUBufferIncremental();
        m_incremental_updates++;
        
        if (m_timing_enabled) {
            m_timing_stats.was_full_reupload = false;
        }
    } else {
        // Full re-upload required
        uploadGPUBufferFull(batch);
        
        if (m_timing_enabled) {
            m_timing_stats.was_full_reupload = true;
        }
    }

    if (m_timing_enabled) {
        auto upload_end = std::chrono::high_resolution_clock::now();
        m_timing_stats.last_upload_time = std::chrono::duration_cast<std::chrono::microseconds>(
                upload_end - upload_start);

        // Update moving average
        double const alpha = 0.1;
        m_timing_stats.avg_upload_time_us = alpha * static_cast<double>(m_timing_stats.last_upload_time.count()) +
                                            (1.0 - alpha) * m_timing_stats.avg_upload_time_us;
        m_timing_stats.sample_count++;
    }
}

void StreamingPolyLineRenderer::uploadData(CorePlotting::RenderablePolyLineBatch const & batch) {
    if (!m_initialized) {
        return;
    }

    auto upload_start = std::chrono::high_resolution_clock::now();
    m_total_updates++;

    // Force full re-upload
    uploadGPUBufferFull(batch);

    if (m_timing_enabled) {
        auto upload_end = std::chrono::high_resolution_clock::now();
        m_timing_stats.last_upload_time = std::chrono::duration_cast<std::chrono::microseconds>(
                upload_end - upload_start);
        m_timing_stats.was_full_reupload = true;
        m_timing_stats.sample_count++;
    }
}

float StreamingPolyLineRenderer::getCacheHitRatio() const {
    if (m_total_updates == 0) {
        return 0.0f;
    }
    return static_cast<float>(m_incremental_updates) / static_cast<float>(m_total_updates);
}

bool StreamingPolyLineRenderer::computeDirtyRegions(CorePlotting::RenderablePolyLineBatch const & batch) {
    m_dirty_regions.clear();
    m_pending_vertices = batch.vertices;

    // Check if we have cached data to compare against
    if (!m_cached_batch.valid) {
        return false;  // First upload - need full upload
    }

    // Check if topology changed (requires full re-upload)
    if (batch.line_start_indices.size() != m_cached_batch.line_start_indices.size() ||
        batch.line_vertex_counts.size() != m_cached_batch.line_vertex_counts.size()) {
        return false;
    }

    // Check if vertex counts changed
    for (size_t i = 0; i < batch.line_vertex_counts.size(); ++i) {
        if (batch.line_vertex_counts[i] != m_cached_batch.line_vertex_counts[i]) {
            return false;
        }
    }

    // Check buffer capacity
    size_t const required_bytes = batch.vertices.size() * sizeof(float);
    if (required_bytes > m_gpu_buffer_capacity) {
        return false;  // Need to reallocate
    }

    // Find dirty regions by comparing vertices
    // Strategy: Scan for contiguous regions that differ
    size_t const vertex_count = batch.vertices.size();
    if (vertex_count != m_cached_batch.vertices.size()) {
        return false;
    }

    bool in_dirty_region = false;
    size_t dirty_start = 0;

    for (size_t i = 0; i < vertex_count; ++i) {
        bool const is_different = std::abs(batch.vertices[i] - m_cached_batch.vertices[i]) > m_comparison_tolerance;

        if (is_different && !in_dirty_region) {
            // Start of dirty region
            dirty_start = i;
            in_dirty_region = true;
        } else if (!is_different && in_dirty_region) {
            // End of dirty region
            m_dirty_regions.push_back({
                    dirty_start * sizeof(float),
                    i * sizeof(float)});
            in_dirty_region = false;
        }
    }

    // Close any open dirty region
    if (in_dirty_region) {
        m_dirty_regions.push_back({
                dirty_start * sizeof(float),
                vertex_count * sizeof(float)});
    }

    // If too many dirty regions, it's more efficient to do full upload
    // (glBufferSubData has overhead per call)
    if (m_dirty_regions.size() > 10) {
        m_dirty_regions.clear();
        return false;
    }

    // Calculate total dirty bytes
    size_t dirty_bytes = 0;
    for (auto const & region : m_dirty_regions) {
        dirty_bytes += region.end_byte - region.start_byte;
    }

    // If more than 50% is dirty, just do full upload
    if (dirty_bytes > required_bytes / 2) {
        m_dirty_regions.clear();
        return false;
    }

    return true;  // Incremental update is worthwhile
}

void StreamingPolyLineRenderer::updateGPUBufferIncremental() {
    auto * gl = GLFunctions::get();
    if (!gl || m_dirty_regions.empty()) {
        // No changes needed, but still update cached metadata
        return;
    }

    (void) m_vao.bind();
    (void) m_vbo.bind();

    size_t total_uploaded = 0;

    // Upload each dirty region using glBufferSubData
    for (auto const & region : m_dirty_regions) {
        size_t const offset = region.start_byte;
        size_t const size = region.end_byte - region.start_byte;
        size_t const float_offset = offset / sizeof(float);

        // Use write() which calls glBufferSubData
        m_vbo.write(static_cast<int>(offset),
                    m_pending_vertices.data() + float_offset,
                    static_cast<int>(size));
        total_uploaded += size;
    }

    m_vbo.release();
    m_vao.release();

    // Update cache with new data
    m_cached_batch.vertices = std::move(m_pending_vertices);
    m_cached_batch.valid = true;

    if (m_timing_enabled) {
        m_timing_stats.bytes_uploaded = total_uploaded;
        m_timing_stats.bytes_total = m_cached_batch.vertices.size() * sizeof(float);
    }

    m_dirty_regions.clear();
}

void StreamingPolyLineRenderer::uploadGPUBufferFull(CorePlotting::RenderablePolyLineBatch const & batch) {
    if (batch.vertices.empty()) {
        clearData();
        return;
    }

    size_t const required_bytes = batch.vertices.size() * sizeof(float);
    size_t const desired_capacity = static_cast<size_t>(
            static_cast<float>(required_bytes) * m_capacity_multiplier);

    (void) m_vao.bind();
    (void) m_vbo.bind();

    // Reallocate if needed (or if first allocation)
    if (desired_capacity > m_gpu_buffer_capacity || m_gpu_buffer_capacity == 0) {
        // Allocate buffer with extra capacity
        m_vbo.allocate(nullptr, static_cast<int>(desired_capacity));
        m_gpu_buffer_capacity = desired_capacity;
    }

    // Upload data
    m_vbo.write(0, batch.vertices.data(), static_cast<int>(required_bytes));
    m_gpu_buffer_used = required_bytes;

    m_vbo.release();
    m_vao.release();

    // Update cache
    m_cached_batch.vertices = batch.vertices;
    m_cached_batch.line_start_indices = batch.line_start_indices;
    m_cached_batch.line_vertex_counts = batch.line_vertex_counts;
    m_cached_batch.global_color = batch.global_color;
    m_cached_batch.model_matrix = batch.model_matrix;
    m_cached_batch.thickness = batch.thickness;
    m_cached_batch.valid = true;

    if (m_timing_enabled) {
        m_timing_stats.bytes_uploaded = required_bytes;
        m_timing_stats.bytes_total = required_bytes;
    }
}

bool StreamingPolyLineRenderer::loadShadersFromManager() {
    std::string const vertex_path = m_shader_base_path + "line.vert";
    std::string const fragment_path = m_shader_base_path + "line.frag";

    return ShaderManager::instance().loadProgram(
            SHADER_PROGRAM_NAME,
            vertex_path,
            fragment_path,
            "",
            ShaderSourceType::FileSystem);
}

bool StreamingPolyLineRenderer::compileEmbeddedShaders() {
    // Reuse the same embedded shaders as PolyLineRenderer
    return m_embedded_shader.createFromSource(PolyLineShaders::VERTEX_SHADER,
                                              PolyLineShaders::FRAGMENT_SHADER);
}

void StreamingPolyLineRenderer::setupVertexAttributes() {
    auto * gl = GLFunctions::get();
    if (!gl) {
        return;
    }

    (void) m_vao.bind();
    (void) m_vbo.bind();

    // Position attribute: vec2 at location 0
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);

    m_vbo.release();
    m_vao.release();
}

}// namespace PlottingOpenGL
