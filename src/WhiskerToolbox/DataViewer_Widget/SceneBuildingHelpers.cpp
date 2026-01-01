#include "SceneBuildingHelpers.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/Transformers/GapDetector.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "DigitalTimeSeries/IntervalWithId.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cmath>

namespace DataViewerHelpers {

namespace {

/**
 * @brief Create a local-space layout for model-matrix rendering
 * 
 * Returns a SeriesLayout with y_center=0 and gain=1.0, representing
 * the local-space [-1, 1] coordinate system. The model matrix is
 * responsible for positioning in world space.
 */
[[nodiscard]] CorePlotting::SeriesLayout makeLocalSpaceLayout() {
    return CorePlotting::SeriesLayout{
        "",
        CorePlotting::LayoutTransform{0.0f, 1.0f},  // center=0, half_height=1 (maps [-1,1])
        0
    };
}

} // anonymous namespace

CorePlotting::RenderablePolyLineBatch buildAnalogSeriesBatch(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        CorePlotting::AnalogSeriesMatrixParams const & model_params,
        [[maybe_unused]] CorePlotting::ViewProjectionParams const & view_params) {

    CorePlotting::RenderablePolyLineBatch batch;
    batch.global_color = params.color;
    batch.thickness = params.thickness;

    // Build Model matrix
    batch.model_matrix = CorePlotting::getAnalogModelMatrix(model_params);

    if (!master_time_frame) {
        return batch;
    }

    // Use local-space layout (Y=raw value, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper with indices for gap detection
    auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
            series, local_layout, *master_time_frame, 1.0f, params.start_time, params.end_time);

    if (params.detect_gaps) {
        // Use GapDetector for segmented rendering
        CorePlotting::GapDetector::Config gap_config;
        gap_config.time_threshold = static_cast<int64_t>(params.gap_threshold);
        gap_config.min_segment_length = 2;
        
        // Materialize range and segment by gaps
        std::vector<CorePlotting::MappedAnalogVertex> vertices;
        for (auto const & v : mapped_range) {
            vertices.push_back(v);
        }
        
        batch = CorePlotting::GapDetector::segmentByGaps(vertices, gap_config);
        
        // Restore batch properties that segmentByGaps doesn't set
        batch.global_color = params.color;
        batch.thickness = params.thickness;
        batch.model_matrix = CorePlotting::getAnalogModelMatrix(model_params);
    } else {
        // No gap detection - single continuous line
        std::vector<float> all_vertices;
        
        for (auto const & vertex : mapped_range) {
            all_vertices.push_back(vertex.x);
            all_vertices.push_back(vertex.y);
        }
        
        if (all_vertices.size() >= 4) { // At least 2 vertices
            int const vertex_count = static_cast<int>(all_vertices.size()) / 2;
            batch.line_start_indices.push_back(0);
            batch.line_vertex_counts.push_back(vertex_count);
            batch.vertices = std::move(all_vertices);
        }
    }

    return batch;
}

CorePlotting::RenderableGlyphBatch buildEventSeriesBatch(
        DigitalEventSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        EventBatchParams const & params,
        CorePlotting::EventSeriesMatrixParams const & model_params,
        [[maybe_unused]] CorePlotting::ViewProjectionParams const & view_params) {

    CorePlotting::RenderableGlyphBatch batch;
    batch.glyph_type = params.glyph_type;
    batch.size = params.glyph_size;

    // Build Model matrix
    batch.model_matrix = CorePlotting::getEventModelMatrix(model_params);

    if (!master_time_frame) {
        return batch;
    }

    // Use local-space layout (Y=0, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper - returns materialized vector for cross-TimeFrame support
    auto mapped_events = CorePlotting::TimeSeriesMapper::mapEventsInRange(
            series, local_layout, *master_time_frame, params.start_time, params.end_time);

    // Reserve space
    batch.positions.reserve(mapped_events.size());
    batch.entity_ids.reserve(mapped_events.size());

    // Extract positions and entity IDs from mapped elements
    for (auto const & event : mapped_events) {
        batch.positions.emplace_back(event.x, event.y);
        batch.entity_ids.push_back(event.entity_id);
    }

    return batch;
}

