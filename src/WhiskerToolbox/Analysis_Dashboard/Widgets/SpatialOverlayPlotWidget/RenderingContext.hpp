#ifndef RENDERINGCONTEXT_HPP
#define RENDERINGCONTEXT_HPP

#include <QMatrix4x4>
#include <QRect>

/**
 * @brief Holds rendering context information for a single frame.
 *
 * This struct encapsulates all the necessary data required for rendering
 * and transforming coordinates between different spaces (world, screen, etc.).
 * It is passed to visualization objects during rendering and selection passes.
 */
struct RenderingContext {
    QMatrix4x4 model_matrix;
    QMatrix4x4 view_matrix;
    QMatrix4x4 projection_matrix;
    QRect viewport_rect;      // Viewport dimensions in pixels
    QRectF world_bounds;      // Viewport bounds in world coordinates
};

#endif // RENDERINGCONTEXT_HPP 