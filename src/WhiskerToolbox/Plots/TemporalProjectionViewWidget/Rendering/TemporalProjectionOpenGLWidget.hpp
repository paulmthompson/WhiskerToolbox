#ifndef TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP
#define TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP

/**
 * @file TemporalProjectionOpenGLWidget.hpp
 * @brief OpenGL-based temporal projection view visualization widget
 *
 * Uses CorePlotting::ViewStateData cached from state for projection and
 * interaction. Delegates to WhiskerToolbox::Plots helpers for ortho projection,
 * screenToWorld, panning, and zoom.
 *
 * @see TemporalProjectionViewState
 * @see PlotInteractionHelpers.hpp
 */

#include "Core/TemporalProjectionViewState.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>

class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering temporal projection views
 *
 * Displays 2D projections with pan/zoom; state holds view transform and axis
 * ranges.
 */
class TemporalProjectionOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit TemporalProjectionOpenGLWidget(QWidget * parent = nullptr);
    ~TemporalProjectionOpenGLWidget() override;

    TemporalProjectionOpenGLWidget(TemporalProjectionOpenGLWidget const &) = delete;
    TemporalProjectionOpenGLWidget & operator=(TemporalProjectionOpenGLWidget const &) = delete;
    TemporalProjectionOpenGLWidget(TemporalProjectionOpenGLWidget &&) = delete;
    TemporalProjectionOpenGLWidget & operator=(TemporalProjectionOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<TemporalProjectionViewState> state);

signals:
    void viewBoundsChanged();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;

private slots:
    void onStateChanged();
    void onViewStateChanged();

private:
    std::shared_ptr<TemporalProjectionViewState> _state;
    int _widget_width{1};
    int _widget_height{1};

    CorePlotting::ViewStateData _cached_view_state;
    glm::mat4 _projection_matrix{1.0f};
    glm::mat4 _view_matrix{1.0f};

    bool _is_panning{false};
    QPoint _click_start_pos;
    QPoint _last_mouse_pos;
    static constexpr int DRAG_THRESHOLD = 4;

    void updateMatrices();
    void handlePanning(int delta_x, int delta_y);
    void handleZoom(float delta, bool y_only, bool both_axes);
    [[nodiscard]] QPointF screenToWorld(QPoint const & screen_pos) const;
};

#endif  // TEMPORAL_PROJECTION_OPENGL_WIDGET_HPP
