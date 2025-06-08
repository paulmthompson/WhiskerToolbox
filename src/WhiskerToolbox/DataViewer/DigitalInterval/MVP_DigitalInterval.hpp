#ifndef MVP_DIGITALINTERVAL_HPP
#define MVP_DIGITALINTERVAL_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

struct DigitalIntervalSeriesDisplayOptions;

/**
 * @brief Create Model matrix for digital interval series positioning and scaling
 *
 * Handles series-specific transformations for digital interval series including
 * VerticalSpaceManager positioning with allocated height or full canvas mode.
 *
 * @param display_options Display configuration for the interval series
 * @param key Series key for debugging output
 * @return Model transformation matrix
 */
glm::mat4 getIntervalModelMat(DigitalIntervalSeriesDisplayOptions const * display_options,
                              std::string const & key);

/**
 * @brief Create View matrix for digital interval series global transformations
 *
 * Handles view-level transformations applied to all digital interval series.
 * Currently returns identity matrix as pan offset is handled in projection.
 *
 * @return View transformation matrix
 */
glm::mat4 getIntervalViewMat();

/**
 * @brief Create Projection matrix for digital interval series coordinate mapping
 *
 * Maps world coordinates to screen coordinates for digital interval series,
 * including dynamic viewport adjustments for panning and zooming.
 *
 * @param start_time Start time of visible range
 * @param end_time End time of visible range
 * @param yMin Minimum Y coordinate of viewport
 * @param yMax Maximum Y coordinate of viewport
 * @param verticalPanOffset Vertical pan offset for dynamic viewport
 * @return Projection transformation matrix
 */
glm::mat4 getIntervalProjectionMat(float start_time,
                                   float end_time,
                                   float yMin,
                                   float yMax,
                                   float verticalPanOffset);

#endif// MVP_DIGITALINTERVAL_HPP
