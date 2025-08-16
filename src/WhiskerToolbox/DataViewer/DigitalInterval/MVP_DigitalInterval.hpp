#ifndef MVP_DIGITALINTERVAL_HPP
#define MVP_DIGITALINTERVAL_HPP

#include "TimeFrame/interval_data.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

struct NewDigitalIntervalSeriesDisplayOptions;
struct PlottingManager;

/**
 * @brief Create new Model matrix for digital interval series positioning and scaling
 *
 * New implementation for digital interval series that handles positioning and scaling.
 * Intervals are rendered as rectangles that extend vertically across the full canvas
 * or allocated space with appropriate alpha blending.
 *
 * @param display_options New display configuration for the interval series
 * @param plotting_manager Reference to plotting manager for coordinate allocation
 * @return Model transformation matrix
 */
glm::mat4 new_getIntervalModelMat(NewDigitalIntervalSeriesDisplayOptions const & display_options,
                                  PlottingManager const & plotting_manager);

/**
 * @brief Create new View matrix for digital interval series global transformations
 *
 * New implementation for view-level transformations applied to all digital interval series.
 * Handles global panning and view-specific transformations.
 *
 * @param plotting_manager Reference to plotting manager for global transformations
 * @return View transformation matrix
 */
glm::mat4 new_getIntervalViewMat(PlottingManager const & plotting_manager);

/**
 * @brief Create new Projection matrix for digital interval series coordinate mapping
 *
 * New implementation that maps data coordinates to normalized device coordinates.
 * Takes data index range instead of time range for testing purposes.
 * Intervals are projected to extend vertically across the visible range.
 *
 * @param start_data_index Start index of data range to display (e.g., 1)
 * @param end_data_index End index of data range to display (e.g., 10000)
 * @param y_min Minimum Y coordinate in data space
 * @param y_max Maximum Y coordinate in data space
 * @param plotting_manager Reference to plotting manager for viewport configuration
 * @return Projection transformation matrix
 */
glm::mat4 new_getIntervalProjectionMat(int start_data_index,
                                       int end_data_index,
                                       float y_min,
                                       float y_max,
                                       PlottingManager const & plotting_manager);

/**
 * @brief Generate test digital interval data
 * 
 * Creates a vector of intervals with start and end times between 0 and max_time.
 * Intervals are ordered by start time and guarantee end > start for each interval.
 * 
 * @param num_intervals Number of intervals to generate
 * @param max_time Maximum time value for interval generation
 * @param min_duration Minimum duration for each interval
 * @param max_duration Maximum duration for each interval
 * @param seed Random seed for reproducibility
 * @return Vector of generated interval data
 */
std::vector<Interval> generateTestIntervalData(size_t num_intervals,
                                               float max_time = 10000.0f,
                                               float min_duration = 50.0f,
                                               float max_duration = 500.0f,
                                               unsigned int seed = 42);

/**
 * @brief Set intrinsic properties for digital interval display options
 * 
 * Analyzes interval data and configures display options with appropriate
 * scaling and positioning for optimal visualization.
 * 
 * @param intervals Vector of interval data to analyze
 * @param display_options Display options to configure
 */
void setIntervalIntrinsicProperties(std::vector<Interval> const & intervals,
                                    NewDigitalIntervalSeriesDisplayOptions & display_options);

#endif// MVP_DIGITALINTERVAL_HPP
