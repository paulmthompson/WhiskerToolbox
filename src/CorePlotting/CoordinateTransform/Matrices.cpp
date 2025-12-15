#include "Matrices.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace CorePlotting {

glm::mat4 createOrthoProjection(float left, float right, float bottom, float top, float near, float far) {
    return glm::ortho(left, right, bottom, top, near, far);
}

glm::mat4 createViewMatrix(float pan_x, float pan_y, float zoom_x, float zoom_y) {
    glm::mat4 view(1.0f);

    // 1. Scale (Zoom)
    // Scaling around the origin (0,0).
    view = glm::scale(view, glm::vec3(zoom_x, zoom_y, 1.0f));

    // 2. Translate (Pan)
    // We translate the world by (pan_x, pan_y).
    view = glm::translate(view, glm::vec3(pan_x, pan_y, 0.0f));

    return view;
}

glm::mat4 createModelMatrix(float scale_x, float scale_y, float translate_x, float translate_y) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(translate_x, translate_y, 0.0f));
    model = glm::scale(model, glm::vec3(scale_x, scale_y, 1.0f));
    return model;
}

}// namespace CorePlotting
