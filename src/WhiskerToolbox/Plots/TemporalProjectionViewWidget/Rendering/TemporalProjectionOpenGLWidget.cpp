#include "TemporalProjectionOpenGLWidget.hpp"

#include "Core/TemporalProjectionViewState.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

TemporalProjectionOpenGLWidget::TemporalProjectionOpenGLWidget(QWidget * parent)
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

TemporalProjectionOpenGLWidget::~TemporalProjectionOpenGLWidget()
{
    makeCurrent();
    doneCurrent();
}

void TemporalProjectionOpenGLWidget::setState(std::shared_ptr<TemporalProjectionViewState> state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        _cached_view_state = _state->viewState();
        connect(_state.get(), &TemporalProjectionViewState::stateChanged,
                this, &TemporalProjectionOpenGLWidget::onStateChanged);
        connect(_state.get(), &TemporalProjectionViewState::viewStateChanged,
                this, &TemporalProjectionOpenGLWidget::onViewStateChanged);
        updateMatrices();
        update();
    }
}

void TemporalProjectionOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TemporalProjectionOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // TODO: Add temporal projection rendering logic here
}

void TemporalProjectionOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void TemporalProjectionOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void TemporalProjectionOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
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

void TemporalProjectionOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void TemporalProjectionOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void TemporalProjectionOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void TemporalProjectionOpenGLWidget::onStateChanged()
{
    update();
}

void TemporalProjectionOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void TemporalProjectionOpenGLWidget::updateMatrices()
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

void TemporalProjectionOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    float const x_range =
        static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const y_range =
        static_cast<float>(_cached_view_state.y_max - _cached_view_state.y_min);

    WhiskerToolbox::Plots::handlePanning(*_state, _cached_view_state, delta_x, delta_y,
                                         x_range, y_range, _widget_width, _widget_height);
}

void TemporalProjectionOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(*_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF TemporalProjectionOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(_projection_matrix, _widget_width,
                                               _widget_height, screen_pos);
}
