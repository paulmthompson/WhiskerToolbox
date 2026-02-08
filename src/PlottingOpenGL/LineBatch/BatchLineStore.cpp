/**
 * @file BatchLineStore.cpp
 * @brief Implementation of GPU buffer management for batch line data
 */
#include "BatchLineStore.hpp"

#include <cstring> // memcpy

namespace PlottingOpenGL {

BatchLineStore::BatchLineStore() = default;

BatchLineStore::~BatchLineStore()
{
    cleanup();
}

bool BatchLineStore::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!GLFunctions::hasCurrentContext()) {
        return false;
    }

    // Create all SSBOs — if any fail, clean up and return false
    bool ok = true;
    ok = ok && m_segments_ssbo.create();
    ok = ok && m_visibility_ssbo.create();
    ok = ok && m_selection_ssbo.create();
    ok = ok && m_intersection_results_ssbo.create();
    ok = ok && m_intersection_count_ssbo.create();
    ok = ok && m_line_ids_ssbo.create();

    if (!ok) {
        cleanup();
        return false;
    }

    // Pre-allocate the intersection results buffer
    m_intersection_results_ssbo.allocate(
        nullptr, static_cast<int>(RESULTS_CAPACITY * sizeof(std::uint32_t)));

    // Initialize intersection count to zero
    std::uint32_t zero = 0;
    m_intersection_count_ssbo.allocate(&zero, sizeof(std::uint32_t));

    m_initialized = true;
    return true;
}

void BatchLineStore::cleanup()
{
    m_segments_ssbo.destroy();
    m_visibility_ssbo.destroy();
    m_selection_ssbo.destroy();
    m_intersection_results_ssbo.destroy();
    m_intersection_count_ssbo.destroy();
    m_line_ids_ssbo.destroy();
    m_initialized = false;
}

// ── Full upload ────────────────────────────────────────────────────────

void BatchLineStore::upload(CorePlotting::LineBatchData const & batch)
{
    m_cpu_data = batch;

    if (!m_initialized) {
        return; // CPU data is stored; GPU upload deferred
    }

    // Pack segments into 5-float compute-shader format
    packSegments();

    // Upload packed segments
    if (!m_packed_segments.empty()) {
        m_segments_ssbo.allocate(
            m_packed_segments.data(),
            static_cast<int>(m_packed_segments.size() * sizeof(float)));
    } else {
        m_segments_ssbo.allocate(nullptr, 0);
    }

    // Upload visibility mask
    if (!m_cpu_data.visibility_mask.empty()) {
        m_visibility_ssbo.allocate(
            m_cpu_data.visibility_mask.data(),
            static_cast<int>(m_cpu_data.visibility_mask.size() * sizeof(std::uint32_t)));
    } else {
        m_visibility_ssbo.allocate(nullptr, 0);
    }

    // Upload selection mask
    if (!m_cpu_data.selection_mask.empty()) {
        m_selection_ssbo.allocate(
            m_cpu_data.selection_mask.data(),
            static_cast<int>(m_cpu_data.selection_mask.size() * sizeof(std::uint32_t)));
    } else {
        m_selection_ssbo.allocate(nullptr, 0);
    }

    // Upload per-segment line IDs (for the render path vertex attribute)
    if (!m_cpu_data.line_ids.empty()) {
        m_line_ids_ssbo.allocate(
            m_cpu_data.line_ids.data(),
            static_cast<int>(m_cpu_data.line_ids.size() * sizeof(std::uint32_t)));
    } else {
        m_line_ids_ssbo.allocate(nullptr, 0);
    }

    // Reset intersection count
    resetIntersectionCount();
}

// ── Partial updates ────────────────────────────────────────────────────

void BatchLineStore::updateVisibilityMask(std::vector<std::uint32_t> const & mask)
{
    if (mask.size() != m_cpu_data.visibility_mask.size()) {
        return; // Size mismatch
    }

    m_cpu_data.visibility_mask = mask;

    if (m_initialized && !mask.empty()) {
        m_visibility_ssbo.write(
            0, mask.data(),
            static_cast<int>(mask.size() * sizeof(std::uint32_t)));
    }
}

void BatchLineStore::updateSelectionMask(std::vector<std::uint32_t> const & mask)
{
    if (mask.size() != m_cpu_data.selection_mask.size()) {
        return; // Size mismatch
    }

    m_cpu_data.selection_mask = mask;

    if (m_initialized && !mask.empty()) {
        m_selection_ssbo.write(
            0, mask.data(),
            static_cast<int>(mask.size() * sizeof(std::uint32_t)));
    }
}

// ── Bind helpers ───────────────────────────────────────────────────────

void BatchLineStore::bindForCompute() const
{
    if (!m_initialized) {
        return;
    }

    m_segments_ssbo.bindBase(BatchLineBindings::Segments);
    m_intersection_results_ssbo.bindBase(BatchLineBindings::IntersectionResults);
    m_intersection_count_ssbo.bindBase(BatchLineBindings::IntersectionCount);
    m_visibility_ssbo.bindBase(BatchLineBindings::VisibilityMaskCompute);
}

void BatchLineStore::bindForRender() const
{
    if (!m_initialized) {
        return;
    }

    m_selection_ssbo.bindBase(BatchLineBindings::SelectionMask);
    m_visibility_ssbo.bindBase(BatchLineBindings::VisibilityMask);
}

void BatchLineStore::resetIntersectionCount()
{
    if (!m_initialized) {
        return;
    }

    std::uint32_t zero = 0;
    m_intersection_count_ssbo.write(0, &zero, sizeof(std::uint32_t));
}

// ── Internal: pack segments ────────────────────────────────────────────

void BatchLineStore::packSegments()
{
    auto const num_segs = m_cpu_data.numSegments();
    m_packed_segments.clear();
    m_packed_segments.reserve(static_cast<std::size_t>(num_segs) * 5);

    for (std::uint32_t seg = 0; seg < num_segs; ++seg) {
        std::uint32_t const base = seg * 4;

        // x1, y1, x2, y2
        m_packed_segments.push_back(m_cpu_data.segments[base + 0]);
        m_packed_segments.push_back(m_cpu_data.segments[base + 1]);
        m_packed_segments.push_back(m_cpu_data.segments[base + 2]);
        m_packed_segments.push_back(m_cpu_data.segments[base + 3]);

        // Pack line_id as float bits (matching the compute shader's floatBitsToUint)
        std::uint32_t const line_id = m_cpu_data.line_ids[seg];
        float packed_id{};
        std::memcpy(&packed_id, &line_id, sizeof(float));
        m_packed_segments.push_back(packed_id);
    }
}

} // namespace PlottingOpenGL
