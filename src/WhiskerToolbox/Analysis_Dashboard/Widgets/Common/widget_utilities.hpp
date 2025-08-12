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

#endif// ANALYSIS_DASHBOARD_WIDGET_UTILITIES_HPP