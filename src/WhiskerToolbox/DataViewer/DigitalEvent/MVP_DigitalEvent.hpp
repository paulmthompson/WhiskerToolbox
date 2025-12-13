#ifndef MVP_DIGITALEVENT_HPP
#define MVP_DIGITALEVENT_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

struct NewDigitalEventSeriesDisplayOptions;
struct LayoutCalculator;
using PlottingManager = LayoutCalculator;  // Legacy alias

/**
 * @brief Event data representing a single time point
 */
struct EventData {
    float time{0.0f};///< Time of the event occurrence

    /**
     * @brief Constructor for EventData
     * @param event_time Time of the event
     */
    explicit EventData(float event_time)
        : time(event_time) {}

    /**
     * @brief Default constructor
     */
    EventData() = default;

    /**
     * @brief Check if event time is valid (non-negative)
     * @return true if event time is valid
     */
    [[nodiscard]] bool isValid() const {
        return time >= 0.0f;
    }
};



/**
 * @brief Create new Model matrix for digital event series positioning and scaling
 *
 * New implementation for digital event series that handles both plotting modes:
 * - FullCanvas: Events extend from top to bottom of entire plot (like intervals)
 * - Stacked: Events are positioned within allocated space (like analog series)
 *
 * @param display_options New display configuration for the event series
 * @param plotting_manager Reference to plotting manager for coordinate allocation
 * @return Model transformation matrix
 */
glm::mat4 new_getEventModelMat(NewDigitalEventSeriesDisplayOptions const & display_options,
                               PlottingManager const & plotting_manager);

/**
 * @brief Create new View matrix for digital event series global transformations
 *
 * New implementation for view-level transformations applied to all digital event series.
 * Behavior depends on plotting mode:
 * - FullCanvas: No panning (events stay viewport-pinned like intervals)
 * - Stacked: Applies panning (events move with content like analog series)
 *
 * @param display_options Display configuration for mode-dependent behavior
 * @param plotting_manager Reference to plotting manager for global transformations
 * @return View transformation matrix
 */
glm::mat4 new_getEventViewMat(NewDigitalEventSeriesDisplayOptions const & display_options,
                              PlottingManager const & plotting_manager);

/**
 * @brief Create new Projection matrix for digital event series coordinate mapping
 *
 * New implementation that maps data coordinates to normalized device coordinates.
 * Takes data index range instead of time range for testing purposes.
 * Behavior is consistent across both plotting modes.
 *
 * @param start_data_index Start index of data range to display (e.g., 1)
 * @param end_data_index End index of data range to display (e.g., 10000)
 * @param y_min Minimum Y coordinate in data space
 * @param y_max Maximum Y coordinate in data space
 * @param plotting_manager Reference to plotting manager for viewport configuration
 * @return Projection transformation matrix
 */
glm::mat4 new_getEventProjectionMat(int start_data_index,
                                    int end_data_index,
                                    float y_min,
                                    float y_max,
                                    PlottingManager const & plotting_manager);

#endif// MVP_DIGITALEVENT_HPP
