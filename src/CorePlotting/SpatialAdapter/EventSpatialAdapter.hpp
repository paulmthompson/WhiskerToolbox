#ifndef COREPLOTTING_SPATIALADAPTER_EVENTSPATIALADAPTER_HPP
#define COREPLOTTING_SPATIALADAPTER_EVENTSPATIALADAPTER_HPP

#include "SpatialIndex/QuadTree.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "../Layout/SeriesLayout.hpp"
#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Builds QuadTree spatial index from DigitalEventSeries
 * 
 * Creates a spatial index for event visualization, supporting both:
 * 1. Stacked events (single row, absolute time)
 * 2. Raster plots (multiple rows, relative time per trial)
 * 
 * ARCHITECTURE:
 * - Events are inserted at (time, row_y) coordinates
 * - Time can be absolute or relative to row center
 * - EntityId from events enables frame jumping
 * - Y position comes from SeriesLayoutResult
 * 
 * USAGE:
 * ```cpp
 * // Stacked events (single row)
 * auto index = EventSpatialAdapter::buildStacked(
 *     event_series, time_frame, layout, bounds);
 * 
 * // Raster plot (multiple rows)
 * auto index = EventSpatialAdapter::buildRaster(
 *     event_series, time_frame, row_layouts, row_centers, bounds);
 * ```
 */
class EventSpatialAdapter {
public:
    /**
     * @brief Build spatial index for stacked event visualization
     * 
     * Events are positioned at absolute time with Y from layout.
     * All events share the same Y position (one row).
     * 
     * @param series Event series to index
     * @param time_frame Time frame for converting indices to time
     * @param layout Layout position for the event row
     * @param bounds World-space bounds for QuadTree
     * @return Spatial index with all events
     */
    static std::unique_ptr<QuadTree<EntityId>> buildStacked(
        DigitalEventSeries const& series,
        TimeFrame const& time_frame,
        SeriesLayout const& layout,
        BoundingBox const& bounds);

    /**
     * @brief Build spatial index for raster plot visualization
     * 
     * Events are positioned at relative time (relative to row center)
     * with Y from corresponding row layout. The same event can appear
     * in multiple rows at different positions.
     * 
     * @param series Event series to index
     * @param time_frame Time frame for converting indices to time
     * @param row_layouts Layout positions for each row
     * @param row_centers Time center for each row (typically trial start times)
     * @param bounds World-space bounds for QuadTree
     * @return Spatial index with events positioned across rows
     */
    static std::unique_ptr<QuadTree<EntityId>> buildRaster(
        DigitalEventSeries const& series,
        TimeFrame const& time_frame,
        std::vector<SeriesLayout> const& row_layouts,
        std::vector<int64_t> const& row_centers,
        BoundingBox const& bounds);

    /**
     * @brief Build spatial index from explicit coordinates
     * 
     * Useful for testing or when positions are already calculated.
     * 
     * @param positions World-space (x, y) positions for each event
     * @param entity_ids EntityId for each event (parallel to positions)
     * @param bounds World-space bounds for QuadTree
     * @return Spatial index
     */
    static std::unique_ptr<QuadTree<EntityId>> buildFromPositions(
        std::vector<glm::vec2> const& positions,
        std::vector<EntityId> const& entity_ids,
        BoundingBox const& bounds);
};

} // namespace CorePlotting

#endif // COREPLOTTING_SPATIALADAPTER_EVENTSPATIALADAPTER_HPP
