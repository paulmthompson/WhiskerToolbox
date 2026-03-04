#ifndef COREPLOTTING_SELECTION_POLYGONSELECTION_HPP
#define COREPLOTTING_SELECTION_POLYGONSELECTION_HPP

/**
 * @file PolygonSelection.hpp
 * @brief Pure geometry utility for finding points inside a polygon
 *
 * Uses the ray-casting algorithm via CoreGeometry::Polygon::containsPoint().
 * No Qt or OpenGL dependency — suitable for unit testing and sharing across
 * ScatterPlotWidget, TemporalProjectionViewWidget, etc.
 *
 * @see CoreGeometry/polygon.hpp for the underlying containment test
 */

#include <cstddef>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

namespace CorePlotting::Selection {

/**
 * @brief Result of a polygon selection query
 */
struct PolygonSelectionResult {
    std::vector<std::size_t> selected_indices;  ///< Indices into the point arrays
};

/**
 * @brief Find all points that lie inside a polygon
 *
 * Uses the ray-casting (even-odd) algorithm for point-in-polygon containment.
 * Points exactly on the polygon boundary may or may not be included
 * (consistent with the standard ray-casting behavior).
 *
 * @param polygon_vertices Vertices of the selection polygon (at least 3)
 * @param x_values X coordinates of the candidate points
 * @param y_values Y coordinates of the candidate points (same length as x_values)
 * @return PolygonSelectionResult containing indices of points inside the polygon
 *
 * @pre polygon_vertices.size() >= 3
 * @pre x_values.size() == y_values.size()
 */
[[nodiscard]] PolygonSelectionResult selectPointsInPolygon(
    std::span<glm::vec2 const> polygon_vertices,
    std::span<float const> x_values,
    std::span<float const> y_values);

}  // namespace CorePlotting::Selection

#endif  // COREPLOTTING_SELECTION_POLYGONSELECTION_HPP
