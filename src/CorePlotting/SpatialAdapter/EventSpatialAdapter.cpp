#include "EventSpatialAdapter.hpp"
#include <glm/glm.hpp>

namespace CorePlotting {

std::unique_ptr<QuadTree<EntityId>> EventSpatialAdapter::buildStacked(
    DigitalEventSeries const& series,
    TimeFrame const& time_frame,
    SeriesLayout const& layout,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Get all events with their entity IDs
    auto const event_view = series.view();
    float const row_y = layout.result.allocated_y_center;
    
    for (auto const& event : event_view) {
        // Convert to absolute time
        int const time = time_frame.getTimeAtIndex(event.event_time);
        float const x = static_cast<float>(time);
        
        // Insert at absolute time, row Y position
        index->insert(x, row_y, event.entity_id);
    }
    
    return index;
}

std::unique_ptr<QuadTree<EntityId>> EventSpatialAdapter::buildRaster(
    DigitalEventSeries const& series,
    TimeFrame const& time_frame,
    std::vector<SeriesLayout> const& row_layouts,
    std::vector<int64_t> const& row_centers,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    if (row_layouts.size() != row_centers.size()) {
        return index;  // Empty index on size mismatch
    }
    
    // Get all events with their entity IDs
    auto const event_view = series.view();
    
    // For each row, place events relative to row center
    for (size_t row_idx = 0; row_idx < row_centers.size(); ++row_idx) {
        int64_t const row_center = row_centers[row_idx];
        float const row_y = row_layouts[row_idx].result.allocated_y_center;
        
        for (auto const& event : event_view) {
            // Convert to absolute time
            int const event_time = time_frame.getTimeAtIndex(event.event_time);
            
            // Calculate relative time (x position)
            float const relative_time = static_cast<float>(event_time - row_center);
            
            // Insert at relative time, row Y position
            index->insert(relative_time, row_y, event.entity_id);
        }
    }
    
    return index;
}

std::unique_ptr<QuadTree<EntityId>> EventSpatialAdapter::buildFromPositions(
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
