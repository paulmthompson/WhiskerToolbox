#include "MVP_DigitalEvent.hpp"

#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"

#include <iostream>

// Model Matrix: Handles series-specific positioning and scaling
glm::mat4 getEventModelMat(DigitalEventSeriesDisplayOptions const * display_options,
                           int visible_series_index,
                           int center_coord) {

    auto Model = glm::mat4(1.0f);

    if (display_options->display_mode == EventDisplayMode::Stacked) {
        // Check if this series has been positioned by VerticalSpaceManager
        if (display_options->vertical_spacing == 0.0f) {
            // VerticalSpaceManager positioning: use calculated position and scale
            float const series_center_y = display_options->y_offset;
            float const series_height = display_options->event_height;

            // Apply scaling: transform from normalized [-1,+1] to allocated height
            Model = glm::scale(Model, glm::vec3(1.0f, series_height * 0.5f, 1.0f));

            // Apply translation: move to calculated center position
            Model = glm::translate(Model, glm::vec3(0.0f, series_center_y / (series_height * 0.5f), 0.0f));

        } else {
            // Legacy index-based positioning for backward compatibility
            float const y_offset = static_cast<float>(visible_series_index) * display_options->vertical_spacing;
            float const series_center = y_offset + center_coord;
            float const series_height = display_options->event_height;

            // Apply scaling and translation
            Model = glm::scale(Model, glm::vec3(1.0f, series_height * 0.5f, 1.0f));
            Model = glm::translate(Model, glm::vec3(0.0f, series_center / (series_height * 0.5f), 0.0f));
        }
    }

    return Model;
}

// View Matrix: Handles global panning (applied to all series equally)
// Note: Pan offset is now handled in Projection matrix for dynamic viewport
glm::mat4 getEventViewMat() {

    auto View = glm::mat4(1.0f);
    return View;
}

glm::mat4 getEventProjectionMat(float yMin,
                                float yMax,
                                float verticalPanOffset,
                                int64_t start_time,
                                int64_t end_time)
{
    // Projection Matrix: Maps world coordinates to screen coordinates
    // - X axis: maps time range [start_time, end_time] to screen width
    // - Y axis: maps world Y coordinates [min_y, max_y] to screen height
    //
    // For dynamic viewport: adjust Y bounds based on content and pan offset
    float dynamic_min_y = yMin + verticalPanOffset;
    float dynamic_max_y = yMax + verticalPanOffset;

    // Projection Matrix. Orthographic. Horizontal Zoom. Vertical Zoom.
    auto Projection = glm::ortho(static_cast<float>(start_time),
                                 static_cast<float>(end_time),
                                 dynamic_min_y,
                                 dynamic_max_y);

    return Projection;
}
