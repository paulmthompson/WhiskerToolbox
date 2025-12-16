#ifndef COREPLOTTING_MAPPERS_SPATIALMAPPER_HPP
#define COREPLOTTING_MAPPERS_SPATIALMAPPER_HPP

#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "MapperConcepts.hpp"
#include "Layout/SeriesLayout.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>

namespace CorePlotting {

/**
 * @brief Mapper for spatial data (SpatialOverlay-style visualization)
 * 
 * Transforms spatial data (PointData, LineData) directly to world-space
 * coordinates. Unlike TimeSeriesMapper, X and Y coordinates come directly
 * from the data's spatial coordinates, not from time conversion.
 * 
 * This mapper is used for:
 * - SpatialOverlay: displaying points, lines, masks over video frames
 * - Scatter plots: displaying point clouds
 * - Any visualization where X/Y are spatial, not temporal
 * 
 * All methods return ranges for zero-copy single-traversal consumption.
 */
namespace SpatialMapper {

// ============================================================================
// Point Mapping: PointData → MappedElement range
// ============================================================================

/**
 * @brief Map points at a specific time to world-space positions (materialized)
 * 
 * Extracts points from PointData at the given time frame index and
 * transforms to world coordinates with optional scaling. Returns a vector
 * since we need to zip data and entity IDs together.
 * 
 * @param points Point data source
 * @param time Current time frame index
 * @param x_scale X coordinate scale (e.g., for aspect ratio correction)
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of MappedElement
 */
[[nodiscard]] inline std::vector<MappedElement> mapPointsAtTime(
    PointData const & points,
    TimeFrameIndex time,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<MappedElement> result;
    
    auto point_data = points.getAtTime(time);
    auto entity_ids = points.getEntityIdsAtTime(time);
    
    // Iterate through both ranges together
    auto data_it = std::ranges::begin(point_data);
    auto data_end = std::ranges::end(point_data);
    auto id_it = std::ranges::begin(entity_ids);
    
    while (data_it != data_end) {
        Point2D<float> const & pt = *data_it;
        EntityId eid = *id_it;
        
        result.emplace_back(
            pt.x * x_scale + x_offset,
            pt.y * y_scale + y_offset,
            eid
        );
        
        ++data_it;
        ++id_it;
    }
    
    return result;
}

/**
 * @brief Map a single Point2D to MappedElement
 * 
 * Direct conversion with scaling/offset.
 * 
 * @param point Source point
 * @param entity_id Entity identifier
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset
 * @param y_offset Y offset
 * @return MappedElement with transformed coordinates
 */
[[nodiscard]] inline MappedElement mapPoint(
    Point2D<float> const & point,
    EntityId entity_id,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    return MappedElement{
        point.x * x_scale + x_offset,
        point.y * y_scale + y_offset,
        entity_id
    };
}

/**
 * @brief Map a range of Point2D to MappedElement range
 * 
 * @tparam R Range type yielding Point2D<float>
 * @param points Range of points
 * @param get_entity_id Function to get EntityId for each point (called with index)
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset
 * @param y_offset Y offset
 * @return Range of MappedElement
 */
template<std::ranges::input_range R>
requires requires(std::ranges::range_value_t<R> pt) {
    { pt.x } -> std::convertible_to<float>;
    { pt.y } -> std::convertible_to<float>;
}
[[nodiscard]] inline auto mapPoints(
    R const & points,
    std::function<EntityId(size_t)> get_entity_id,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    size_t index = 0;
    return points | std::views::transform([=, &index, get_entity_id = std::move(get_entity_id)](auto const & pt) mutable {
        return MappedElement{
            pt.x * x_scale + x_offset,
            pt.y * y_scale + y_offset,
            get_entity_id(index++)
        };
    });
}

// ============================================================================
// Line Mapping: LineData → MappedLineView range  
// ============================================================================

/**
 * @brief Map lines at a specific time to line views
 * 
 * Each line in LineData at the given time becomes an OwningLineView
 * with transformed vertices.
 * 
 * @param lines Line data source
 * @param time Current time frame index
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset
 * @param y_offset Y offset
 * @return Vector of OwningLineView (vertices materialized for stable iteration)
 */
[[nodiscard]] inline std::vector<OwningLineView> mapLinesAtTime(
    LineData const & lines,
    TimeFrameIndex time,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<OwningLineView> result;
    
    auto line_data = lines.getAtTime(time);
    auto entity_ids = lines.getEntityIdsAtTime(time);
    
    // Iterate through both ranges together
    auto data_it = std::ranges::begin(line_data);
    auto data_end = std::ranges::end(line_data);
    auto id_it = std::ranges::begin(entity_ids);
    
    while (data_it != data_end) {
        Line2D const & line = *data_it;
        EntityId eid = *id_it;
        
        std::vector<MappedVertex> vertices;
        vertices.reserve(line.size());
        
        for (auto const & pt : line) {
            vertices.emplace_back(
                pt.x * x_scale + x_offset,
                pt.y * y_scale + y_offset
            );
        }
        
        result.emplace_back(eid, std::move(vertices));
        
        ++data_it;
        ++id_it;
    }
    
    return result;
}

/**
 * @brief Map a single Line2D to a line view
 * 
 * Creates an OwningLineView from a Line2D with coordinate transformation.
 * 
 * @param line Source line
 * @param entity_id Entity identifier for this line
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset
 * @param y_offset Y offset
 * @return OwningLineView with transformed vertices
 */
[[nodiscard]] inline OwningLineView mapLine(
    Line2D const & line,
    EntityId entity_id,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<MappedVertex> vertices;
    vertices.reserve(line.size());
    
    for (auto const & pt : line) {
        vertices.emplace_back(
            pt.x * x_scale + x_offset,
            pt.y * y_scale + y_offset
        );
    }
    
    return OwningLineView{entity_id, std::move(vertices)};
}

/**
 * @brief Create a lazy line view (no vertex materialization)
 * 
 * For cases where you want to iterate once without storing vertices.
 * The returned view applies transforms lazily during iteration.
 * 
 * @param line Source line  
 * @param entity_id Entity identifier
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset
 * @param y_offset Y offset
 * @return MappedLineView with lazy transformation
 */
[[nodiscard]] inline auto mapLineLazy(
    Line2D const & line,
    EntityId entity_id,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    return makeLineView(entity_id, line, x_scale, y_scale, x_offset, y_offset);
}

// ============================================================================
// Batch Mapping Utilities
// ============================================================================

/**
 * @brief Extract entity IDs from a range of mapped elements
 * 
 * Utility for building spatial indices.
 * 
 * @tparam R Range type (MappedElementRange)
 * @param elements Range of mapped elements
 * @return Vector of EntityId
 */
template<MappedElementRange R>
[[nodiscard]] inline std::vector<EntityId> extractEntityIds(R const & elements) {
    std::vector<EntityId> result;
    
    for (auto const & elem : elements) {
        result.push_back(elem.entity_id);
    }
    
    return result;
}

/**
 * @brief Extract positions from a range of mapped elements
 * 
 * Utility for building glyph batches.
 * 
 * @tparam R Range type (MappedElementRange)
 * @param elements Range of mapped elements
 * @return Vector of glm::vec2 positions
 */
template<MappedElementRange R>
[[nodiscard]] inline std::vector<glm::vec2> extractPositions(R const & elements) {
    std::vector<glm::vec2> result;
    
    for (auto const & elem : elements) {
        result.push_back(elem.position());
    }
    
    return result;
}

} // namespace SpatialMapper

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_SPATIALMAPPER_HPP