CorePlotting::RenderableRectangleBatch buildIntervalSeriesBatch(
        DigitalIntervalSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        IntervalBatchParams const & params,
        CorePlotting::IntervalSeriesMatrixParams const & model_params,
        [[maybe_unused]] CorePlotting::ViewProjectionParams const & view_params) {

    CorePlotting::RenderableRectangleBatch batch;

    // Build Model matrix
    batch.model_matrix = CorePlotting::getIntervalModelMatrix(model_params);

    if (!master_time_frame) {
        return batch;
    }

    // Use local-space layout (Y fills [-1,1], model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper - returns materialized vector for cross-TimeFrame support
    auto mapped_intervals = CorePlotting::TimeSeriesMapper::mapIntervalsInRange(
            series, local_layout, *master_time_frame, params.start_time, params.end_time);

    // Reserve space
    batch.bounds.reserve(mapped_intervals.size());
    batch.colors.reserve(mapped_intervals.size());
    batch.entity_ids.reserve(mapped_intervals.size());

    // Extract bounds, colors, and entity IDs from mapped elements
    for (auto const & interval : mapped_intervals) {
        batch.bounds.emplace_back(interval.x, interval.y, interval.width, interval.height);
        batch.colors.push_back(params.color);
        batch.entity_ids.push_back(interval.entity_id);
    }

    return batch;
}

CorePlotting::RenderableRectangleBatch buildIntervalHighlightBatch(
        int64_t start_time,
        int64_t end_time,
        glm::vec4 const & highlight_color,
        glm::mat4 const & model_matrix) {

    CorePlotting::RenderableRectangleBatch batch;
    batch.model_matrix = model_matrix;

    float const x = static_cast<float>(start_time);
    float const width = static_cast<float>(end_time - start_time);
    float const y_min = -1.0f;
    float const height = 2.0f;

    batch.bounds.emplace_back(x, y_min, width, height);
    batch.colors.push_back(highlight_color);
    batch.entity_ids.push_back(EntityId{0});// No specific entity for highlights

    return batch;
}

CorePlotting::RenderablePolyLineBatch buildIntervalHighlightBorderBatch(
        int64_t start_time,
        int64_t end_time,
        glm::vec4 const & highlight_color,
        float border_thickness,
        glm::mat4 const & model_matrix) {
    
    CorePlotting::RenderablePolyLineBatch batch;
    batch.model_matrix = model_matrix;
    batch.global_color = highlight_color;
    batch.thickness = border_thickness;
    
    float const x_start = static_cast<float>(start_time);
    float const x_end = static_cast<float>(end_time);
    float const y_min = -1.0f;
    float const y_max = 1.0f;
    
    // Build 4 line segments for the rectangle border
    // Bottom edge
    batch.vertices.push_back(x_start); batch.vertices.push_back(y_min);
    batch.vertices.push_back(x_end);   batch.vertices.push_back(y_min);
    batch.line_start_indices.push_back(0);
    batch.line_vertex_counts.push_back(2);
    
    // Top edge
    batch.vertices.push_back(x_start); batch.vertices.push_back(y_max);
    batch.vertices.push_back(x_end);   batch.vertices.push_back(y_max);
    batch.line_start_indices.push_back(2);
    batch.line_vertex_counts.push_back(2);
    
    // Left edge
    batch.vertices.push_back(x_start); batch.vertices.push_back(y_min);
    batch.vertices.push_back(x_start); batch.vertices.push_back(y_max);
    batch.line_start_indices.push_back(4);
    batch.line_vertex_counts.push_back(2);
    
    // Right edge
    batch.vertices.push_back(x_end);   batch.vertices.push_back(y_min);
    batch.vertices.push_back(x_end);   batch.vertices.push_back(y_max);
    batch.line_start_indices.push_back(6);
    batch.line_vertex_counts.push_back(2);
    
    return batch;
}

CorePlotting::RenderableGlyphBatch buildAnalogSeriesMarkerBatch(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        CorePlotting::AnalogSeriesMatrixParams const & model_params,
        [[maybe_unused]] CorePlotting::ViewProjectionParams const & view_params) {
    
    CorePlotting::RenderableGlyphBatch batch;
    batch.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    batch.size = params.thickness * 2.0f; // Use thickness to determine marker size
    
    // Build Model matrix
    batch.model_matrix = CorePlotting::getAnalogModelMatrix(model_params);
    
    if (!master_time_frame) {
        return batch;
    }
    
    // Use local-space layout (Y=raw value, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper, materialize here
    auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeries(
            series, local_layout, *master_time_frame, 1.0f, params.start_time, params.end_time);
    
    // Materialize and convert to positions
    for (auto const & vertex : mapped_range) {
        batch.positions.emplace_back(vertex.x, vertex.y);
    }
    
    return batch;
}

