#include "GapDetector.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"

#include <cmath>

namespace CorePlotting {

GapDetector::GapDetector(Config const & config)
    : _config(config) {}

void GapDetector::setTimeThreshold(int64_t threshold) {
    _config.time_threshold = threshold;
}

void GapDetector::setValueThreshold(float threshold) {
    _config.value_threshold = threshold;
}

void GapDetector::setMinSegmentLength(int length) {
    _config.min_segment_length = length;
}

bool GapDetector::detectGap(float time1, float time2, float value1, float value2) const {
    // Check time threshold
    if (_config.time_threshold > 0) {
        float const time_diff = std::abs(time2 - time1);
        if (time_diff > static_cast<float>(_config.time_threshold)) {
            return true;
        }
    }

    // Check value threshold
    if (_config.value_threshold > 0.0f) {
        float const value_diff = std::abs(value2 - value1);
        if (value_diff > _config.value_threshold) {
            return true;
        }
    }

    return false;
}

RenderablePolyLineBatch GapDetector::transform(
        AnalogTimeSeries const & series,
        TimeFrame const & time_frame,
        EntityId entity_id) const {

    // Get data from series
    auto const & time_indices = series.getTimeSeries();
    auto const data_span = series.getAnalogTimeSeries();
    std::vector<float> data(data_span.begin(), data_span.end());

    if (time_indices.size() != data.size() || time_indices.empty()) {
        return RenderablePolyLineBatch{};// Empty batch
    }

    // Convert time indices to actual time values
    std::vector<float> time_values;
    time_values.reserve(time_indices.size());

    for (auto const & idx: time_indices) {
        int const time = time_frame.getTimeAtIndex(idx);
        time_values.push_back(static_cast<float>(time));
    }

    // Transform using array version
    RenderablePolyLineBatch batch = transform(time_values, data, entity_id);

    return batch;
}

RenderablePolyLineBatch GapDetector::transform(
        std::vector<float> const & time_values,
        std::vector<float> const & data_values,
        EntityId entity_id) const {

    RenderablePolyLineBatch batch;
    batch.global_entity_id = entity_id;

    if (time_values.size() != data_values.size() || time_values.empty()) {
        return batch;// Empty batch
    }

    // Reserve space for worst case (every sample is its own segment)
    batch.vertices.reserve(time_values.size() * 2);
    batch.line_start_indices.reserve(time_values.size());
    batch.line_vertex_counts.reserve(time_values.size());

    // Track current segment
    std::vector<float> current_segment_vertices;
    current_segment_vertices.reserve(time_values.size() * 2);

    // Build segments
    for (size_t i = 0; i < time_values.size(); ++i) {
        float const time = time_values[i];
        float const value = data_values[i];

        // Check for gap (if not first sample and not at segment start)
        bool is_gap = false;
        if (i > 0 && !current_segment_vertices.empty()) {
            float const prev_time = time_values[i - 1];
            float const prev_value = data_values[i - 1];
            is_gap = detectGap(prev_time, time, prev_value, value);
        }

        if (is_gap) {
            // Finalize current segment if it meets minimum length
            if (current_segment_vertices.size() >=
                static_cast<size_t>(_config.min_segment_length * 2)) {

                // start_index is the vertex index (not float index)
                int32_t const start_index = static_cast<int32_t>(batch.vertices.size() / 2);
                int32_t const vertex_count = static_cast<int32_t>(current_segment_vertices.size() / 2);

                batch.line_start_indices.push_back(start_index);
                batch.line_vertex_counts.push_back(vertex_count);

                batch.vertices.insert(
                        batch.vertices.end(),
                        current_segment_vertices.begin(),
                        current_segment_vertices.end());
            }

            // Start new segment
            current_segment_vertices.clear();
        }

        // Add vertex to current segment
        current_segment_vertices.push_back(time);
        current_segment_vertices.push_back(value);
    }

    // Finalize last segment
    if (current_segment_vertices.size() >=
        static_cast<size_t>(_config.min_segment_length * 2)) {

        // start_index is the vertex index (not float index)
        int32_t const start_index = static_cast<int32_t>(batch.vertices.size() / 2);
        int32_t const vertex_count = static_cast<int32_t>(current_segment_vertices.size() / 2);

        batch.line_start_indices.push_back(start_index);
        batch.line_vertex_counts.push_back(vertex_count);

        batch.vertices.insert(
                batch.vertices.end(),
                current_segment_vertices.begin(),
                current_segment_vertices.end());
    }

    return batch;
}

}// namespace CorePlotting
