#ifndef COREPLOTTING_COORDINATETRANSFORM_SCREENTOWORLD_HPP
#define COREPLOTTING_COORDINATETRANSFORM_SCREENTOWORLD_HPP

#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Converts screen coordinates to world coordinates.
 * 
 * @param screen_pos Screen position (x, y) in pixels.
 * @param screen_size Screen dimensions (width, height) in pixels.
 * @param view_matrix The current view matrix.
 * @param projection_matrix The current projection matrix.
 * @return World position (x, y).
 */
glm::vec2 screenToWorld(glm::vec2 screen_pos, glm::vec2 screen_size, glm::mat4 const & view_matrix, glm::mat4 const & projection_matrix);

/**
 * @brief Converts world coordinates to screen coordinates.
 * 
 * @param world_pos World position (x, y).
 * @param screen_size Screen dimensions (width, height) in pixels.
 * @param view_matrix The current view matrix.
 * @param projection_matrix The current projection matrix.
 * @return Screen position (x, y) in pixels.
 */
glm::vec2 worldToScreen(glm::vec2 world_pos, glm::vec2 screen_size, glm::mat4 const & view_matrix, glm::mat4 const & projection_matrix);

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_SCREENTOWORLD_HPP
