#include "MVP_DigitalEvent.hpp"

#include "DigitalEventSeriesDisplayOptions.hpp"
#include "PlottingManager/PlottingManager.hpp"

#include <algorithm>
#include <random>

glm::mat4 new_getEventModelMat(NewDigitalEventSeriesDisplayOptions const & display_options,
                               PlottingManager const & plotting_manager) {
    glm::mat4 Model(1.0f);

    if (display_options.plotting_mode == EventPlottingMode::FullCanvas) {
        // Full Canvas Mode: Events extend from top to bottom of entire plot (like digital intervals)
        // Events are rendered as lines spanning the full viewport height
        // Scale to full viewport height with margin factor
        float const height_scale = (plotting_manager.viewport_y_max - plotting_manager.viewport_y_min) *
                                   display_options.margin_factor;

        // Center events in the middle of the viewport
        float const center_y = (plotting_manager.viewport_y_max + plotting_manager.viewport_y_min) * 0.5f;

        // Apply scaling for full canvas height
        Model[1][1] = height_scale * 0.5f;// Half scale because we'll map [-1,1] to full height

        // Apply translation to center
        Model[3][1] = center_y;

    } else if (display_options.plotting_mode == EventPlottingMode::Stacked) {
        // Stacked Mode: Events are positioned within allocated space (like analog series)
        // Events extend within their allocated height portion

        // Scale to allocated height with margin factor
        float const height_scale = display_options.allocated_height * display_options.margin_factor;

        // Apply scaling for allocated height
        Model[1][1] = height_scale * 0.5f;// Half scale because we'll map [-1,1] to allocated height

        // Apply translation to allocated center
        Model[3][1] = display_options.allocated_y_center;
    }

    // Apply global scaling factors
    Model[1][1] *= display_options.global_vertical_scale * plotting_manager.global_vertical_scale;

    return Model;
}

glm::mat4 new_getEventViewMat(NewDigitalEventSeriesDisplayOptions const & display_options,
                              PlottingManager const & plotting_manager) {
    auto View = glm::mat4(1.0f);

    // Panning behavior depends on plotting mode
    if (display_options.plotting_mode == EventPlottingMode::FullCanvas) {
        // Full Canvas Mode: Events stay viewport-pinned (like digital intervals)
        // No panning applied - events remain fixed to viewport bounds

    } else if (display_options.plotting_mode == EventPlottingMode::Stacked) {
        // Stacked Mode: Events move with content (like analog series)
        // Apply global vertical panning
        if (plotting_manager.vertical_pan_offset != 0.0f) {
            View = glm::translate(View, glm::vec3(0.0f, plotting_manager.vertical_pan_offset, 0.0f));
        }
    }

    return View;
}

glm::mat4 new_getEventProjectionMat(int start_data_index,
                                    int end_data_index,
                                    float y_min,
                                    float y_max,
                                    PlottingManager const & plotting_manager) {

    static_cast<void>(plotting_manager);

    // Map data indices to normalized device coordinates [-1, 1]
    // X-axis: map [start_data_index, end_data_index] to screen width
    // Y-axis: map [y_min, y_max] to viewport height
    //
    // Note: Events use the same projection logic regardless of plotting mode
    // The mode-dependent behavior is handled in Model and View matrices

    auto const data_start = static_cast<float>(start_data_index);
    auto const data_end = static_cast<float>(end_data_index);

    // Create orthographic projection matrix
    // This maps world coordinates to normalized device coordinates [-1, 1]
    auto Projection = glm::ortho(data_start, data_end, y_min, y_max);

    return Projection;
}

// Helper functions for test data generation and intrinsic properties

std::vector<EventData> generateTestEventData(size_t num_events,
                                             float max_time,
                                             unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> time_dist(0.0f, max_time);

    std::vector<EventData> events;
    events.reserve(num_events);

    for (size_t i = 0; i < num_events; ++i) {
        float event_time = time_dist(gen);
        events.emplace_back(event_time);
    }

    // Sort events by time for optimal visualization
    std::sort(events.begin(), events.end(),
              [](EventData const & a, EventData const & b) {
                  return a.time < b.time;
              });

    return events;
}

void setEventIntrinsicProperties(std::vector<EventData> const & events,
                                 NewDigitalEventSeriesDisplayOptions & display_options) {
    if (events.empty()) {
        return;
    }

    // For events, we don't need complex intrinsic properties like analog series
    // Events are simple vertical lines, so basic configuration is sufficient

    // Adjust alpha based on event count to prevent over-saturation
    if (events.size() > 100) {
        display_options.alpha = std::max(0.3f, display_options.alpha * 0.7f);
    } else if (events.size() > 50) {
        display_options.alpha = std::max(0.5f, display_options.alpha * 0.85f);
    }

    // For dense event series, reduce line thickness to avoid clutter
    if (events.size() > 200) {
        display_options.line_thickness = std::max(1, display_options.line_thickness - 1);
    }
}
