#include "SceneBuilder.hpp"
#include "../CoordinateTransform/ViewState.hpp"
#include "CoreGeometry/boundingbox.hpp"

namespace CorePlotting {

SceneBuilder& SceneBuilder::setViewState(ViewState const& state) {
    computeMatricesFromViewState(state, _scene.view_matrix, _scene.projection_matrix);
    _has_matrices = true;
    return *this;
}

SceneBuilder& SceneBuilder::setMatrices(glm::mat4 const& view, glm::mat4 const& projection) {
    _scene.view_matrix = view;
    _scene.projection_matrix = projection;
    _has_matrices = true;
    return *this;
}

SceneBuilder& SceneBuilder::addPolyLineBatch(RenderablePolyLineBatch const& batch) {
    _scene.poly_line_batches.push_back(batch);
    return *this;
}

SceneBuilder& SceneBuilder::addPolyLineBatch(RenderablePolyLineBatch&& batch) {
    _scene.poly_line_batches.push_back(std::move(batch));
    return *this;
}

SceneBuilder& SceneBuilder::addGlyphBatch(RenderableGlyphBatch const& batch) {
    _scene.glyph_batches.push_back(batch);
    return *this;
}

SceneBuilder& SceneBuilder::addGlyphBatch(RenderableGlyphBatch&& batch) {
    _scene.glyph_batches.push_back(std::move(batch));
    return *this;
}

SceneBuilder& SceneBuilder::addRectangleBatch(RenderableRectangleBatch const& batch) {
    _scene.rectangle_batches.push_back(batch);
    return *this;
}

SceneBuilder& SceneBuilder::addRectangleBatch(RenderableRectangleBatch&& batch) {
    _scene.rectangle_batches.push_back(std::move(batch));
    return *this;
}

SceneBuilder& SceneBuilder::buildSpatialIndex(BoundingBox const& bounds) {
    // Create QuadTree with given bounds
    _scene.spatial_index = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Insert polyline vertices
    for (auto const& batch : _scene.poly_line_batches) {
        // Apply Model matrix to vertices for world-space positions
        glm::mat4 const& M = batch.model_matrix;
        
        for (size_t i = 0; i < batch.vertices.size(); i += 2) {
            float const x = batch.vertices[i];
            float const y = batch.vertices[i + 1];
            
            // Transform to world space
            glm::vec4 const world_pos = M * glm::vec4(x, y, 0.0f, 1.0f);
            
            // Determine EntityId for this vertex
            // Find which line this vertex belongs to
            EntityId entity_id = batch.global_entity_id;
            
            if (!batch.entity_ids.empty()) {
                // Find line index for this vertex
                int vertex_index = static_cast<int>(i / 2);
                int line_index = 0;
                int accumulated_vertices = 0;
                
                for (size_t j = 0; j < batch.line_vertex_counts.size(); ++j) {
                    accumulated_vertices += batch.line_vertex_counts[j];
                    if (vertex_index < accumulated_vertices) {
                        line_index = static_cast<int>(j);
                        break;
                    }
                }
                
                if (line_index < static_cast<int>(batch.entity_ids.size())) {
                    entity_id = batch.entity_ids[line_index];
                }
            }
            
            _scene.spatial_index->insert(world_pos.x, world_pos.y, entity_id);
        }
    }
    
    // Insert glyph positions
    for (auto const& batch : _scene.glyph_batches) {
        glm::mat4 const& M = batch.model_matrix;
        
        for (size_t i = 0; i < batch.positions.size(); ++i) {
            glm::vec2 const& pos = batch.positions[i];
            
            // Transform to world space
            glm::vec4 const world_pos = M * glm::vec4(pos.x, pos.y, 0.0f, 1.0f);
            
            // Get EntityId for this glyph
            EntityId entity_id = (i < batch.entity_ids.size()) ? 
                batch.entity_ids[i] : EntityId(0);
            
            _scene.spatial_index->insert(world_pos.x, world_pos.y, entity_id);
        }
    }
    
    // Insert rectangle centers
    for (auto const& batch : _scene.rectangle_batches) {
        glm::mat4 const& M = batch.model_matrix;
        
        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            glm::vec4 const& rect = batch.bounds[i];  // {x, y, width, height}
            
            // Use rectangle center for spatial indexing
            float const center_x = rect.x + rect.z * 0.5f;
            float const center_y = rect.y + rect.w * 0.5f;
            
            // Transform to world space
            glm::vec4 const world_pos = M * glm::vec4(center_x, center_y, 0.0f, 1.0f);
            
            // Get EntityId for this rectangle
            EntityId entity_id = (i < batch.entity_ids.size()) ? 
                batch.entity_ids[i] : EntityId(0);
            
            _scene.spatial_index->insert(world_pos.x, world_pos.y, entity_id);
        }
    }
    
    return *this;
}

RenderableScene SceneBuilder::build() {
    RenderableScene result = std::move(_scene);
    reset();
    return result;
}

void SceneBuilder::reset() {
    _scene = RenderableScene{};
    _has_matrices = false;
}

} // namespace CorePlotting
