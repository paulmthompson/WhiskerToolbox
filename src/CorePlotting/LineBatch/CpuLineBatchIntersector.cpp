/**
 * @file CpuLineBatchIntersector.cpp
 * @brief CPU brute-force line intersection — ported from line_intersection.comp
 */
#include "CpuLineBatchIntersector.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <unordered_set>

namespace CorePlotting {

// ── Helpers (mirror the GLSL functions) ────────────────────────────────

float CpuLineBatchIntersector::distancePointToSegment(
    glm::vec2 point,
    glm::vec2 seg_start,
    glm::vec2 seg_end)
{
    glm::vec2 const line_vec = seg_end - seg_start;
    float const line_length_sq = glm::dot(line_vec, line_vec);

    if (line_length_sq == 0.0f) {
        return glm::distance(point, seg_start);
    }

    float const t = glm::clamp(
        glm::dot(point - seg_start, line_vec) / line_length_sq,
        0.0f, 1.0f);

    glm::vec2 const projection = seg_start + t * line_vec;
    return glm::distance(point, projection);
}

bool CpuLineBatchIntersector::segmentsIntersect(
    glm::vec2 a1, glm::vec2 a2,
    glm::vec2 b1, glm::vec2 b2,
    float tolerance)
{
    // 1. Distance-based thick-line check (same as compute shader)
    if (distancePointToSegment(a1, b1, b2) <= tolerance ||
        distancePointToSegment(a2, b1, b2) <= tolerance ||
        distancePointToSegment(b1, a1, a2) <= tolerance ||
        distancePointToSegment(b2, a1, a2) <= tolerance) {
        return true;
    }

    // 2. Geometric segment-segment intersection (cross-product method)
    glm::vec2 const dir1 = a2 - a1;
    glm::vec2 const dir2 = b2 - b1;

    float const cross_product = dir1.x * dir2.y - dir1.y * dir2.x;

    // Parallel segments
    if (std::abs(cross_product) < 1e-6f) {
        return false;
    }

    glm::vec2 const diff = b1 - a1;
    float const t = (diff.x * dir2.y - diff.y * dir2.x) / cross_product;
    float const u = (diff.x * dir1.y - diff.y * dir1.x) / cross_product;

    return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
}

// ── World → NDC transform (mirrors worldToNDC in the shader) ──────────

static glm::vec2 worldToNDC(glm::vec2 world_pos, glm::mat4 const & mvp)
{
    glm::vec4 const clip_pos = mvp * glm::vec4(world_pos, 0.0f, 1.0f);
    if (clip_pos.w == 0.0f) {
        return {0.0f, 0.0f};
    }
    return glm::vec2(clip_pos) / clip_pos.w;
}

// ── Main intersection loop ────────────────────────────────────────────

LineIntersectionResult CpuLineBatchIntersector::intersect(
    LineBatchData const & batch,
    LineIntersectionQuery const & query) const
{
    LineIntersectionResult result;

    if (batch.empty()) {
        return result;
    }

    auto const num_segments = batch.numSegments();

    // Track which logical lines have already been recorded (de-duplicate).
    std::unordered_set<std::uint32_t> hit_line_ids;

    for (std::uint32_t seg = 0; seg < num_segments; ++seg) {
        std::uint32_t const line_id = batch.line_ids[seg]; // 1-based
        if (line_id == 0) {
            continue;
        }

        // Visibility check (line_id is 1-based, mask is 0-indexed)
        std::uint32_t const line_index = line_id - 1;
        if (line_index < static_cast<std::uint32_t>(batch.visibility_mask.size()) &&
            batch.visibility_mask[line_index] == 0) {
            continue;
        }

        // Skip if already hit
        if (hit_line_ids.contains(line_id)) {
            continue;
        }

        // Extract segment endpoints (world space)
        std::uint32_t const base = seg * 4;
        glm::vec2 const seg_start{batch.segments[base + 0], batch.segments[base + 1]};
        glm::vec2 const seg_end{batch.segments[base + 2], batch.segments[base + 3]};

        // Transform to NDC
        glm::vec2 const ndc_start = worldToNDC(seg_start, query.mvp);
        glm::vec2 const ndc_end = worldToNDC(seg_end, query.mvp);

        if (segmentsIntersect(query.start_ndc, query.end_ndc,
                              ndc_start, ndc_end, query.tolerance)) {
            hit_line_ids.insert(line_id);
            result.intersected_line_indices.push_back(line_index);
        }
    }

    return result;
}

} // namespace CorePlotting
