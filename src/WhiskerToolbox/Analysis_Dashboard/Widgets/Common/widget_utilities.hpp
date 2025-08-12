#ifndef ANALYSIS_DASHBOARD_WIDGET_UTILITIES_HPP
#define ANALYSIS_DASHBOARD_WIDGET_UTILITIES_HPP

#include <QSurfaceFormat>
#include <QOpenGLWidget>
#include <algorithm>
#include "CoreGeometry/boundingbox.hpp"

inline bool try_create_opengl_context_with_version(QOpenGLWidget * widget, int major, int minor) {
    QSurfaceFormat format;
    format.setVersion(major, minor);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);

    widget->setFormat(format);

    return true;
}

/**
 * @brief Computes camera world view parameters based on data bounds and camera settings
 * 
 * @param data_bounds Bounding box containing the data extents
 * @param zoom_level_x Current zoom level for X axis
 * @param zoom_level_y Current zoom level for Y axis
 * @param pan_offset_x Current pan offset for X axis
 * @param pan_offset_y Current pan offset for Y axis
 * @param padding_factor Padding factor to apply around data bounds
 * @param center_x Output: Computed center X coordinate
 * @param center_y Output: Computed center Y coordinate
 * @param world_width Output: Computed world width
 * @param world_height Output: Computed world height
 */
inline void compute_camera_world_view(BoundingBox const & data_bounds,
                                     float zoom_level_x, float zoom_level_y,
                                     float pan_offset_x, float pan_offset_y,
                                     float padding_factor,
                                     float & center_x, float & center_y,
                                     float & world_width, float & world_height) {
    // Base on data bounds and current per-axis zoom and pan offsets
    float data_width = std::max(1e-6f, data_bounds.width());
    float data_height = std::max(1e-6f, data_bounds.height());
    float cx0 = data_bounds.center_x();
    float cy0 = data_bounds.center_y();

    // Padding applies when computing view extents
    float padding = padding_factor;

    // Visible extents in world units (no aspect correction here; handled by P)
    float half_w = 0.5f * (data_width * padding) / std::max(1e-6f, zoom_level_x);
    float half_h = 0.5f * (data_height * padding) / std::max(1e-6f, zoom_level_y);

    // Apply pan offsets in world units
    float pan_x_world = pan_offset_x * (data_width / std::max(1e-6f, zoom_level_x));
    float pan_y_world = pan_offset_y * (data_height / std::max(1e-6f, zoom_level_y));

    center_x = cx0 + pan_x_world;
    center_y = cy0 + pan_y_world;
    world_width = 2.0f * half_w;
    world_height = 2.0f * half_h;
}

#endif// ANALYSIS_DASHBOARD_WIDGET_UTILITIES_HPP