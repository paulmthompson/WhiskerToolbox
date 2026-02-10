/**
 * @file ILineBatchIntersector.hpp
 * @brief Abstract interface for batch line intersection queries
 *
 * Decouples the intersection algorithm from its execution backend.
 * The CPU fallback (CpuLineBatchIntersector) and the GPU compute-shader
 * implementation (ComputeShaderIntersector) both fulfil this interface.
 *
 * Part of the CorePlotting layer — no OpenGL or Qt dependencies.
 */
#ifndef COREPLOTTING_LINEBATCH_ILINEBATCHINTERSECTOR_HPP
#define COREPLOTTING_LINEBATCH_ILINEBATCHINTERSECTOR_HPP

#include "LineBatchData.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace CorePlotting {

/**
 * @brief Parameters for a line-vs-batch intersection query.
 *
 * The query line is specified in NDC space [-1, 1].
 * Segments stored in LineBatchData are in world space and will be transformed
 * by @c mvp before the intersection test.
 */
struct LineIntersectionQuery {
    glm::vec2 start_ndc{0.0f, 0.0f};  ///< Query line start in NDC [-1,1]
    glm::vec2 end_ndc{0.0f, 0.0f};    ///< Query line end in NDC [-1,1]
    float tolerance{0.05f};            ///< Proximity tolerance in NDC units
    glm::mat4 mvp{1.0f};              ///< World → NDC transform
};

/**
 * @brief Result of a line-vs-batch intersection query.
 */
struct LineIntersectionResult {
    /// 0-based indices into LineBatchData::lines
    std::vector<LineBatchIndex> intersected_line_indices;
};

/**
 * @brief Abstract intersection query interface.
 *
 * Implementations may run on the CPU or dispatch to a GPU compute shader.
 * Consumers call @c intersect() identically regardless of backend.
 */
class ILineBatchIntersector {
public:
    virtual ~ILineBatchIntersector() = default;

    /**
     * @brief Find all lines in @p batch whose segments intersect the query line.
     *
     * Only visible lines (visibility_mask == 1) are tested.
     *
     * @param batch  The batch of line segments to test against.
     * @param query  The query line and transform parameters.
     * @return Indices of intersected lines (0-based into @c batch.lines).
     */
    [[nodiscard]] virtual LineIntersectionResult intersect(
        LineBatchData const & batch,
        LineIntersectionQuery const & query) const = 0;
};

} // namespace CorePlotting

#endif // COREPLOTTING_LINEBATCH_ILINEBATCHINTERSECTOR_HPP
