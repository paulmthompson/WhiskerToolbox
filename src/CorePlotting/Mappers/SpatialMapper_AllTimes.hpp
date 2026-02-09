#ifndef COREPLOTTING_MAPPERS_SPATIALMAPPER_ALLTIMES_HPP
#define COREPLOTTING_MAPPERS_SPATIALMAPPER_ALLTIMES_HPP

#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "SpatialMapper.hpp"

#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <vector>

namespace CorePlotting::SpatialMapper {

/**
 * @brief Flatten all points across all time frames into a single vector
 *
 * Iterates every time frame that contains point data and maps all points
 * at each time to world-space coordinates. The result is a single flat
 * vector suitable for static rendering (TemporalProjectionView).
 *
 * @param points Point data source
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of MappedElement containing all points across all times
 */
[[nodiscard]] inline std::vector<MappedElement> mapAllPoints(
    PointData const & points,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<MappedElement> result;

    // Reserve an estimate based on total entry count
    result.reserve(points.getTotalEntryCount());

    for (auto const time : points.getTimesWithData()) {
        auto mapped_at_time = mapPointsAtTime(points, time, x_scale, y_scale, x_offset, y_offset);
        result.insert(result.end(),
                      std::make_move_iterator(mapped_at_time.begin()),
                      std::make_move_iterator(mapped_at_time.end()));
    }

    return result;
}

/**
 * @brief Flatten all lines across all time frames into a single vector
 *
 * Iterates every time frame that contains line data and maps all lines
 * at each time to world-space OwningLineViews. The result is a single flat
 * vector suitable for static rendering (TemporalProjectionView).
 *
 * @param lines Line data source
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of OwningLineView containing all lines across all times
 */
[[nodiscard]] inline std::vector<OwningLineView> mapAllLines(
    LineData const & lines,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<OwningLineView> result;

    // Reserve an estimate based on total entry count
    result.reserve(lines.getTotalEntryCount());

    for (auto const time : lines.getTimesWithData()) {
        auto mapped_at_time = mapLinesAtTime(lines, time, x_scale, y_scale, x_offset, y_offset);
        result.insert(result.end(),
                      std::make_move_iterator(mapped_at_time.begin()),
                      std::make_move_iterator(mapped_at_time.end()));
    }

    return result;
}

} // namespace CorePlotting::SpatialMapper

#endif // COREPLOTTING_MAPPERS_SPATIALMAPPER_ALLTIMES_HPP
