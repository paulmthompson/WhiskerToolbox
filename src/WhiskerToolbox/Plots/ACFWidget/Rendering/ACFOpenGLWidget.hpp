#ifndef ACF_OPENGL_WIDGET_HPP
#define ACF_OPENGL_WIDGET_HPP

/**
 * @file ACFOpenGLWidget.hpp
 * @brief OpenGL-based ACF visualization widget
 *
 * Single source of truth: ACFState (view state + axis states).
 * Supports pan and zoom; updates state on interaction and reads from state
 * for projection. Rendering is mostly empty (placeholder for future ACF drawing).
 *
 * @see ACFState
 */

#include "Core/ACFState.hpp"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include <glm/glm.hpp>
#include <memory>

class QMouseEvent;
class QWheelEvent;

/**
 * @brief OpenGL widget for rendering ACF plots
 *
 * Displays 2D ACF with pan/zoom; state holds view transform and axis ranges.
 */
class ACFOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit ACFOpenGLWidget(QWidget * parent = nullptr);
    ~ACFOpenGLWidget() override;

    ACFOpenGLWidget(ACFOpenGLWidget const &) = delete;
    ACFOpenGLWidget & operator=(ACFOpenGLWidget const &) = delete;
    ACFOpenGLWidget(ACFOpenGLWidget &&) = delete;
    ACFOpenGLWidget & operator=(ACFOpenGLWidget &&) = delete;

    void setState(std::shared_ptr<ACFState> state);

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
    std::shared_ptr<ACFState> _state;
    int _widget_width{1};
    int _widget_height{1};

    ACFViewState _cached_view_state;
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

#endif  // ACF_OPENGL_WIDGET_HPP
