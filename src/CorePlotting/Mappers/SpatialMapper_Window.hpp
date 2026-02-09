#ifndef COREPLOTTING_MAPPERS_SPATIALMAPPER_WINDOW_HPP
#define COREPLOTTING_MAPPERS_SPATIALMAPPER_WINDOW_HPP

#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "MaskContourMapper.hpp"
#include "SpatialMapper.hpp"

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace CorePlotting {

// ============================================================================
// Timed element types — carry temporal distance for alpha computation
// ============================================================================

/**
 * @brief MappedElement with temporal distance from the center of a window
 *
 * Used by OnionSkinView to compute per-element alpha based on how
 * far the element's time frame is from the current time position.
 * temporal_distance == 0 means the element is at the center (current time).
 */
struct TimedMappedElement {
    float x;                ///< X position in world space
    float y;                ///< Y position in world space
    EntityId entity_id;     ///< Entity identifier for hit testing
    int temporal_distance;  ///< Signed distance from window center (in time indices)

    TimedMappedElement() = default;

    TimedMappedElement(float x_, float y_, EntityId id, int dist)
        : x(x_), y(y_), entity_id(id), temporal_distance(dist) {}

    /**
     * @brief Construct from a MappedElement plus temporal distance
     */
    TimedMappedElement(MappedElement const & elem, int dist)
        : x(elem.x), y(elem.y), entity_id(elem.entity_id), temporal_distance(dist) {}

    /**
     * @brief Convert to glm::vec2 for rendering
     */
    [[nodiscard]] glm::vec2 position() const {
        return {x, y};
    }

    /**
     * @brief Absolute temporal distance (for alpha computation)
     */
    [[nodiscard]] int absTemporalDistance() const {
        return std::abs(temporal_distance);
    }
};

/**
 * @brief OwningLineView with temporal distance from the center of a window
 *
 * Used by OnionSkinView to compute per-line alpha based on how far
 * the line's time frame is from the current time position.
 */
class TimedOwningLineView {
public:
    EntityId entity_id;     ///< Entity identifier for the entire line
    int temporal_distance;  ///< Signed distance from window center (in time indices)

    /**
     * @brief Construct with ownership of vertex data
     */
    TimedOwningLineView(EntityId id, std::vector<MappedVertex> verts, int dist)
        : entity_id(id)
        , temporal_distance(dist)
        , _vertices(std::move(verts)) {}

    /**
     * @brief Construct from an existing OwningLineView plus temporal distance
     */
    TimedOwningLineView(OwningLineView && view, int dist)
        : entity_id(view.entity_id)
        , temporal_distance(dist)
        , _vertices(std::move(view.verticesMut())) {}

    /**
     * @brief Get vertex range for iteration
     */
    [[nodiscard]] std::span<MappedVertex const> vertices() const {
        return _vertices;
    }

    /**
     * @brief Absolute temporal distance (for alpha computation)
     */
    [[nodiscard]] int absTemporalDistance() const {
        return std::abs(temporal_distance);
    }

private:
    std::vector<MappedVertex> _vertices;
};

// ============================================================================
// Windowed mapping functions
// ============================================================================

namespace SpatialMapper {

/**
 * @brief Map points within a temporal window around a center time
 *
 * Gathers points from [center - behind, center + ahead] and tags each
 * with its signed temporal distance from center. Used by OnionSkinView
 * for alpha-graded rendering.
 *
 * @param points Point data source
 * @param center Center time frame index (current position)
 * @param behind Number of time indices to look behind (positive value)
 * @param ahead Number of time indices to look ahead (positive value)
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of TimedMappedElement with temporal_distance set
 */
[[nodiscard]] inline std::vector<TimedMappedElement> mapPointsInWindow(
    PointData const & points,
    TimeFrameIndex center,
    int behind,
    int ahead,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<TimedMappedElement> result;

    int64_t const center_val = center.getValue();
    int64_t const start = center_val - static_cast<int64_t>(behind);
    int64_t const end = center_val + static_cast<int64_t>(ahead);

    for (auto const time : points.getTimesWithData()) {
        int64_t const t = time.getValue();
        if (t < start || t > end) {
            continue;
        }

        int const dist = static_cast<int>(t - center_val);
        auto mapped = mapPointsAtTime(points, time, x_scale, y_scale, x_offset, y_offset);
        for (auto & elem : mapped) {
            result.emplace_back(elem, dist);
        }
    }

    return result;
}

/**
 * @brief Map lines within a temporal window around a center time
 *
 * Gathers lines from [center - behind, center + ahead] and tags each
 * with its signed temporal distance from center.
 *
 * @param lines Line data source
 * @param center Center time frame index (current position)
 * @param behind Number of time indices to look behind (positive value)
 * @param ahead Number of time indices to look ahead (positive value)
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of TimedOwningLineView with temporal_distance set
 */
[[nodiscard]] inline std::vector<TimedOwningLineView> mapLinesInWindow(
    LineData const & lines,
    TimeFrameIndex center,
    int behind,
    int ahead,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<TimedOwningLineView> result;

    int64_t const center_val = center.getValue();
    int64_t const start = center_val - static_cast<int64_t>(behind);
    int64_t const end = center_val + static_cast<int64_t>(ahead);

    for (auto const time : lines.getTimesWithData()) {
        int64_t const t = time.getValue();
        if (t < start || t > end) {
            continue;
        }

        int const dist = static_cast<int>(t - center_val);
        auto mapped = mapLinesAtTime(lines, time, x_scale, y_scale, x_offset, y_offset);
        for (auto & line_view : mapped) {
            result.emplace_back(std::move(line_view), dist);
        }
    }

    return result;
}

/**
 * @brief Map mask contours within a temporal window around a center time
 *
 * Gathers masks from [center - behind, center + ahead], converts each
 * to a contour polyline via MaskContourMapper, and tags each with its
 * signed temporal distance from center.
 *
 * @param masks Mask data source
 * @param center Center time frame index (current position)
 * @param behind Number of time indices to look behind (positive value)
 * @param ahead Number of time indices to look ahead (positive value)
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of TimedOwningLineView with temporal_distance set
 */
[[nodiscard]] inline std::vector<TimedOwningLineView> mapMaskContoursInWindow(
    MaskData const & masks,
    TimeFrameIndex center,
    int behind,
    int ahead,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<TimedOwningLineView> result;

    int64_t const center_val = center.getValue();
    int64_t const start = center_val - static_cast<int64_t>(behind);
    int64_t const end = center_val + static_cast<int64_t>(ahead);

    for (auto const time : masks.getTimesWithData()) {
        int64_t const t = time.getValue();
        if (t < start || t > end) {
            continue;
        }

        int const dist = static_cast<int>(t - center_val);
        auto contours = MaskContourMapper::mapMaskContoursAtTime(
            masks, time, x_scale, y_scale, x_offset, y_offset);
        for (auto & contour : contours) {
            result.emplace_back(std::move(contour), dist);
        }
    }

    return result;
}

} // namespace SpatialMapper
} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_SPATIALMAPPER_WINDOW_HPP
