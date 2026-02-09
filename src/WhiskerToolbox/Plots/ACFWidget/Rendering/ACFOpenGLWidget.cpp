#include "ACFOpenGLWidget.hpp"

#include "Core/ACFState.hpp"
#include "CorePlotting/Mappers/HistogramMapper.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

ACFOpenGLWidget::ACFOpenGLWidget(QWidget * parent)
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

ACFOpenGLWidget::~ACFOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void ACFOpenGLWidget::setState(std::shared_ptr<ACFState> state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        _cached_view_state = _state->viewState();
        connect(_state.get(), &ACFState::stateChanged,
                this, &ACFOpenGLWidget::onStateChanged);
        connect(_state.get(), &ACFState::viewStateChanged,
                this, &ACFOpenGLWidget::onViewStateChanged);
        updateMatrices();
        update();
    }
}

void ACFOpenGLWidget::setHistogramData(
    CorePlotting::HistogramData const & data,
    CorePlotting::HistogramDisplayMode mode,
    CorePlotting::HistogramStyle const & style)
{
    _histogram_data = data;
    _histogram_mode = mode;
    _histogram_style = style;
    _scene_dirty = true;

    // Auto-fit axes to data if state is available; keep view state in sync via setXBounds/setYBounds
    if (_state && !data.counts.empty()) {
        double max_val = data.maxCount();
        if (max_val > 0.0) {
            auto * vas = _state->verticalAxisState();
            if (vas) {
                double new_y_max = max_val * 1.1;  // 10% padding
                if (std::abs(vas->getYMax() - new_y_max) > 0.01) {
                    vas->setYMax(new_y_max);
                }
                _state->setYBounds(vas->getYMin(), vas->getYMax());
            }
        }
        auto * has = _state->horizontalAxisState();
        if (has) {
            has->setXMin(data.bin_start);
            has->setXMax(data.binEnd());
            _state->setXBounds(has->getXMin(), has->getXMax());
        }
    }

    update();
}

void ACFOpenGLWidget::clearHistogramData()
{
    _histogram_data = {};
    _scene_dirty = true;
    update();
}

void ACFOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    if (!_scene_renderer.initialize()) {
        qWarning() << "ACFOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
}

void ACFOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_initialized) {
        return;
    }

    if (_scene_dirty) {
        uploadHistogramScene();
        _scene_dirty = false;
    }

    _scene_renderer.render(_view_matrix, _projection_matrix);
}

void ACFOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void ACFOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void ACFOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
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

void ACFOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void ACFOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void ACFOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void ACFOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void ACFOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void ACFOpenGLWidget::updateMatrices()
{
    _projection_matrix =
        WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);
}

void ACFOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    float const x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const y_range = static_cast<float>(_cached_view_state.y_max - _cached_view_state.y_min);
    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y,
        x_range, y_range, _widget_width, _widget_height);
}

void ACFOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(
        *_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF ACFOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(
        _projection_matrix, _widget_width, _widget_height, screen_pos);
}

void ACFOpenGLWidget::uploadHistogramScene()
{
    if (_histogram_data.counts.empty()) {
        _scene_renderer.clearScene();
        return;
    }

    auto scene = CorePlotting::HistogramMapper::buildScene(
        _histogram_data, _histogram_mode, _histogram_style);

    _scene_renderer.uploadScene(scene);
}
