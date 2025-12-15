#include "PointSpatialAdapter.hpp"

#include <glm/glm.hpp>

namespace CorePlotting {

std::unique_ptr<QuadTree<EntityId>> PointSpatialAdapter::buildFromGlyphs(
    RenderableGlyphBatch const& batch,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Insert all glyph positions with their entity IDs
    for (size_t i = 0; i < batch.positions.size(); ++i) {
        EntityId entity_id = EntityId(0);
        if (!batch.entity_ids.empty() && i < batch.entity_ids.size()) {
            entity_id = batch.entity_ids[i];
        }
        
        index->insert(batch.positions[i].x, batch.positions[i].y, entity_id);
    }
    
    return index;
}

std::unique_ptr<QuadTree<EntityId>> PointSpatialAdapter::buildFromPositions(
    std::vector<glm::vec2> const& positions,
    std::vector<EntityId> const& entity_ids,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    if (positions.size() != entity_ids.size()) {
        return index;  // Empty index on size mismatch
    }
    
    for (size_t i = 0; i < positions.size(); ++i) {
        index->insert(positions[i].x, positions[i].y, entity_ids[i]);
    }
    
    return index;
}

} // namespace CorePlotting