// ============================================================================
// Simplified API using pre-composed Model matrices (Phase 4.13)
// ============================================================================

CorePlotting::RenderablePolyLineBatch buildAnalogSeriesBatchSimplified(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        glm::mat4 const & model_matrix) {
    
    CorePlotting::RenderablePolyLineBatch batch;
    batch.global_color = params.color;
    batch.thickness = params.thickness;
    batch.model_matrix = model_matrix;

    if (!master_time_frame) {
        return batch;
    }

    // Use local-space layout (Y=raw value, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper with indices for gap detection
    auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
            series, local_layout, *master_time_frame, 1.0f, params.start_time, params.end_time);

    if (params.detect_gaps) {
        // Use GapDetector for segmented rendering
        CorePlotting::GapDetector::Config gap_config;
        gap_config.time_threshold = static_cast<int64_t>(params.gap_threshold);
        gap_config.min_segment_length = 2;
        
        // Materialize range and segment by gaps
        std::vector<CorePlotting::MappedAnalogVertex> vertices;
        for (auto const & v : mapped_range) {
            vertices.push_back(v);
        }
        
        batch = CorePlotting::GapDetector::segmentByGaps(vertices, gap_config);
        
        // Restore batch properties that segmentByGaps doesn't set
        batch.global_color = params.color;
        batch.thickness = params.thickness;
        batch.model_matrix = model_matrix;
    } else {
        // No gap detection - single continuous line
        std::vector<float> all_vertices;
        
        for (auto const & vertex : mapped_range) {
            all_vertices.push_back(vertex.x);
            all_vertices.push_back(vertex.y);
        }
        
        if (all_vertices.size() >= 4) { // At least 2 vertices
            int const vertex_count = static_cast<int>(all_vertices.size()) / 2;
            batch.line_start_indices.push_back(0);
            batch.line_vertex_counts.push_back(vertex_count);
            batch.vertices = std::move(all_vertices);
        }
    }

    return batch;
}

CorePlotting::RenderableGlyphBatch buildAnalogSeriesMarkerBatchSimplified(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        glm::mat4 const & model_matrix) {
    
    CorePlotting::RenderableGlyphBatch batch;
    batch.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    batch.size = params.thickness * 2.0f;
    batch.model_matrix = model_matrix;
    
    if (!master_time_frame) {
        return batch;
    }
    
    // Use local-space layout (Y=raw value, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper, materialize here
    auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeries(
            series, local_layout, *master_time_frame, 1.0f, params.start_time, params.end_time);
    
    // Materialize and convert to positions
    for (auto const & vertex : mapped_range) {
        batch.positions.emplace_back(vertex.x, vertex.y);
    }
    
    return batch;
}

CorePlotting::RenderableGlyphBatch buildEventSeriesBatchSimplified(
        DigitalEventSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        EventBatchParams const & params,
        glm::mat4 const & model_matrix) {
    
    CorePlotting::RenderableGlyphBatch batch;
    batch.glyph_type = params.glyph_type;
    batch.size = params.glyph_size;
    batch.model_matrix = model_matrix;

    if (!master_time_frame) {
        return batch;
    }

    // Use local-space layout (Y=0, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper - returns materialized vector for cross-TimeFrame support
    auto mapped_events = CorePlotting::TimeSeriesMapper::mapEventsInRange(
            series, local_layout, *master_time_frame, params.start_time, params.end_time);

    // Reserve space
    batch.positions.reserve(mapped_events.size());
    batch.entity_ids.reserve(mapped_events.size());

    // Extract positions and entity IDs from mapped elements
    for (auto const & event : mapped_events) {
        batch.positions.emplace_back(event.x, event.y);
        batch.entity_ids.push_back(event.entity_id);
    }

    return batch;
}

