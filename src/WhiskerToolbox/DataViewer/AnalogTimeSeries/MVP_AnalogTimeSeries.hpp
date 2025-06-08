#ifndef MVP_ANALOGTIMESERIES_HPP
#define MVP_ANALOGTIMESERIES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

struct AnalogTimeSeriesDisplayOptions;

/**
 * @brief Create Model matrix for analog series positioning and scaling
 *
 * Handles series-specific transformations for analog series including
 * VerticalSpaceManager positioning with allocated space or legacy index-based
 * positioning. Applies amplitude scaling based on data characteristics and user controls.
 *
 * @param display_options Display configuration for the analog series
 * @param key Series key for debugging output
 * @param stdDev Standard deviation of the data for amplitude scaling
 * @param series_index Index of this series for legacy positioning
 * @param ySpacing Vertical spacing between series for legacy positioning
 * @param center_coord Centering coordinate for legacy positioning
 * @param global_zoom Global zoom factor applied to all series
 * @return Model transformation matrix
 */
glm::mat4 getAnalogModelMat(AnalogTimeSeriesDisplayOptions const * display_options,
                            std::string const & key,
                            float stdDev,
                            int series_index,
                            float ySpacing,
                            float center_coord,
                            float global_zoom);

/**
 * @brief Create View matrix for analog series global transformations
 *
 * Handles view-level transformations applied to all analog series.
 * Currently returns identity matrix as pan offset is handled in projection.
 *
 * @return View transformation matrix
 */
glm::mat4 getAnalogViewMat();

/**
 * @brief Create Projection matrix for analog series coordinate mapping
 *
 * Maps world coordinates to screen coordinates for analog series,
 * including dynamic viewport adjustments for panning and zooming.
 *
 * @param start_time Start time of visible range
 * @param end_time End time of visible range
 * @param yMin Minimum Y coordinate of viewport
 * @param yMax Maximum Y coordinate of viewport
 * @param verticalPanOffset Vertical pan offset for dynamic viewport
 * @return Projection transformation matrix
 */
glm::mat4 getAnalogProjectionMat(float start_time,
                                 float end_time,
                                 float yMin,
                                 float yMax,
                                 float verticalPanOffset);

#endif// MVP_ANALOGTIMESERIES_HPP
