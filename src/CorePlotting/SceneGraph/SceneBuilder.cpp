#include "SceneBuilder.hpp"

#include "CoordinateTransform/ViewState.hpp"

#include "CoreGeometry/boundingbox.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <stdexcept>

namespace CorePlotting {

SceneBuilder & SceneBuilder::setBounds(BoundingBox const & bounds) {
    _bounds = bounds;
    return *this;
}

SceneBuilder & SceneBuilder::setViewState(ViewState const & state) {
    computeMatricesFromViewState(state, _scene.view_matrix, _scene.projection_matrix);
    _has_matrices = true;
    return *this;
}

SceneBuilder & SceneBuilder::setMatrices(glm::mat4 const & view, glm::mat4 const & projection) {
    _scene.view_matrix = view;
    _scene.projection_matrix = projection;
    _has_matrices = true;
    return *this;
}

// ============================================================================
// High-level series methods
// ============================================================================

SceneBuilder & SceneBuilder::addEventSeries(
        std::string const & series_key,
        DigitalEventSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame) {

    // Create glyph batch for rendering
    RenderableGlyphBatch batch;
    batch.glyph_type = RenderableGlyphBatch::GlyphType::Circle;
    batch.size = 4.0f;// Default glyph size
    batch.model_matrix = glm::mat4(1.0f);

    float const y_center = layout.result.allocated_y_center;

    for (auto const & event: series.view()) {
        float x = static_cast<float>(time_frame.getTimeAtIndex(event.event_time));
        batch.positions.emplace_back(x, y_center);
        batch.entity_ids.push_back(event.entity_id);
    }

    _scene.glyph_batches.push_back(std::move(batch));

    // Store for spatial index construction
    _pending_events.push_back({series_key, &series, &layout, &time_frame});

    return *this;
}

SceneBuilder & SceneBuilder::addIntervalSeries(
        std::string const & series_key,
        DigitalIntervalSeries const & series,
        SeriesLayout const & layout,
        TimeFrame const & time_frame) {

    // Create rectangle batch for rendering
    RenderableRectangleBatch batch;
    batch.model_matrix = glm::mat4(1.0f);

    float const height = layout.result.allocated_height;
    float const y_bottom = layout.result.allocated_y_center - height / 2.0f;

    for (auto const & interval: series.view()) {
        float x_start = static_cast<float>(time_frame.getTimeAtIndex(TimeFrameIndex{interval.interval.start}));
        float x_end = static_cast<float>(time_frame.getTimeAtIndex(TimeFrameIndex{interval.interval.end}));
        float width = x_end - x_start;

        batch.bounds.push_back(glm::vec4(x_start, y_bottom, width, height));
        batch.entity_ids.push_back(interval.entity_id);
    }

    // Track batch index -> series key mapping
    size_t batch_index = _scene.rectangle_batches.size();
    _rectangle_batch_key_map[batch_index] = series_key;

    _scene.rectangle_batches.push_back(std::move(batch));

    // Store for spatial index construction
    _pending_intervals.push_back({series_key, &series, &layout, &time_frame});

    return *this;
}

// ============================================================================
// Low-level batch methods
// ============================================================================

SceneBuilder & SceneBuilder::addPolyLineBatch(RenderablePolyLineBatch const & batch) {
    _scene.poly_line_batches.push_back(batch);
    return *this;
}

SceneBuilder & SceneBuilder::addPolyLineBatch(RenderablePolyLineBatch && batch) {
    _scene.poly_line_batches.push_back(std::move(batch));
    return *this;
}

SceneBuilder & SceneBuilder::addGlyphBatch(RenderableGlyphBatch const & batch) {
    _scene.glyph_batches.push_back(batch);
    return *this;
}

SceneBuilder & SceneBuilder::addGlyphBatch(RenderableGlyphBatch && batch) {
    _scene.glyph_batches.push_back(std::move(batch));
    return *this;
}

SceneBuilder & SceneBuilder::addRectangleBatch(RenderableRectangleBatch const & batch) {
    _scene.rectangle_batches.push_back(batch);
    return *this;
}

SceneBuilder & SceneBuilder::addRectangleBatch(RenderableRectangleBatch && batch) {
    _scene.rectangle_batches.push_back(std::move(batch));
    return *this;
}

SceneBuilder & SceneBuilder::buildSpatialIndex(BoundingBox const & bounds) {
    // Create QuadTree with given bounds
    _scene.spatial_index = std::make_unique<QuadTree<EntityId>>(bounds);

    // Insert polyline vertices
    for (auto const & batch: _scene.poly_line_batches) {
        // Apply Model matrix to vertices for world-space positions
        glm::mat4 const & M = batch.model_matrix;

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
    for (auto const & batch: _scene.glyph_batches) {
        glm::mat4 const & M = batch.model_matrix;

        for (size_t i = 0; i < batch.positions.size(); ++i) {
            glm::vec2 const & pos = batch.positions[i];

            // Transform to world space
            glm::vec4 const world_pos = M * glm::vec4(pos.x, pos.y, 0.0f, 1.0f);

            // Get EntityId for this glyph
            EntityId entity_id = (i < batch.entity_ids.size()) ? batch.entity_ids[i] : EntityId(0);

            _scene.spatial_index->insert(world_pos.x, world_pos.y, entity_id);
        }
    }

    // Insert rectangle centers
    for (auto const & batch: _scene.rectangle_batches) {
        glm::mat4 const & M = batch.model_matrix;

        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            glm::vec4 const & rect = batch.bounds[i];// {x, y, width, height}

            // Use rectangle center for spatial indexing
            float const center_x = rect.x + rect.z * 0.5f;
            float const center_y = rect.y + rect.w * 0.5f;

            // Transform to world space
            glm::vec4 const world_pos = M * glm::vec4(center_x, center_y, 0.0f, 1.0f);

            // Get EntityId for this rectangle
            EntityId entity_id = (i < batch.entity_ids.size()) ? batch.entity_ids[i] : EntityId(0);

            _scene.spatial_index->insert(world_pos.x, world_pos.y, entity_id);
        }
    }

    return *this;
}

void SceneBuilder::buildSpatialIndexFromPendingSeries() {
    if (!_bounds.has_value()) {
        throw std::runtime_error("SceneBuilder: Cannot build spatial index - bounds not set");
    }

    _scene.spatial_index = std::make_unique<QuadTree<EntityId>>(_bounds.value());

    // Insert events
    for (auto const & pending: _pending_events) {
        float const y_center = pending.layout->result.allocated_y_center;

        for (auto const & event: pending.series->view()) {
            float x = static_cast<float>(pending.time_frame->getTimeAtIndex(event.event_time));
            _scene.spatial_index->insert(x, y_center, event.entity_id);
        }
    }

    // Insert interval centers
    for (auto const & pending: _pending_intervals) {
        float const y_center = pending.layout->result.allocated_y_center;

        for (auto const & interval: pending.series->view()) {
            float x_start = static_cast<float>(pending.time_frame->getTimeAtIndex(
                    TimeFrameIndex{interval.interval.start}));
            float x_end = static_cast<float>(pending.time_frame->getTimeAtIndex(
                    TimeFrameIndex{interval.interval.end}));
            float x_center = (x_start + x_end) / 2.0f;

            _scene.spatial_index->insert(x_center, y_center, interval.entity_id);
        }
    }
}

RenderableScene SceneBuilder::build() {
    // Automatically build spatial index if we have pending discrete series
    if (!_pending_events.empty() || !_pending_intervals.empty()) {
        buildSpatialIndexFromPendingSeries();
    }

    RenderableScene result = std::move(_scene);
    reset();
    return result;
}

void SceneBuilder::reset() {
    _scene = RenderableScene{};
    _has_matrices = false;
    _bounds.reset();
    _pending_events.clear();
    _pending_intervals.clear();
    _rectangle_batch_key_map.clear();
}

}// namespace CorePlotting
