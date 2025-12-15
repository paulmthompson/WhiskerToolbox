#pragma once

#include "Entity/EntityTypes.hpp"
#include "SpatialIndex/QuadTree.hpp"
#include "../SceneGraph/RenderablePrimitives.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace CorePlotting {

/**
 * @brief Builds spatial indexes from glyph batches (points, events)
 * 
 * Creates QuadTree<EntityId> for spatial queries on glyph positions.
 */
class PointSpatialAdapter {
public:
    /**
     * @brief Build spatial index from a RenderableGlyphBatch
     * 
     * @param batch Glyph batch containing positions and entity IDs
     * @param bounds BoundingBox for the QuadTree
     * @return QuadTree with (x,y) = positions, data = EntityId
     */
    static std::unique_ptr<QuadTree<EntityId>> buildFromGlyphs(
        RenderableGlyphBatch const& batch,
        BoundingBox const& bounds);
    
    /**
     * @brief Build spatial index from explicit coordinates
     * 
     * @param positions 2D positions for each point
     * @param entity_ids EntityId for each point (must match positions size)
     * @param bounds BoundingBox for the QuadTree
     * @return QuadTree with (x,y) = positions, data = EntityId
     */
    static std::unique_ptr<QuadTree<EntityId>> buildFromPositions(
        std::vector<glm::vec2> const& positions,
        std::vector<EntityId> const& entity_ids,
        BoundingBox const& bounds);
};

} // namespace CorePlotting