CorePlotting::RenderableRectangleBatch buildIntervalSeriesBatchSimplified(
        DigitalIntervalSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        IntervalBatchParams const & params,
        glm::mat4 const & model_matrix) {
    
    CorePlotting::RenderableRectangleBatch batch;
    batch.model_matrix = model_matrix;

    if (!master_time_frame) {
        return batch;
    }

    // Use local-space layout (Y fills [-1,1], model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper - returns materialized vector for cross-TimeFrame support
    auto mapped_intervals = CorePlotting::TimeSeriesMapper::mapIntervalsInRange(
            series, local_layout, *master_time_frame, params.start_time, params.end_time);

    // Reserve space
    batch.bounds.reserve(mapped_intervals.size());
    batch.colors.reserve(mapped_intervals.size());
    batch.entity_ids.reserve(mapped_intervals.size());

    // Extract bounds, colors, and entity IDs from mapped elements
    for (auto const & interval : mapped_intervals) {
        batch.bounds.emplace_back(interval.x, interval.y, interval.width, interval.height);
        batch.colors.push_back(params.color);
        batch.entity_ids.push_back(interval.entity_id);
    }

    return batch;
}

// ============================================================================
// Cached Vertex API for efficient scrolling (Phase: Streaming Optimization)
// ============================================================================

std::vector<DataViewer::CachedAnalogVertex> generateVerticesForRange(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time) {
    
    std::vector<DataViewer::CachedAnalogVertex> result;
    
    if (!master_time_frame) {
        return result;
    }
    
    // Use local-space layout (Y=raw value, model matrix handles positioning)
    auto const local_layout = makeLocalSpaceLayout();
    
    // Use range-based mapper with indices
    auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
            series, local_layout, *master_time_frame, 1.0f, start_time, end_time);
    
    // Materialize into CachedAnalogVertex format
    for (auto const & vertex : mapped_range) {
        result.push_back(DataViewer::CachedAnalogVertex{
            vertex.x,
            vertex.y,
            TimeFrameIndex{vertex.time_index}
        });
    }
    
    return result;
}

CorePlotting::RenderablePolyLineBatch buildAnalogSeriesBatchCached(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        glm::mat4 const & model_matrix,
        DataViewer::AnalogVertexCache & cache) {
    
    CorePlotting::RenderablePolyLineBatch batch;
    batch.global_color = params.color;
    batch.thickness = params.thickness;
    batch.model_matrix = model_matrix;

    if (!master_time_frame) {
        return batch;
    }

    // Initialize cache if needed (use 3x visible window for smooth scrolling)
    size_t const visible_points = static_cast<size_t>(params.end_time.getValue() - params.start_time.getValue());
    size_t const desired_capacity = visible_points * 3;
    
    if (!cache.isInitialized() || cache.capacity() < desired_capacity) {
        cache.initialize(desired_capacity);
    }
    
    // Check if we need to update the cache
    if (cache.needsUpdate(params.start_time, params.end_time)) {
        auto missing_ranges = cache.getMissingRanges(params.start_time, params.end_time);
        
        if (missing_ranges.size() == 1 && 
            missing_ranges[0].start == params.start_time && 
            missing_ranges[0].end == params.end_time) {
            // Complete cache miss - regenerate all vertices
            auto vertices = generateVerticesForRange(series, master_time_frame, 
                                                     params.start_time, params.end_time);
            cache.setVertices(vertices, params.start_time, params.end_time);
        } else {
            // Incremental update - only generate missing ranges
            for (auto const & range : missing_ranges) {
                auto vertices = generateVerticesForRange(series, master_time_frame,
                                                         range.start, range.end);
                if (range.prepend) {
                    cache.prependVertices(vertices);
                } else {
                    cache.appendVertices(vertices);
                }
            }
        }
    }
    
    // Extract vertices for the requested range
    auto flat_vertices = cache.getVerticesForRange(params.start_time, params.end_time);
    
    // Gap detection is currently not supported with caching
    // (would require tracking original indices in the cache)
    if (!flat_vertices.empty() && flat_vertices.size() >= 4) {
        int const vertex_count = static_cast<int>(flat_vertices.size()) / 2;
        batch.line_start_indices.push_back(0);
        batch.line_vertex_counts.push_back(vertex_count);
        batch.vertices = std::move(flat_vertices);
    }

    return batch;
}

}// namespace DataViewerHelpers
