#ifndef MVP_ANALOGTIMESERIES_HPP
#define MVP_ANALOGTIMESERIES_HPP

#include "AnalogTimeSeriesDisplayOptions.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class AnalogTimeSeriesInMemory;
using AnalogTimeSeries = AnalogTimeSeriesInMemory;
struct PlottingManager;

/**
 * @brief Create new Model matrix for analog series positioning and scaling
 *
 * New implementation that handles the three-tier scaling system:
 * intrinsic (data-based), user-specified, and global scaling.
 * Data is centered around its mean value for proper visual centering.
 *
 * @param display_options New display configuration for the analog series
 * @param std_dev Standard deviation of the data for intrinsic scaling
 * @param data_mean Mean value of the data for intrinsic centering
 * @param plotting_manager Reference to plotting manager for coordinate allocation
 * @return Model transformation matrix
 */
glm::mat4 new_getAnalogModelMat(NewAnalogTimeSeriesDisplayOptions const & display_options,
                                float std_dev,
                                float data_mean,
                                PlottingManager const & plotting_manager);

/**
 * @brief Create new View matrix for analog series global transformations
 *
 * New implementation for view-level transformations applied to all analog series.
 * Handles global panning and view-specific transformations.
 *
 * @param plotting_manager Reference to plotting manager for global transformations
 * @return View transformation matrix
 */
glm::mat4 new_getAnalogViewMat(PlottingManager const & plotting_manager);

/**
 * @brief Create new Projection matrix for analog series coordinate mapping
 *
 * New implementation that maps data coordinates to normalized device coordinates.
 * Takes data index range instead of time range for testing purposes.
 *
 * @param start_data_index Start index of data range to display (e.g., 1)
 * @param end_data_index End index of data range to display (e.g., 10000)
 * @param y_min Minimum Y coordinate in data space
 * @param y_max Maximum Y coordinate in data space
 * @param plotting_manager Reference to plotting manager for viewport configuration
 * @return Projection transformation matrix
 */
glm::mat4 new_getAnalogProjectionMat(TimeFrameIndex start_data_index,
                                     TimeFrameIndex end_data_index,
                                     float y_min,
                                     float y_max,
                                     PlottingManager const & plotting_manager);

/**
 * @brief Set intrinsic properties for analog display options
 * 
 * Calculates and caches the mean and standard deviation for a dataset,
 * storing them in the display options for use during MVP matrix generation.
 * 
 * @param data Vector of data values
 * @param display_options Display options to update with intrinsic properties
 */
void setAnalogIntrinsicProperties(AnalogTimeSeries const * analog,
                                  NewAnalogTimeSeriesDisplayOptions & display_options);

/**
 * @brief Get cached standard deviation for an analog series (new display options)
 *
 * Overload for the new display options structure.
 *
 * @param series The analog time series to calculate standard deviation for
 * @param display_options The new display options containing the cache
 * @return Cached standard deviation value
 */
float getCachedStdDev(AnalogTimeSeries const & series, NewAnalogTimeSeriesDisplayOptions & display_options);

/**
 * @brief Invalidate cached display calculations (new display options)
 *
 * Overload for the new display options structure.
 *
 * @param display_options The new display options to invalidate
 */
void invalidateDisplayCache(NewAnalogTimeSeriesDisplayOptions & display_options);

#endif// MVP_ANALOGTIMESERIES_HPP
