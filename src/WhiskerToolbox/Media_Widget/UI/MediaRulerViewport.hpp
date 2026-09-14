#ifndef MEDIA_RULER_VIEWPORT_HPP
#define MEDIA_RULER_VIEWPORT_HPP

/**
 * @file MediaRulerViewport.hpp
 * @brief Compute visible media-pixel bounds for Media Viewer rulers
 */

#include <QGraphicsView>

#include <algorithm>

/**
 * @brief Visible media-pixel coordinate range in the graphics view
 */
struct MediaRulerViewport {
    double min_x = 0.0; ///< Left edge in media pixels
    double max_x = 0.0; ///< Right edge in media pixels
    double min_y = 0.0; ///< Top edge in media pixels
    double max_y = 0.0; ///< Bottom edge in media pixels
};

/**
 * @brief Compute the media-pixel range currently visible in the graphics view
 * @param view Graphics view displaying the media canvas
 * @param x_aspect Scene-to-media X scale factor (canvas_width / logical_width)
 * @param y_aspect Scene-to-media Y scale factor (canvas_height / logical_height)
 * @return Visible bounds in media/source pixel coordinates
 * @pre x_aspect > 0
 * @pre y_aspect > 0
 */
[[nodiscard]] inline MediaRulerViewport computeVisibleMediaViewport(
        QGraphicsView const & view,
        float x_aspect,
        float y_aspect) {
    MediaRulerViewport viewport;

    if (x_aspect <= 0.0f || y_aspect <= 0.0f || view.viewport() == nullptr) {
        return viewport;
    }

    auto const * vp = view.viewport();
    auto const scene_tl = view.mapToScene(vp->rect().topLeft());
    auto const scene_br = view.mapToScene(vp->rect().bottomRight());

    viewport.min_x = scene_tl.x() / static_cast<double>(x_aspect);
    viewport.max_x = scene_br.x() / static_cast<double>(x_aspect);
    viewport.min_y = scene_tl.y() / static_cast<double>(y_aspect);
    viewport.max_y = scene_br.y() / static_cast<double>(y_aspect);

    if (viewport.min_x > viewport.max_x) {
        std::swap(viewport.min_x, viewport.max_x);
    }
    if (viewport.min_y > viewport.max_y) {
        std::swap(viewport.min_y, viewport.max_y);
    }

    return viewport;
}

#endif// MEDIA_RULER_VIEWPORT_HPP
