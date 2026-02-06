#include "SceneHitTester.hpp"

namespace CorePlotting {

HitTestResult SceneHitTester::hitTest(
        float world_x,
        float world_y,
        RenderableScene const & scene,
        LayoutResponse const & layout) const {
    HitTestResult best_result = HitTestResult::noHit();

    // Strategy 1: Query QuadTree for discrete elements (events, points)
    if (scene.spatial_index) {
        HitTestResult quad_result = queryQuadTree(world_x, world_y, scene);
        best_result = selectBestHit(best_result, quad_result);
    }

    // Strategy 2: Query rectangle batches for intervals
    // Note: For a complete implementation, the caller should provide the series_key_map
    // For now, we use a simple index-based approach
    // (In practice, the widget maintains this mapping when building the scene)

    // Strategy 3: Query series regions (always do this as fallback)
    HitTestResult region_result = querySeriesRegion(world_x, world_y, layout);

    // If we have a discrete hit, it takes priority over region hits
    if (_config.prioritize_discrete && best_result.isDiscrete()) {
        return best_result;
    }

    return selectBestHit(best_result, region_result);
}

HitTestResult SceneHitTester::queryQuadTree(
        float world_x,
        float world_y,
        RenderableScene const & scene) const {
    if (!scene.spatial_index) {
        return HitTestResult::noHit();
    }

    // Query the QuadTree for the nearest point within tolerance
    auto * nearest = scene.spatial_index->findNearest(world_x, world_y, _config.point_tolerance);

    if (nearest) {
        float dx = nearest->x - world_x;
        float dy = nearest->y - world_y;
        float dist = std::sqrt(dx * dx + dy * dy);

        // Lookup series_key from entity_to_series_key map
        std::string series_key;
        auto it = scene.entity_to_series_key.find(nearest->data);
        if (it != scene.entity_to_series_key.end()) {
            series_key = it->second;
        }

        // Create an event hit result
        return HitTestResult::eventHit(
                series_key,
                nearest->data,// QuadTree<EntityId> stores EntityId in .data
                dist,
                nearest->x,
                nearest->y);
    }

    return HitTestResult::noHit();
}

HitTestResult SceneHitTester::queryIntervals(
        float world_x,
        float world_y,
        RenderableScene const & scene,
        std::map<size_t, std::string> const & series_key_map) const {
    HitTestResult best = HitTestResult::noHit();

    for (size_t batch_idx = 0; batch_idx < scene.rectangle_batches.size(); ++batch_idx) {
        auto const & batch = scene.rectangle_batches[batch_idx];

        // Get series key for this batch
        std::string series_key;
        auto key_it = series_key_map.find(batch_idx);
        if (key_it != series_key_map.end()) {
            series_key = key_it->second;
        }

        // Check each rectangle in the batch
        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            auto const & rect = batch.bounds[i];
            float rect_x = rect.x;
            float rect_y = rect.y;
            float rect_w = rect.z;
            float rect_h = rect.w;

            // Check if world_x is within interval time range
            if (world_x >= rect_x && world_x <= (rect_x + rect_w)) {
                // Calculate distance (for Y, check if within height)
                float y_dist = 0.0f;
                if (world_y < rect_y) {
                    y_dist = rect_y - world_y;
                } else if (world_y > rect_y + rect_h) {
                    y_dist = world_y - (rect_y + rect_h);
                }

                float dist = y_dist;// For intervals, X is always inside

                EntityId entity_id{0};
                if (i < batch.entity_ids.size()) {
                    entity_id = batch.entity_ids[i];
                }

                auto result = HitTestResult::intervalBodyHit(
                        series_key,
                        entity_id,
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(rect_x + rect_w),
                        dist);

                best = selectBestHit(best, result);
            }
        }
    }

    return best;
}

HitTestResult SceneHitTester::findIntervalEdgeByEntityId(
        float world_x,
        RenderableScene const & scene,
        std::unordered_set<EntityId> const & selected_entities,
        std::map<size_t, std::string> const & series_key_map) const {
    HitTestResult best = HitTestResult::noHit();

    for (size_t batch_idx = 0; batch_idx < scene.rectangle_batches.size(); ++batch_idx) {
        auto const & batch = scene.rectangle_batches[batch_idx];

        // Get series key for this batch
        std::string series_key;
        auto key_it = series_key_map.find(batch_idx);
        if (key_it != series_key_map.end()) {
            series_key = key_it->second;
        }

        for (size_t i = 0; i < batch.bounds.size(); ++i) {
            // Get EntityId for this interval
            EntityId entity_id{0};
            if (i < batch.entity_ids.size()) {
                entity_id = batch.entity_ids[i];
            }

            // Only check edges of selected intervals (skip if not selected)
            if (!selected_entities.empty() && !selected_entities.contains(entity_id)) {
                continue;
            }

            auto const & rect = batch.bounds[i];
            float rect_x = rect.x;
            float rect_w = rect.z;
            float left_edge = rect_x;
            float right_edge = rect_x + rect_w;

            // Check left edge
            float left_dist = std::abs(world_x - left_edge);
            if (left_dist <= _config.edge_tolerance) {
                auto result = HitTestResult::intervalEdgeHit(
                        series_key,
                        entity_id,
                        true,// is_left_edge
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(right_edge),
                        left_edge,
                        left_dist);
                best = selectBestHit(best, result);
            }

            // Check right edge
            float right_dist = std::abs(world_x - right_edge);
            if (right_dist <= _config.edge_tolerance) {
                auto result = HitTestResult::intervalEdgeHit(
                        series_key,
                        entity_id,
                        false,// is_left_edge (right edge)
                        static_cast<int64_t>(rect_x),
                        static_cast<int64_t>(right_edge),
                        right_edge,
                        right_dist);
                best = selectBestHit(best, result);
            }
        }
    }

    return best;
}

HitTestResult SceneHitTester::querySeriesRegion(
        float world_x,
        float world_y,
        LayoutResponse const & layout) const {
    auto series_result = findSeriesAtWorldY(world_y, layout);

    if (series_result.has_value()) {
        return HitTestResult::analogSeriesHit(
                series_result->series_key,
                world_x,
                world_y,
                series_result->is_within_bounds ? 0.0f : std::abs(series_result->series_local_y));
    }

    return HitTestResult::noHit();
}

HitTestResult SceneHitTester::selectBestHit(
        HitTestResult const & a,
        HitTestResult const & b) const {
    // If one is no-hit, return the other
    if (!a.hasHit()) return b;
    if (!b.hasHit()) return a;

    // Prioritize discrete elements if configured
    if (_config.prioritize_discrete) {
        if (a.isDiscrete() && !b.isDiscrete()) return a;
        if (b.isDiscrete() && !a.isDiscrete()) return b;
    }

    // Prioritize interval edges over bodies (for drag detection)
    if (a.isIntervalEdge() && !b.isIntervalEdge()) return a;
    if (b.isIntervalEdge() && !a.isIntervalEdge()) return b;

    // Otherwise, return the closest one
    return a.isCloserThan(b) ? a : b;
}

}// namespace CorePlotting