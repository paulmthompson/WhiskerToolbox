#ifndef TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP
#define TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP

/**
 * @file TemporalProjectionOpenGLWidget.hpp
 * @brief OpenGL-based temporal projection view visualization widget
 * 
 * This widget renders 2D projections of data collapsed across temporal windows.
 * 
 * Architecture:
 * - Receives TemporalProjectionViewState for axis ranges and plot options
 * - Uses OpenGL for efficient rendering
 * 
 * @see TemporalProjectionViewState
 */

#include "Core/TemporalProjectionViewState.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <memory>

class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering temporal projection views
 * 
 * Displays 2D projections with configurable axis ranges.
 */
class TemporalProjectionOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit TemporalProjectionOpenGLWidget(QWidget * parent = nullptr);
    ~TemporalProjectionOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    TemporalProjectionOpenGLWidget(TemporalProjectionOpenGLWidget const &) = delete;
    TemporalProjectionOpenGLWidget & operator=(TemporalProjectionOpenGLWidget const &) = delete;
    TemporalProjectionOpenGLWidget(TemporalProjectionOpenGLWidget &&) = delete;
    TemporalProjectionOpenGLWidget & operator=(TemporalProjectionOpenGLWidget &&) = delete;

    /**
     * @brief Set the TemporalProjectionViewState for this widget
     * 
     * The state provides axis ranges and plot options.
     * The widget connects to state signals to react to changes.
     * 
     * @param state Shared pointer to TemporalProjectionViewState
     */
    void setState(std::shared_ptr<TemporalProjectionViewState> state);

signals:
    /**
     * @brief Emitted when view bounds change (axis ranges change)
     */
    void viewBoundsChanged();

protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    // Mouse interaction
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private slots:
    /**
     * @brief Update rendering when state changes
     */
    void onStateChanged();

private:
    // State management
    std::shared_ptr<TemporalProjectionViewState> _state;

    // Widget dimensions
    int _widget_width{1};
    int _widget_height{1};
};

#endif // TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP
