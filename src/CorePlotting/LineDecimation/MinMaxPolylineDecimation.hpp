#ifndef COREPLOTTING_LINEDECIMATION_MINMAXPOLYLINEDECIMATION_HPP
#define COREPLOTTING_LINEDECIMATION_MINMAXPOLYLINEDECIMATION_HPP

/**
 * @file MinMaxPolylineDecimation.hpp
 * @brief Min–max (envelope) decimation for `RenderablePolyLineBatch` line strips.
 *
 * Reduces vertex count for dense polylines (e.g. analog time series) while preserving
 * per-column extrema in x, suitable for display when horizontal resolution is limited.
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

namespace CorePlotting {

/**
 * @brief Parameters for min–max polyline decimation.
 */
struct MinMaxDecimationParams {
    /// Number of x-buckets along each line. @c 0 disables decimation (caller returns a copy).
    int bucket_count{0};
};

/**
 * @brief Apply min–max decimation to each line strip in @p in.
 *
 * For each logical line (parallel @c line_start_indices / @c line_vertex_counts), vertices
 * are partitioned into @c bucket_count uniform buckets along x (or by sample index when
 * x span is degenerate). Each non-empty bucket contributes up to two points: the point
 * with minimum y and the point with maximum y (emit in increasing x order). The first
 * and last vertices of each strip are always included (with consecutive duplicates removed).
 *
 * @param in Source batch (unchanged).
 * @param params @c bucket_count <= 0 returns a copy of @p in.
 * @return Decimated batch; metadata (@c thickness, @c model_matrix, colors, entity ids) is copied from @p in.
 *
 * @pre When @c params.bucket_count > 0, @c in.line_start_indices.size() == @c in.line_vertex_counts.size().
 *      If sizes differ, @p in is returned unchanged.
 * @post Returned batch has the same number of logical lines as @p in when decimation runs.
 */
[[nodiscard]] RenderablePolyLineBatch decimatePolyLineBatchMinMax(
        RenderablePolyLineBatch const & in,
        MinMaxDecimationParams params);

}// namespace CorePlotting

#endif// COREPLOTTING_LINEDECIMATION_MINMAXPOLYLINEDECIMATION_HPP
