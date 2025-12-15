#ifndef COREPLOTTING_SPATIALADAPTER_ISPATIALLYINDEXED_HPP
#define COREPLOTTING_SPATIALADAPTER_ISPATIALLYINDEXED_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <optional>

namespace CorePlotting {

/**
 * @brief Interface for spatially-indexed visualizations
 * 
 * Provides a common interface for widgets that support spatial queries
 * via QuadTree indexing. This enables consistent interaction patterns
 * across different plot types (DataViewer, SpatialOverlay, EventPlot, etc.).
 * 
 * USAGE:
 * ```cpp
 * class MyWidget : public ISpatiallyIndexed {
 *     std::optional<EntityId> findEntityAtPosition(float world_x, float world_y, float tolerance) const override {
 *         return _scene.spatial_index->findNearest(world_x, world_y, tolerance);
 *     }
 *     
 *     std::optional<TimeFrameIndex> getSourceTime(EntityId id) const override {
 *         return _series->getTimeForEntity(id);
 *     }
 * };
 * ```
 */
class ISpatiallyIndexed {
public:
    virtual ~ISpatiallyIndexed() = default;

    /**
     * @brief Find entity at a world-space position
     * 
     * @param world_x X coordinate in world space
     * @param world_y Y coordinate in world space  
     * @param tolerance Search radius in world space units
     * @return EntityId if found, std::nullopt otherwise
     */
    virtual std::optional<EntityId> findEntityAtPosition(
            float world_x,
            float world_y,
            float tolerance) const = 0;

    /**
     * @brief Find all entities within a rectangular region
     * 
     * @param min_x Left boundary in world space
     * @param max_x Right boundary in world space
     * @param min_y Bottom boundary in world space
     * @param max_y Top boundary in world space
     * @return Vector of EntityIds within the region
     */
    virtual std::vector<EntityId> findEntitiesInRegion(
            float min_x, float max_x,
            float min_y, float max_y) const = 0;

    /**
     * @brief Get source time for an entity
     * 
     * Given an EntityId, retrieve the corresponding time in the source
     * time frame. This enables frame jumping from hover/selection.
     * 
     * @param entity_id The entity to query
     * @return TimeFrameIndex if entity has time data, std::nullopt otherwise
     */
    virtual std::optional<TimeFrameIndex> getSourceTime(EntityId entity_id) const = 0;

    /**
     * @brief Check if spatial indexing is currently available
     * 
     * @return true if spatial queries can be performed, false otherwise
     */
    virtual bool hasSpatialIndex() const = 0;
};

}// namespace CorePlotting

#endif// COREPLOTTING_SPATIALADAPTER_ISPATIALLYINDEXED_HPP
