#include "SceneBuildingHelpers.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "DigitalTimeSeries/IntervalWithId.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cmath>

namespace DataViewerHelpers {

// Helper function to get time index in series time frame from master time frame
namespace {
TimeFrameIndex convertToSeriesTimeFrame(
        TimeFrameIndex const & master_idx,
        TimeFrame const * master_tf,
        TimeFrame const * series_tf) {
    if (master_tf == series_tf || !master_tf || !series_tf) {
        return master_idx;
    }
    // Convert master time frame index to absolute time, then to series time frame index
    auto const absolute_time = master_tf->getTimeAtIndex(master_idx);
    return series_tf->getIndexAtTime(absolute_time);
}
}// anonymous namespace

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

    // Get time range in series time frame
    auto series_start_idx = convertToSeriesTimeFrame(
            params.start_time,
            master_time_frame.get(),
            series.getTimeFrame().get());
    auto series_end_idx = convertToSeriesTimeFrame(
            params.end_time,
            master_time_frame.get(),
            series.getTimeFrame().get());

    // Get data range
    auto analog_range = series.getTimeValueRangeInTimeFrameIndexRange(
            series_start_idx, series_end_idx);

    std::vector<float> segment_vertices;
    int prev_index = -1;
    bool first_point = true;
    int current_line_start = 0;

    for (auto const & [time_idx, value]: analog_range) {
        auto const x_pos = static_cast<float>(series.getTimeFrame()->getTimeAtIndex(time_idx));
        auto const y_pos = value;

        // Check for gap if this isn't the first point and gap detection is enabled
        if (!first_point && params.detect_gaps) {
            int const current_index = time_idx.getValue();
            int const gap_size = current_index - prev_index;

            if (gap_size > static_cast<int>(params.gap_threshold)) {
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
        segment_vertices.push_back(x_pos);
        segment_vertices.push_back(y_pos);

        prev_index = time_idx.getValue();
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

    // Get visible events with IDs in the time range
    auto visible_events = series.getEventsWithIdsInRange(
            params.start_time,
            params.end_time,
            *master_time_frame);

    // Reserve space
    batch.positions.reserve(visible_events.size());
    batch.entity_ids.reserve(visible_events.size());

    for (auto const & event: visible_events) {
        // Calculate X position in master time frame coordinates
        float x_pos;
        if (series.getTimeFrame().get() == master_time_frame.get()) {
            // Same time frame - event is already in correct coordinates
            x_pos = static_cast<float>(event.event_time.getValue());
        } else {
            // Different time frames - convert event index to time
            x_pos = static_cast<float>(series.getTimeFrame()->getTimeAtIndex(event.event_time));
        }

        // Y position is 0 (center of normalized space [-1, 1])
        // The Model matrix will position this correctly in the layout
        batch.positions.emplace_back(x_pos, 0.0f);

        // Store entity ID for hit testing
        batch.entity_ids.push_back(event.entity_id);
    }

    // Set global color (same for all events in this batch)
    // Per-instance colors could be added if needed for group coloring

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

    // Get visible intervals with IDs (overlapping with time range)
    // Use the iterator interface which provides IntervalWithId
    auto visible_intervals = series.getIntervalsWithIdsInRange(
            params.start_time,
            params.end_time,
            *master_time_frame);

    // Reserve space
    batch.bounds.reserve(visible_intervals.size());
    batch.colors.reserve(visible_intervals.size());
    batch.entity_ids.reserve(visible_intervals.size());

    float const start_time_f = static_cast<float>(params.start_time.getValue());
    float const end_time_f = static_cast<float>(params.end_time.getValue());

    for (auto const & interval_with_id: visible_intervals) {
        // Convert to master time frame coordinates
        auto start = static_cast<float>(
                series.getTimeFrame()->getTimeAtIndex(TimeFrameIndex(interval_with_id.interval.start)));
        auto end = static_cast<float>(
                series.getTimeFrame()->getTimeAtIndex(TimeFrameIndex(interval_with_id.interval.end)));

        // Clip to visible range
        start = std::max(start, start_time_f);
        end = std::min(end, end_time_f);

        float const width = end - start;

        // Normalized Y coordinates [-1, 1] - Model matrix handles positioning
        float const y_min = -1.0f;
        float const height = 2.0f;

        // bounds = {x, y, width, height}
        batch.bounds.emplace_back(start, y_min, width, height);
        batch.colors.push_back(params.color);
        batch.entity_ids.push_back(interval_with_id.entity_id);
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
    
    // Get time range in series time frame
    auto series_start_idx = convertToSeriesTimeFrame(
            params.start_time,
            master_time_frame.get(),
            series.getTimeFrame().get());
    auto series_end_idx = convertToSeriesTimeFrame(
            params.end_time,
            master_time_frame.get(),
            series.getTimeFrame().get());
    
    // Get data range
    auto analog_range = series.getTimeValueRangeInTimeFrameIndexRange(
            series_start_idx, series_end_idx);
    
    // Reserve estimated space
    batch.positions.reserve(1000); // Reasonable initial estimate
    
    for (auto const & [time_idx, value] : analog_range) {
        auto const x_pos = static_cast<float>(series.getTimeFrame()->getTimeAtIndex(time_idx));
        auto const y_pos = value;
        
        batch.positions.emplace_back(x_pos, y_pos);
    }
    
    return batch;
}

}// namespace DataViewerHelpers
