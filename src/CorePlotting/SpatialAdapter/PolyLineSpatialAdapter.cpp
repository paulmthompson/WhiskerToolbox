#include "PolyLineSpatialAdapter.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace CorePlotting {

std::unique_ptr<QuadTree<EntityId>> PolyLineSpatialAdapter::buildFromVertices(
    RenderablePolyLineBatch const& batch,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Insert every vertex from each line
    size_t const num_lines = batch.line_start_indices.size();
    for (size_t line_idx = 0; line_idx < num_lines; ++line_idx) {
        int32_t const start_idx = batch.line_start_indices[line_idx];
        int32_t const vertex_count = batch.line_vertex_counts[line_idx];
        
        // Get EntityId for this line
        EntityId entity_id = batch.global_entity_id;
        if (!batch.entity_ids.empty() && line_idx < batch.entity_ids.size()) {
            entity_id = batch.entity_ids[line_idx];
        }
        
        // Insert all vertices for this line
        for (int32_t i = 0; i < vertex_count; ++i) {
            size_t const vertex_idx = static_cast<size_t>(start_idx + i) * 2;
            float const x = batch.vertices[vertex_idx];
            float const y = batch.vertices[vertex_idx + 1];
            index->insert(x, y, entity_id);
        }
    }
    
    return index;
}

std::unique_ptr<QuadTree<EntityId>> PolyLineSpatialAdapter::buildFromBoundingBoxes(
    RenderablePolyLineBatch const& batch,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Compute AABB for each line and insert corner points
    size_t const num_lines = batch.line_start_indices.size();
    for (size_t line_idx = 0; line_idx < num_lines; ++line_idx) {
        int32_t const start_idx = batch.line_start_indices[line_idx];
        int32_t const vertex_count = batch.line_vertex_counts[line_idx];
        
        if (vertex_count == 0) {
            continue;
        }
        
        // Get EntityId for this line
        EntityId entity_id = batch.global_entity_id;
        if (!batch.entity_ids.empty() && line_idx < batch.entity_ids.size()) {
            entity_id = batch.entity_ids[line_idx];
        }
        
        // Initialize bounding box from first vertex
        size_t const first_vertex_idx = static_cast<size_t>(start_idx) * 2;
        float min_x = batch.vertices[first_vertex_idx];
        float max_x = batch.vertices[first_vertex_idx];
        float min_y = batch.vertices[first_vertex_idx + 1];
        float max_y = batch.vertices[first_vertex_idx + 1];
        
        // Expand bounding box to include all vertices
        for (int32_t i = 1; i < vertex_count; ++i) {
            size_t const vertex_idx = static_cast<size_t>(start_idx + i) * 2;
            float const x = batch.vertices[vertex_idx];
            float const y = batch.vertices[vertex_idx + 1];
            
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
        }
        
        // Insert four corners of the AABB
        index->insert(min_x, min_y, entity_id);
        index->insert(max_x, min_y, entity_id);
        index->insert(min_x, max_y, entity_id);
        index->insert(max_x, max_y, entity_id);
        
        // Also insert center point for better coverage
        float const center_x = (min_x + max_x) * 0.5f;
        float const center_y = (min_y + max_y) * 0.5f;
        index->insert(center_x, center_y, entity_id);
    }
    
    return index;
}

std::unique_ptr<QuadTree<EntityId>> PolyLineSpatialAdapter::buildFromSampledPoints(
    RenderablePolyLineBatch const& batch,
    float sample_interval,
    BoundingBox const& bounds) {
    
    auto index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    if (sample_interval <= 0.0f) {
        return index;  // Invalid sample interval
    }
    
    // Sample points along each line
    size_t const num_lines = batch.line_start_indices.size();
    for (size_t line_idx = 0; line_idx < num_lines; ++line_idx) {
        int32_t const start_idx = batch.line_start_indices[line_idx];
        int32_t const vertex_count = batch.line_vertex_counts[line_idx];
        
        // Get EntityId for this line
        EntityId entity_id = batch.global_entity_id;
        if (!batch.entity_ids.empty() && line_idx < batch.entity_ids.size()) {
            entity_id = batch.entity_ids[line_idx];
        }
        
        if (vertex_count < 2) {
            // Single vertex or empty - just insert as-is
            if (vertex_count == 1) {
                size_t const vertex_idx = static_cast<size_t>(start_idx) * 2;
                index->insert(batch.vertices[vertex_idx], batch.vertices[vertex_idx + 1], entity_id);
            }
            continue;
        }
        
        // Always insert first vertex
        size_t const first_vertex_idx = static_cast<size_t>(start_idx) * 2;
        index->insert(batch.vertices[first_vertex_idx], batch.vertices[first_vertex_idx + 1], entity_id);
        
        // Walk along line segments, sampling at intervals
        float accumulated_distance = 0.0f;
        for (int32_t i = 1; i < vertex_count; ++i) {
            size_t const start_vertex_idx = static_cast<size_t>(start_idx + i - 1) * 2;
            size_t const end_vertex_idx = static_cast<size_t>(start_idx + i) * 2;
            
            glm::vec2 const start{batch.vertices[start_vertex_idx], batch.vertices[start_vertex_idx + 1]};
            glm::vec2 const end{batch.vertices[end_vertex_idx], batch.vertices[end_vertex_idx + 1]};
            
            glm::vec2 const segment = end - start;
            float const segment_length = glm::length(segment);
            
            if (segment_length < 1e-6f) {
                continue;  // Skip degenerate segments
            }
            
            glm::vec2 const direction = segment / segment_length;
            
            // Sample along this segment
            float distance_along_segment = sample_interval - accumulated_distance;
            while (distance_along_segment < segment_length) {
                glm::vec2 const sample_point = start + direction * distance_along_segment;
                index->insert(sample_point.x, sample_point.y, entity_id);
                distance_along_segment += sample_interval;
            }
            
            // Update accumulated distance for next segment
            accumulated_distance = segment_length - (distance_along_segment - sample_interval);
            
            // Always insert last vertex
            if (i == vertex_count - 1) {
                index->insert(end.x, end.y, entity_id);
            }
        }
    }
    
    return index;
}

} // namespace CorePlotting
