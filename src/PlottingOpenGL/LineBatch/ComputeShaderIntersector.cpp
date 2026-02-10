/**
 * @file ComputeShaderIntersector.cpp
 * @brief OpenGL 4.3 compute shader intersection — ported from LineDataVisualization
 */
#include "ComputeShaderIntersector.hpp"

#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderSourceType.hpp"

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <unordered_set>

// Define GL 4.3 constants if not available (macOS only supports GL 4.1)
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif
#ifndef GL_MAX_COMPUTE_WORK_GROUP_COUNT
#define GL_MAX_COMPUTE_WORK_GROUP_COUNT 0x91BE
#endif

namespace PlottingOpenGL {

ComputeShaderIntersector::ComputeShaderIntersector(BatchLineStore & store)
    : m_store(store)
{
}

ComputeShaderIntersector::~ComputeShaderIntersector()
{
    cleanup();
}

bool ComputeShaderIntersector::initialize(std::string const & shader_resource_path)
{
    if (m_initialized) {
        return true;
    }

    if (!GLFunctions::hasCurrentContext()) {
        return false;
    }

    ShaderManager & sm = ShaderManager::instance();

    // Re-use existing program if already loaded for this context
    auto * existing = sm.getProgram(SHADER_PROGRAM_NAME);
    if (!existing) {
        // Determine source type from path prefix
        ShaderSourceType source_type = ShaderSourceType::FileSystem;
        if (shader_resource_path.starts_with(":/")) {
            source_type = ShaderSourceType::Resource;
        }

        bool const ok = sm.loadComputeProgram(
            SHADER_PROGRAM_NAME, shader_resource_path, source_type);
        if (!ok) {
            std::cerr << "[ComputeShaderIntersector] Failed to load compute shader: "
                      << shader_resource_path << std::endl;
            return false;
        }
        existing = sm.getProgram(SHADER_PROGRAM_NAME);
    }

    if (!existing) {
        std::cerr << "[ComputeShaderIntersector] Compute program is null after loading\n";
        return false;
    }

    m_compute_program = existing->getNativeProgram();
    if (!m_compute_program) {
        std::cerr << "[ComputeShaderIntersector] getNativeProgram() returned null\n";
        return false;
    }

    m_initialized = true;
    return true;
}

void ComputeShaderIntersector::cleanup()
{
    // Shader program lifetime is managed by ShaderManager — don't delete it.
    m_compute_program = nullptr;
    m_initialized = false;
}

// ── intersect() ────────────────────────────────────────────────────────

CorePlotting::LineIntersectionResult ComputeShaderIntersector::intersect(
    CorePlotting::LineBatchData const & /*batch*/,
    CorePlotting::LineIntersectionQuery const & query) const
{
    CorePlotting::LineIntersectionResult result;

    if (!m_initialized || !m_compute_program) {
        return result;
    }

    auto const & cpu = m_store.cpuData();
    if (cpu.empty()) {
        return result;
    }

    auto const num_segments = cpu.numSegments();

    // Reset the atomic counter before dispatch
    const_cast<BatchLineStore &>(m_store).resetIntersectionCount();

    // Dispatch the compute shader
    dispatchBatched(num_segments, query);

    // ── Read back results ──────────────────────────────────────────────

    auto * ef = GLFunctions::getExtra();
    if (!ef) {
        return result;
    }

    // Ensure all shader writes are visible
    ef->glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    ef->glFinish();

    // Read intersection count
    auto * f = GLFunctions::get();
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_store.intersectionCountBufferId());
    void * count_ptr = ef->glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0, sizeof(std::uint32_t), GL_MAP_READ_BIT);
    std::uint32_t result_count = 0;
    if (count_ptr) {
        result_count = *static_cast<std::uint32_t const *>(count_ptr);
        ef->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (result_count == 0) {
        return result;
    }

    // Clamp to capacity
    result_count = std::min(result_count, BatchLineStore::RESULTS_CAPACITY);

    // Read intersection results
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_store.intersectionResultsBufferId());
    void * results_ptr = ef->glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0,
        static_cast<int>(result_count * sizeof(std::uint32_t)),
        GL_MAP_READ_BIT);

    if (results_ptr) {
        auto const * ids = static_cast<std::uint32_t const *>(results_ptr);
        std::unordered_set<std::uint32_t> unique_line_ids;

        for (std::uint32_t i = 0; i < result_count; ++i) {
            std::uint32_t const line_id = ids[i]; // 1-based
            if (line_id == 0 || line_id > cpu.numLines()) {
                continue;
            }
            if (unique_line_ids.insert(line_id).second) {
                // Convert 1-based line_id to 0-based LineBatchIndex
                result.intersected_line_indices.push_back(line_id - 1);
            }
        }

        ef->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return result;
}

// ── Batched dispatch ───────────────────────────────────────────────────

void ComputeShaderIntersector::dispatchBatched(
    std::uint32_t num_segments,
    CorePlotting::LineIntersectionQuery const & query) const
{
    auto * ef = GLFunctions::getExtra();
    if (!ef || !m_compute_program) {
        return;
    }

    m_compute_program->bind();

    // Set query uniforms
    m_compute_program->setUniformValue("u_query_line_start",
        query.start_ndc.x, query.start_ndc.y);
    m_compute_program->setUniformValue("u_query_line_end",
        query.end_ndc.x, query.end_ndc.y);
    m_compute_program->setUniformValue("u_line_width", query.tolerance);

    // MVP matrix: glm stores column-major, QMatrix4x4(float*) reads row-major,
    // so we must transpose to get the correct layout for Qt/GL uniforms.
    QMatrix4x4 qt_mvp(glm::value_ptr(query.mvp));
    m_compute_program->setUniformValue("u_mvp_matrix", qt_mvp.transposed());

    // Canvas size
    auto const & cpu = m_store.cpuData();
    m_compute_program->setUniformValue("u_canvas_size",
        cpu.canvas_width, cpu.canvas_height);

    // Explicit sizes
    m_compute_program->setUniformValue("u_total_segments",
        static_cast<GLint>(num_segments));
    m_compute_program->setUniformValue("u_visibility_count",
        static_cast<GLint>(cpu.visibility_mask.size()));
    m_compute_program->setUniformValue("u_results_capacity",
        static_cast<GLint>(BatchLineStore::RESULTS_CAPACITY));

    // Bind SSBOs
    m_store.bindForCompute();

    // Query hardware work-group limits
    GLint max_work_groups_x = 65535; // GL spec minimum
    ef->glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_work_groups_x);
    if (max_work_groups_x <= 0) {
        max_work_groups_x = 65535;
    }

    auto const max_invocations =
        static_cast<std::uint64_t>(max_work_groups_x) * LOCAL_SIZE_X;

    // Batched dispatch loop
    std::uint64_t remaining = num_segments;
    std::uint64_t offset = 0;

    while (remaining > 0) {
        auto const batch_size =
            std::min<std::uint64_t>(remaining, max_invocations);
        auto const groups_x =
            static_cast<std::uint32_t>((batch_size + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X);

        m_compute_program->setUniformValue("u_segment_offset",
            static_cast<GLint>(offset));
        m_compute_program->setUniformValue("u_segments_in_batch",
            static_cast<GLint>(batch_size));

        ef->glDispatchCompute(groups_x, 1, 1);
        ef->glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        offset += batch_size;
        remaining -= batch_size;
    }

    m_compute_program->release();
}

} // namespace PlottingOpenGL
