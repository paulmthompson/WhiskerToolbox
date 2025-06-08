#ifndef MVP_DIGITALEVENT_HPP
#define MVP_DIGITALEVENT_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct DigitalEventSeriesDisplayOptions;

/**
 * @brief Create Model matrix for digital event series positioning and scaling
 *
 * Handles series-specific transformations for digital event series including
 * stacked positioning with VerticalSpaceManager or legacy index-based positioning.
 *
 * @param display_options Display configuration for the event series
 * @param visible_series_index Index of this series among visible series
 * @param center_coord Centering coordinate for legacy positioning
 * @return Model transformation matrix
 */
glm::mat4 getEventModelMat(DigitalEventSeriesDisplayOptions const * display_options,
                           int visible_series_index,
                           int center_coord);

/**
 * @brief Create View matrix for digital event series global transformations
 *
 * Handles view-level transformations applied to all digital event series.
 * Currently returns identity matrix as pan offset is handled in projection.
 *
 * @return View transformation matrix
 */
glm::mat4 getEventViewMat();

/**
 * @brief Create Projection matrix for digital event series coordinate mapping
 *
 * Maps world coordinates to screen coordinates for digital event series,
 * including dynamic viewport adjustments for panning and zooming.
 *
 * @param yMin Minimum Y coordinate of viewport
 * @param yMax Maximum Y coordinate of viewport
 * @param verticalPanOffset Vertical pan offset for dynamic viewport
 * @param start_time Start time of visible range
 * @param end_time End time of visible range
 * @return Projection transformation matrix
 */
glm::mat4 getEventProjectionMat(float yMin,
                                float yMax,
                                float verticalPanOffset,
                                int64_t start_time,
                                int64_t end_time);

#endif// MVP_DIGITALEVENT_HPP
