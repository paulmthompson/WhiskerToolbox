/**
 * @file CpuLineBatchIntersector.hpp
 * @brief Brute-force CPU implementation of ILineBatchIntersector
 *
 * Ported from the GLSL compute shader (line_intersection.comp).
 * Fully testable with Catch2 — no GL context required.
 *
 * Used as:
 *  - macOS fallback (no OpenGL 4.3)
 *  - Small-batch fast path (avoids GPU dispatch overhead)
 *  - Reference implementation for validating the compute shader
 */
#ifndef COREPLOTTING_LINEBATCH_CPULINEBATCHINTERSECTOR_HPP
#define COREPLOTTING_LINEBATCH_CPULINEBATCHINTERSECTOR_HPP

#include "ILineBatchIntersector.hpp"

namespace CorePlotting {

class CpuLineBatchIntersector : public ILineBatchIntersector {
public:
    [[nodiscard]] LineIntersectionResult intersect(
        LineBatchData const & batch,
        LineIntersectionQuery const & query) const override;

    // ── Exposed for unit testing ───────────────────────────────────────

    /**
     * @brief Test whether two line segments intersect (or are within tolerance).
     *
     * Uses the same algorithm as the GPU compute shader:
     *  1. Distance-from-endpoint checks (thick-line tolerance).
     *  2. Geometric cross-product based segment-segment intersection.
     */
    [[nodiscard]] static bool segmentsIntersect(
        glm::vec2 a1, glm::vec2 a2,
        glm::vec2 b1, glm::vec2 b2,
        float tolerance);

    /**
     * @brief Shortest distance from a point to a line segment.
     */
    [[nodiscard]] static float distancePointToSegment(
        glm::vec2 point,
        glm::vec2 seg_start,
        glm::vec2 seg_end);
};

} // namespace CorePlotting

#endif // COREPLOTTING_LINEBATCH_CPULINEBATCHINTERSECTOR_HPP
