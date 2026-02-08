#include "ScatterPlotOpenGLWidget.hpp"

#include "Core/ScatterPlotState.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    setFormat(format);
}

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget()
{
    makeCurrent();
    doneCurrent();
}

void ScatterPlotOpenGLWidget::setState(std::shared_ptr<ScatterPlotState> state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        _cached_view_state = _state->viewState();
        connect(_state.get(), &ScatterPlotState::stateChanged,
                this, &ScatterPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &ScatterPlotState::viewStateChanged,
                this, &ScatterPlotOpenGLWidget::onViewStateChanged);
        updateMatrices();
        update();
    }
}

void ScatterPlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ScatterPlotOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // TODO: Render scatter points when data pipeline is available
}

void ScatterPlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void ScatterPlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() & Qt::LeftButton) {
        int const dx = event->pos().x() - _click_start_pos.x();
        int const dy = event->pos().y() - _click_start_pos.y();
        int const distance_sq = dx * dx + dy * dy;
        if (!_is_panning && distance_sq > DRAG_THRESHOLD * DRAG_THRESHOLD) {
            _is_panning = true;
            setCursor(Qt::ClosedHandCursor);
        }
        if (_is_panning) {
            int const delta_x = event->pos().x() - _last_mouse_pos.x();
            int const delta_y = event->pos().y() - _last_mouse_pos.y();
            handlePanning(delta_x, delta_y);
        }
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void ScatterPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void ScatterPlotOpenGLWidget::onStateChanged()
{
    update();
}

void ScatterPlotOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void ScatterPlotOpenGLWidget::updateMatrices()
{
    // Use only cached view state for X and Y ranges (single source of truth)
    float const x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const x_center =
        static_cast<float>(_cached_view_state.x_min + _cached_view_state.x_max) / 2.0f;
    float const y_range = static_cast<float>(_cached_view_state.y_max - _cached_view_state.y_min);
    float const y_center =
        static_cast<float>(_cached_view_state.y_min + _cached_view_state.y_max) / 2.0f;

    _projection_matrix = WhiskerToolbox::Plots::computeOrthoProjection(
        _cached_view_state, x_range, x_center, y_range, y_center);
    _view_matrix = glm::mat4(1.0f);
}

void ScatterPlotOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    float const x_range =
        static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const y_range =
        static_cast<float>(_cached_view_state.y_max - _cached_view_state.y_min);

    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y, x_range, y_range,
        _widget_width, _widget_height);
}

void ScatterPlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(
        *_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF ScatterPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(
        _projection_matrix, _widget_width, _widget_height, screen_pos);
}
