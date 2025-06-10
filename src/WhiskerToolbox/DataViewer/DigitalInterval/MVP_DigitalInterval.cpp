#include "MVP_DigitalInterval.hpp"

#include "DigitalIntervalSeriesDisplayOptions.hpp"
#include "PlottingManager/PlottingManager.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>


glm::mat4 new_getIntervalModelMat(NewDigitalIntervalSeriesDisplayOptions const & display_options,
                                  PlottingManager const & plotting_manager) {
    auto Model = glm::mat4(1.0f);

    // Apply global zoom scaling
    float const global_scale = display_options.global_zoom * display_options.global_vertical_scale;

    // For digital intervals extending full canvas, we want them to span the entire allocated height
    // The allocated_height represents the total height available for this series
    if (display_options.extend_full_canvas) {
        // Scale to use the full allocated height with margin factor
        float const height_scale = display_options.allocated_height * display_options.margin_factor * 0.5f;
        Model = glm::scale(Model, glm::vec3(1.0f, height_scale * global_scale, 1.0f));

        // Translate to allocated center position
        Model = glm::translate(Model, glm::vec3(0.0f, display_options.allocated_y_center, 0.0f));
    } else {
        // Apply standard scaling and positioning
        Model = glm::scale(Model, glm::vec3(1.0f, global_scale, 1.0f));
        Model = glm::translate(Model, glm::vec3(0.0f, display_options.allocated_y_center, 0.0f));
    }

    return Model;
}

glm::mat4 new_getIntervalViewMat(PlottingManager const & plotting_manager) {
    auto View = glm::mat4(1.0f);

    // Digital intervals should NOT move with vertical panning
    // They always extend from the top to bottom of the current view
    // Unlike analog series, panning should not affect their position
    // (they remain "pinned" to the current viewport)

    return View;
}

glm::mat4 new_getIntervalProjectionMat(int start_data_index,
                                       int end_data_index,
                                       float y_min,
                                       float y_max,
                                       PlottingManager const & plotting_manager) {
    // Convert data indices to normalized coordinate range
    // Map [start_data_index, end_data_index] to [-1, 1] in X
    // Map [y_min, y_max] to viewport range in Y

    auto const left = static_cast<float>(start_data_index);
    auto const right = static_cast<float>(end_data_index);

    // Use viewport Y range from plotting manager with any additional pan offset
    float bottom = plotting_manager.viewport_y_min;
    float top = plotting_manager.viewport_y_max;

    // Create orthographic projection matrix
    auto Projection = glm::ortho(left, right, bottom, top);

    return Projection;
}

std::vector<IntervalData> generateTestIntervalData(size_t num_intervals,
                                                   float max_time,
                                                   float min_duration,
                                                   float max_duration,
                                                   unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> time_dist(0.0f, max_time * 0.8f);// Leave some space for intervals
    std::uniform_real_distribution<float> duration_dist(min_duration, max_duration);

    std::vector<IntervalData> intervals;
    intervals.reserve(num_intervals);

    // Generate random intervals
    for (size_t i = 0; i < num_intervals; ++i) {
        float start_time = time_dist(gen);
        float duration = duration_dist(gen);
        float end_time = start_time + duration;

        // Ensure we don't exceed max_time
        if (end_time > max_time) {
            end_time = max_time;
            start_time = std::max(0.0f, end_time - duration);
        }

        intervals.emplace_back(start_time, end_time);
    }

    // Sort intervals by start time to ensure ordering requirement
    std::sort(intervals.begin(), intervals.end(),
              [](IntervalData const & a, IntervalData const & b) {
                  return a.start_time < b.start_time;
              });

    // Validate all intervals (end > start)
    for (auto & interval: intervals) {
        if (!interval.isValid()) {
            // This shouldn't happen with our generation method, but just in case
            interval.end_time = interval.start_time + min_duration;
        }
    }

    return intervals;
}

void setIntervalIntrinsicProperties(std::vector<IntervalData> const & intervals,
                                    NewDigitalIntervalSeriesDisplayOptions & display_options) {
    if (intervals.empty()) {
        return;
    }

    // For digital intervals, intrinsic properties are minimal since they're meant to be
    // visual indicators rather than quantitative data. The main consideration is
    // ensuring good visibility across the canvas.

    // Calculate some basic statistics for potential future use
    float total_duration = 0.0f;
    float min_duration = std::numeric_limits<float>::max();
    float max_duration = 0.0f;

    for (auto const & interval: intervals) {
        float const duration = interval.getDuration();
        total_duration += duration;
        min_duration = std::min(min_duration, duration);
        max_duration = std::max(max_duration, duration);
    }

    float const avg_duration = total_duration / static_cast<float>(intervals.size());

    // For now, we keep the default display options
    // Future enhancements could adjust alpha based on interval density,
    // or adjust positioning based on overlap analysis

    // Ensure alpha is appropriate for overlapping intervals
    if (intervals.size() > 50) {
        // With many intervals, reduce alpha to prevent over-saturation
        display_options.alpha = std::max(0.1f, 0.3f * std::sqrt(50.0f / static_cast<float>(intervals.size())));
    }

    // Set full canvas extension for maximum visibility
    display_options.extend_full_canvas = true;
}
