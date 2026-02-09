#ifndef COREPLOTTING_MAPPERS_MASKCONTOURMAPPER_HPP
#define COREPLOTTING_MAPPERS_MASKCONTOURMAPPER_HPP

#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "SpatialMapper.hpp"

#include "CoreGeometry/masks.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <vector>

namespace CorePlotting::MaskContourMapper {

/**
 * @brief Convert a single Mask2D to a contour polyline
 *
 * Uses get_mask_outline() to extract ordered boundary points from
 * the sparse pixel mask, then maps them to an OwningLineView.
 * This allows masks to be rendered through the standard polyline
 * pipeline without a dedicated mask renderer.
 *
 * @param mask Source mask (sparse pixel set)
 * @param entity_id Entity identifier for this mask
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return OwningLineView with contour vertices, or empty-vertex view if mask is empty
 */
[[nodiscard]] inline OwningLineView mapMaskContour(
    Mask2D const & mask,
    EntityId entity_id,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    auto outline_points = get_mask_outline(mask);

    std::vector<MappedVertex> vertices;
    vertices.reserve(outline_points.size());

    for (auto const & pt : outline_points) {
        vertices.emplace_back(
            static_cast<float>(pt.x) * x_scale + x_offset,
            static_cast<float>(pt.y) * y_scale + y_offset
        );
    }

    return OwningLineView{entity_id, std::move(vertices)};
}

/**
 * @brief Map all mask contours at a specific time frame
 *
 * Extracts masks from MaskData at the given time, converts each to
 * a contour polyline via get_mask_outline(), and returns the set as
 * OwningLineViews.
 *
 * @param masks Mask data source
 * @param time Current time frame index
 * @param x_scale X coordinate scale
 * @param y_scale Y coordinate scale
 * @param x_offset X offset applied after scaling
 * @param y_offset Y offset applied after scaling
 * @return Vector of OwningLineView, one per mask at this time
 */
[[nodiscard]] inline std::vector<OwningLineView> mapMaskContoursAtTime(
    MaskData const & masks,
    TimeFrameIndex time,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    std::vector<OwningLineView> result;

    auto mask_data = masks.getAtTime(time);
    auto entity_ids = masks.getEntityIdsAtTime(time);

    auto data_it = std::ranges::begin(mask_data);
    auto data_end = std::ranges::end(mask_data);
    auto id_it = std::ranges::begin(entity_ids);

    while (data_it != data_end) {
        Mask2D const & mask = *data_it;
        EntityId eid = *id_it;

        auto contour = mapMaskContour(mask, eid, x_scale, y_scale, x_offset, y_offset);
        result.push_back(std::move(contour));

        ++data_it;
        ++id_it;
    }

    return result;
}

} // namespace CorePlotting::MaskContourMapper

#endif // COREPLOTTING_MAPPERS_MASKCONTOURMAPPER_HPP
