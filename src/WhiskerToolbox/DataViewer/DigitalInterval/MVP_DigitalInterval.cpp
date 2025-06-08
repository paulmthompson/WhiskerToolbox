#include "MVP_DigitalInterval.hpp"

#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"

#include <iostream>

// Model Matrix: Handles series-specific positioning and scaling for digital intervals
glm::mat4 getIntervalModelMat(DigitalIntervalSeriesDisplayOptions const * display_options,
                              std::string const & key) {
    auto Model = glm::mat4(1.0f);

    // Check if this series has been positioned by VerticalSpaceManager
    // (VerticalSpaceManager sets a specific y_offset value)
    if (display_options->y_offset != 0.0f) {
        // VerticalSpaceManager positioning: use calculated position and scale
        float const series_center_y = display_options->y_offset;
        float const series_height = display_options->interval_height;

        // Apply translation: move to calculated center position
        Model = glm::translate(Model, glm::vec3(0, series_center_y, 0));

        // Apply scaling: scale to allocated height
        Model = glm::scale(Model, glm::vec3(1, series_height * 0.5f, 1));

        std::cout << "  Interval '" << key << "' MVP: center_y=" << series_center_y
                  << ", height=" << series_height << std::endl;
    }
    // For intervals without VerticalSpaceManager, use full canvas (no Model matrix transforms)

    return Model;
}

// View Matrix: Handles global panning (applied to all series equally) for digital intervals
// Note: Pan offset is now handled in Projection matrix for dynamic viewport
glm::mat4 getIntervalViewMat() {
    auto View = glm::mat4(1.0f);
    // View = glm::translate(View, glm::vec3(0, _verticalPanOffset, 0)); // Moved to Projection

    return View;
}

// Projection Matrix: Maps world coordinates to screen coordinates for digital intervals
glm::mat4 getIntervalProjectionMat(float start_time,
                                   float end_time,
                                   float yMin,
                                   float yMax,
                                   float verticalPanOffset) {
    // Projection Matrix: Maps world coordinates to screen coordinates
    // - X axis: maps time range [start_time, end_time] to screen width
    // - Y axis: maps world Y coordinates [min_y, max_y] to screen height
    //
    // For dynamic viewport: adjust Y bounds based on content and pan offset
    float dynamic_min_y = yMin + verticalPanOffset;
    float dynamic_max_y = yMax + verticalPanOffset;

    auto Projection = glm::ortho(start_time, end_time, dynamic_min_y, dynamic_max_y);

    return Projection;
}
