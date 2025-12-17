#include "SceneBuildingHelpers.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
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
 * Returns a SeriesLayout with y_center=0 and height=2, representing
 * the local-space [-1, 1] coordinate system. The model matrix is
 * responsible for positioning in world space.
 */
[[nodiscard]] CorePlotting::SeriesLayout makeLocalSpaceLayout() {
    return CorePlotting::SeriesLayout{
        CorePlotting::SeriesLayoutResult{0.0f, 2.0f},
        "",
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
    
    // Use range-based mapper with indices for gap detection, materialize here
    auto mapped_range = CorePlotting::TimeSeriesMapper::mapAnalogSeriesWithIndices(
            series, local_layout, *master_time_frame, 1.0f, params.start_time, params.end_time);

    std::vector<float> segment_vertices;
    int64_t prev_index = -1;
    bool first_point = true;
    int current_line_start = 0;

    for (auto const & vertex : mapped_range) {
        // Check for gap if this isn't the first point and gap detection is enabled
        if (!first_point && params.detect_gaps) {
            int64_t const gap_size = vertex.time_index - prev_index;

            if (gap_size > static_cast<int64_t>(params.gap_threshold)) {
                // Gap detected - finalize current segment
                if (segment_vertices.size() >= 4) {// At least 2 vertices
                    // Record this segment
                    int const vertex_count = static_cast<int>(segment_vertices.size()) / 2;
                    if (vertex_count >= 2) {
                        batch.line_start_indices.push_back(current_line_start);
                        batch.line_vertex_counts.push_back(vertex_count);

                        // Append segment vertices to batch
                        batch.vertices.insert(batch.vertices.end(),
                                              segment_vertices.begin(),
                                              segment_vertices.end());
                        current_line_start += vertex_count;
                    }
                }
                segment_vertices.clear();
            }
        }

        // Add vertex to current segment
        segment_vertices.push_back(vertex.x);
        segment_vertices.push_back(vertex.y);

        prev_index = vertex.time_index;
        first_point = false;
    }

    // Finalize last segment
    if (segment_vertices.size() >= 4) {
        int const vertex_count = static_cast<int>(segment_vertices.size()) / 2;
        if (vertex_count >= 2) {
            batch.line_start_indices.push_back(current_line_start);
            batch.line_vertex_counts.push_back(vertex_count);
            batch.vertices.insert(batch.vertices.end(),
                                  segment_vertices.begin(),
                                  segment_vertices.end());
        }
    } else if (!params.detect_gaps && !segment_vertices.empty()) {
        // No gap detection - single line with all vertices
        int const vertex_count = static_cast<int>(segment_vertices.size()) / 2;
        if (vertex_count >= 2) {
            batch.line_start_indices.push_back(0);
            batch.line_vertex_counts.push_back(vertex_count);
            batch.vertices = std::move(segment_vertices);
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

}// namespace DataViewerHelpers
