#include "PolygonSelection.hpp"

#include "CoreGeometry/polygon.hpp"

#include <cassert>

namespace CorePlotting::Selection {

PolygonSelectionResult selectPointsInPolygon(
    std::span<glm::vec2 const> polygon_vertices,
    std::span<float const> x_values,
    std::span<float const> y_values) {

    assert(polygon_vertices.size() >= 3);
    assert(x_values.size() == y_values.size());

    PolygonSelectionResult result;

    // Build a CoreGeometry::Polygon from the glm vertices
    std::vector<Point2D<float>> geo_verts;
    geo_verts.reserve(polygon_vertices.size());
    for (auto const & v : polygon_vertices) {
        geo_verts.emplace_back(v.x, v.y);
    }
    Polygon poly(geo_verts);

    // Use the bounding box for a fast rejection test
    auto const bbox = poly.getBoundingBox();

    for (std::size_t i = 0; i < x_values.size(); ++i) {
        float const x = x_values[i];
        float const y = y_values[i];

        // Quick bounding-box rejection
        if (x < bbox.min_x || x > bbox.max_x || y < bbox.min_y || y > bbox.max_y) {
            continue;
        }

        if (poly.containsPoint(Point2D<float>(x, y))) {
            result.selected_indices.push_back(i);
        }
    }

    return result;
}

}  // namespace CorePlotting::Selection
