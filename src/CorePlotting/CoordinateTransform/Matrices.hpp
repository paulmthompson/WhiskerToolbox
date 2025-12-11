#ifndef COREPLOTTING_COORDINATETRANSFORM_MATRICES_HPP
#define COREPLOTTING_COORDINATETRANSFORM_MATRICES_HPP

#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Creates a standard orthographic projection matrix.
 * 
 * Maps the specified world volume to NDC [-1, 1].
 */
glm::mat4 createOrthoProjection(float left, float right, float bottom, float top, float near = -1.0f, float far = 1.0f);

/**
 * @brief Creates a view matrix for 2D panning and zooming.
 * 
 * @param pan_x Horizontal pan in World Units.
 * @param pan_y Vertical pan in World Units.
 * @param zoom_x Horizontal zoom factor.
 * @param zoom_y Vertical zoom factor.
 */
glm::mat4 createViewMatrix(float pan_x, float pan_y, float zoom_x, float zoom_y);

/**
 * @brief Creates a model matrix for 2D scaling and translation.
 * 
 * Applied as: Translate * Scale * Vertex
 */
glm::mat4 createModelMatrix(float scale_x, float scale_y, float translate_x, float translate_y);

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_MATRICES_HPP
