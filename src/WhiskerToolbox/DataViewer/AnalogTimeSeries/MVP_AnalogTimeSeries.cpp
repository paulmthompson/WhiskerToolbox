#include "MVP_AnalogTimeSeries.hpp"

#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"

#include <iostream>

// Model Matrix: Handles series-specific positioning and scaling for analog series
glm::mat4 getAnalogModelMat(AnalogTimeSeriesDisplayOptions const * display_options,
                            std::string const & key,
                            float stdDev,
                            int series_index,
                            float ySpacing,
                            float center_coord,
                            float global_zoom) {
    auto Model = glm::mat4(1.0f);

    // Check if this series has been positioned by VerticalSpaceManager
    // (VerticalSpaceManager sets a specific y_offset value and allocated_height > 0)
    if (display_options->y_offset != 0.0f && display_options->allocated_height > 0.0f) {
        // VerticalSpaceManager positioning: use calculated position and allocated space
        float const series_center_y = display_options->y_offset;

        // Calculate amplitude scaling based on allocated height and data characteristics
        // Use a reasonable portion of the allocated height for amplitude display
        float const usable_height = display_options->allocated_height * 0.8f; // 80% of allocated space

        // Scale amplitude to fit within the usable height
        // Use the original approach but base it on allocated space rather than corrupted scale_factor
        float const base_amplitude_scale = 1.0f / stdDev; // Basic amplitude normalization
        float const height_scale = usable_height / 1.0f;  // Scale to fit in allocated height

        // Apply user controls: global zoom and user scale factor
        float const user_controls = display_options->user_scale_factor * global_zoom;
        float const amplitude_scale = base_amplitude_scale * height_scale * user_controls;

        // Apply translation: move to calculated center position
        Model = glm::translate(Model, glm::vec3(0.0f, series_center_y, 0.0f));

        // Apply scaling: scale amplitude to fit in allocated space
        Model = glm::scale(Model, glm::vec3(1.0f, amplitude_scale, 1.0f));

        std::cout << "  Analog '" << key << "' MVP: center_y=" << series_center_y
                  << ", allocated_height=" << display_options->allocated_height
                  << ", stdDev=" << stdDev << ", user_controls=" << user_controls
                  << ", amplitude_scale=" << amplitude_scale << std::endl;
    } else {
        // Legacy index-based positioning for backward compatibility
        float const y_offset = static_cast<float>(series_index) * ySpacing;

        // Apply translation: position based on index and spacing
        Model = glm::translate(Model, glm::vec3(0.0f, y_offset, 0.0f));

        // Center all series around zero
        Model = glm::translate(Model, glm::vec3(0.0f, center_coord, 0.0f));

        // Apply scaling: scale based on actual standard deviation and user settings
        // Fixed: Apply both user scale factor AND global zoom consistently
        float const legacy_amplitude_scale = (1.0f / stdDev) * display_options->user_scale_factor * global_zoom;
        Model = glm::scale(Model, glm::vec3(1.0f, legacy_amplitude_scale, 1.0f));
    }

    return Model;
}

// View Matrix: Handles global panning (applied to all series equally) for analog series
// Note: Pan offset is now handled in Projection matrix for dynamic viewport
glm::mat4 getAnalogViewMat() {
    auto View = glm::mat4(1.0f);
    // View = glm::translate(View, glm::vec3(0, _verticalPanOffset, 0)); // Moved to Projection

    return View;
}

// Projection Matrix: Maps world coordinates to screen coordinates for analog series
glm::mat4 getAnalogProjectionMat(float start_time,
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
