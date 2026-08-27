/**
 * @file QtOpenGLBenchmarkFixture.hpp
 * @brief Shared Qt/OpenGL initialization for OpenGLWidget stress/probe benchmarks.
 */

#ifndef QT_OPENGL_BENCHMARK_FIXTURE_HPP
#define QT_OPENGL_BENCHMARK_FIXTURE_HPP

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QWidget>

#include <iostream>

namespace Neuralyzer::Benchmark {

/**
 * @brief Apply default OpenGL surface format used by plot widgets in this repo.
 */
inline QSurfaceFormat defaultPlotWidgetSurfaceFormat() {
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setSamples(4);
    return format;
}

/**
 * @brief Initialize Qt platform defaults for OpenGL widget benchmarks.
 *
 * Does not override QT_QPA_PLATFORM — respects the user's display setup (X11, Wayland, etc.).
 */
inline void initializeQtPlatformForOpenGLBenchmarks() {
    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        qputenv("QT_LOGGING_RULES", QByteArray("qt.qpa.wayland*=false"));
    }

    QSurfaceFormat::setDefaultFormat(defaultPlotWidgetSurfaceFormat());
}

/**
 * @brief Configure a QOpenGLWidget with the same format as in-app plot widgets.
 */
inline void configureOpenGLWidgetFormat(QOpenGLWidget & widget) {
    widget.setFormat(defaultPlotWidgetSurfaceFormat());
}

/**
 * @brief Configure a QOpenGLWidget for offscreen rendering (no visible window).
 *
 * Still requires @c show() so Qt creates a platform surface and GL context.
 */
inline void configureOffscreenOpenGLWidget(QOpenGLWidget & widget) {
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    configureOpenGLWidgetFormat(widget);
}

/**
 * @brief Block until the widget's OpenGL context is valid or timeout.
 * @return true when context()->isValid(), false on timeout.
 */
inline bool waitForOpenGLWidgetContext(QOpenGLWidget & widget, int timeout_ms = 10'000) {
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        widget.update();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        if (widget.context() != nullptr && widget.context()->isValid()) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Show an offscreen QOpenGLWidget and wait until its context is valid.
 * @return true when context()->isValid(), false on timeout.
 */
inline bool showOffscreenOpenGLWidget(QOpenGLWidget & widget, int timeout_ms = 10'000) {
    configureOffscreenOpenGLWidget(widget);
    widget.show();
    return waitForOpenGLWidgetContext(widget, timeout_ms);
}

/**
 * @brief Hide the widget and drain pending GL teardown events before destruction.
 */
inline void shutdownOpenGLWidgetForBenchmark(QOpenGLWidget & widget) {
    widget.hide();
    if (widget.context() != nullptr && widget.context()->isValid()) {
        widget.makeCurrent();
        widget.doneCurrent();
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

/**
 * @brief Print platform and GL context status for benchmark logs.
 */
inline void printOpenGLWidgetContextStatus(QOpenGLWidget & widget) {
    std::cout << "  qt_platform=" << QGuiApplication::platformName().toStdString() << '\n';
    if (widget.context() != nullptr && widget.context()->isValid()) {
        widget.makeCurrent();
        std::cout << "  opengl_context=valid\n";
        widget.doneCurrent();
    } else {
        std::cout << "  opengl_context=invalid\n";
    }
}

}// namespace Neuralyzer::Benchmark

#endif// QT_OPENGL_BENCHMARK_FIXTURE_HPP
