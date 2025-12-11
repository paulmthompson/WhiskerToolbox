#include "ScreenToWorld.hpp"

namespace CorePlotting {

glm::vec2 screenToWorld(glm::vec2 screen_pos, glm::vec2 screen_size, glm::mat4 const & view_matrix, glm::mat4 const & projection_matrix) {
    // 1. Convert Screen to NDC
    // Screen: (0,0) top-left to (width, height) bottom-right
    // NDC: (-1, -1) bottom-left to (1, 1) top-right.
    // Qt/Window coords are (0,0) top-left.

    float ndc_x = (2.0f * screen_pos.x) / screen_size.x - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_pos.y) / screen_size.y;// Flip Y

    glm::vec4 ndc(ndc_x, ndc_y, 0.0f, 1.0f);

    // 2. Inverse Transform
    glm::mat4 inverse_vp = glm::inverse(projection_matrix * view_matrix);
    glm::vec4 world = inverse_vp * ndc;

    // Perspective divide (though usually w=1 for ortho)
    if (world.w != 0.0f) {
        world /= world.w;
    }

    return glm::vec2(world.x, world.y);
}

glm::vec2 worldToScreen(glm::vec2 world_pos, glm::vec2 screen_size, glm::mat4 const & view_matrix, glm::mat4 const & projection_matrix) {
    glm::vec4 world(world_pos.x, world_pos.y, 0.0f, 1.0f);

    // 1. Transform to Clip Space
    glm::vec4 clip = projection_matrix * view_matrix * world;

    // 2. Perspective Divide -> NDC
    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    // 3. NDC to Screen
    float screen_x = (ndc.x + 1.0f) * 0.5f * screen_size.x;
    float screen_y = (1.0f - ndc.y) * 0.5f * screen_size.y;// Flip Y

    return glm::vec2(screen_x, screen_y);
}

}// namespace CorePlotting
