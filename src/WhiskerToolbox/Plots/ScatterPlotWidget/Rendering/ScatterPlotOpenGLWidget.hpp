#ifndef SCATTER_PLOT_OPENGL_WIDGET_HPP
#define SCATTER_PLOT_OPENGL_WIDGET_HPP

/**
 * @file ScatterPlotOpenGLWidget.hpp
 * @brief OpenGL-based scatter plot visualization widget
 * 
 * This widget renders 2D scatter plots showing relationships
 * between two variables.
 * 
 * Architecture:
 * - Receives ScatterPlotState for axis ranges and plot options
 * - Uses OpenGL for efficient rendering
 * 
 * @see ScatterPlotState
 */

#include "Core/ScatterPlotState.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <memory>

class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering scatter plots
 * 
 * Displays 2D scatter plots with configurable axis ranges.
 */
class ScatterPlotOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit ScatterPlotOpenGLWidget(QWidget * parent = nullptr);
    ~ScatterPlotOpenGLWidget() override;

    // Non-copyable, non-movable (GL resources)
    ScatterPlotOpenGLWidget(ScatterPlotOpenGLWidget const &) = delete;
    ScatterPlotOpenGLWidget & operator=(ScatterPlotOpenGLWidget const &) = delete;
    ScatterPlotOpenGLWidget(ScatterPlotOpenGLWidget &&) = delete;
    ScatterPlotOpenGLWidget & operator=(ScatterPlotOpenGLWidget &&) = delete;

    /**
     * @brief Set the ScatterPlotState for this widget
     * 
     * The state provides axis ranges and plot options.
     * The widget connects to state signals to react to changes.
     * 
     * @param state Shared pointer to ScatterPlotState
     */
    void setState(std::shared_ptr<ScatterPlotState> state);

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
    std::shared_ptr<ScatterPlotState> _state;

    // Widget dimensions
    int _widget_width{1};
    int _widget_height{1};
};

#endif // SCATTER_PLOT_OPENGL_WIDGET_HPP
