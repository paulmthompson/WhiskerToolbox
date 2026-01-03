#include "InverseTransform.hpp"

#include "CorePlotting/Layout/LayoutTransform.hpp"

namespace CorePlotting {

// ============================================================================
// Canvas ↔ NDC Conversions
// ============================================================================

glm::vec2 canvasToNDC(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height) {

    // X: 0 → -1, width → +1
    float const ndc_x = (2.0f * canvas_x / static_cast<float>(viewport_width)) - 1.0f;

    // Y: 0 (top) → +1, height (bottom) → -1 (flip for OpenGL convention)
    float const ndc_y = 1.0f - (2.0f * canvas_y / static_cast<float>(viewport_height));

    return {ndc_x, ndc_y};
}

glm::vec2 ndcToCanvas(
        float ndc_x, float ndc_y,
        int viewport_width, int viewport_height) {

    float const canvas_x = (ndc_x + 1.0f) * 0.5f * static_cast<float>(viewport_width);
    float const canvas_y = (1.0f - ndc_y) * 0.5f * static_cast<float>(viewport_height);

    return {canvas_x, canvas_y};
}

// ============================================================================
// NDC ↔ World Conversions
// ============================================================================

glm::vec2 ndcToWorld(
        glm::vec2 ndc_pos,
        glm::mat4 const & inverse_vp) {

    // Use z=0 for 2D plotting (we're on the near plane)
    glm::vec4 const ndc_point(ndc_pos.x, ndc_pos.y, 0.0f, 1.0f);
    glm::vec4 const world_point = inverse_vp * ndc_point;

    // Perspective divide (for orthographic, w should be 1.0)
    if (std::abs(world_point.w) > 1e-10f) {
        return {world_point.x / world_point.w, world_point.y / world_point.w};
    }
    return {world_point.x, world_point.y};
}

glm::vec2 worldToNDC(
        glm::vec2 world_pos,
        glm::mat4 const & vp) {

    glm::vec4 const world_point(world_pos.x, world_pos.y, 0.0f, 1.0f);
    glm::vec4 const ndc_point = vp * world_point;

    if (std::abs(ndc_point.w) > 1e-10f) {
        return {ndc_point.x / ndc_point.w, ndc_point.y / ndc_point.w};
    }
    return {ndc_point.x, ndc_point.y};
}

// ============================================================================
// Combined Canvas ↔ World Conversions
// ============================================================================

glm::vec2 canvasToWorld(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix) {

    // Canvas → NDC
    glm::vec2 const ndc = canvasToNDC(canvas_x, canvas_y, viewport_width, viewport_height);

    // NDC → World (need inverse VP)
    glm::mat4 const vp = projection_matrix * view_matrix;
    glm::mat4 const inverse_vp = glm::inverse(vp);

    return ndcToWorld(ndc, inverse_vp);
}

glm::vec2 worldToCanvas(
        float world_x, float world_y,
        int viewport_width, int viewport_height,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix) {

    // World → NDC
    glm::mat4 const vp = projection_matrix * view_matrix;
    glm::vec2 const ndc = worldToNDC({world_x, world_y}, vp);

    // NDC → Canvas
    return ndcToCanvas(ndc.x, ndc.y, viewport_width, viewport_height);
}

// ============================================================================
// World ↔ Data Conversions
// ============================================================================


float worldYToDataY(float world_y, LayoutTransform const & y_transform) {
    return y_transform.inverse(world_y);
}

float dataYToWorldY(float data_y, LayoutTransform const & y_transform) {
    return y_transform.apply(data_y);
}


// ============================================================================
// Combined Canvas → Data Conversion
// ============================================================================

CanvasToDataResult canvasToData(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        LayoutTransform const & y_transform) {

    CanvasToDataResult result;

    // Canvas → World
    glm::vec2 const world = canvasToWorld(
            canvas_x, canvas_y,
            viewport_width, viewport_height,
            view_matrix, projection_matrix);

    result.world_x = world.x;
    result.world_y = world.y;

    // World → Data
    result.time_index = worldXToTimeIndex(world.x);
    result.data_y = worldYToDataY(world.y, y_transform);

    return result;
}

}// namespace CorePlotting