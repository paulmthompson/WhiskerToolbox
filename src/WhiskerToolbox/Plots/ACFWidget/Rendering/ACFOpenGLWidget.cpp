#include "ACFOpenGLWidget.hpp"

#include "Core/ACFState.hpp"
#include "CorePlotting/Mappers/HistogramMapper.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

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

    // Auto-fit vertical axis to data if state is available
    if (_state && !data.counts.empty()) {
        double max_val = data.maxCount();
        if (max_val > 0.0) {
            auto * vas = _state->verticalAxisState();
            if (vas) {
                double new_y_max = max_val * 1.1; // 10% padding
                if (std::abs(vas->getYMax() - new_y_max) > 0.01) {
                    vas->setYMax(new_y_max);
                }
            }
        }
        auto * has = _state->horizontalAxisState();
        if (has) {
            has->setXMin(data.bin_start);
            has->setXMax(data.binEnd());
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
    double x_min = 0.0;
    double x_max = 100.0;
    double y_min = 0.0;
    double y_max = 100.0;
    if (_state) {
        auto * has = _state->horizontalAxisState();
        auto * vas = _state->verticalAxisState();
        if (has) {
            x_min = has->getXMin();
            x_max = has->getXMax();
        }
        if (vas) {
            y_min = vas->getYMin();
            y_max = vas->getYMax();
        }
    }
    float const x_range = static_cast<float>(x_max - x_min);
    float const x_center = static_cast<float>(x_min + x_max) / 2.0f;
    float const zoomed_x_range = x_range / static_cast<float>(_cached_view_state.x_zoom);
    float const y_range = static_cast<float>(y_max - y_min);
    float const y_center = static_cast<float>(y_min + y_max) / 2.0f;
    float const zoomed_y_range = y_range / static_cast<float>(_cached_view_state.y_zoom);
    float const pan_x = static_cast<float>(_cached_view_state.x_pan);
    float const pan_y = static_cast<float>(_cached_view_state.y_pan);

    float const left = x_center - zoomed_x_range / 2.0f + pan_x;
    float const right = x_center + zoomed_x_range / 2.0f + pan_x;
    float const bottom = y_center - zoomed_y_range / 2.0f + pan_y;
    float const top = y_center + zoomed_y_range / 2.0f + pan_y;

    _projection_matrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    _view_matrix = glm::mat4(1.0f);
}

void ACFOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    double x_min = 0.0, x_max = 100.0;
    double y_min = 0.0, y_max = 100.0;
    if (auto * has = _state->horizontalAxisState()) {
        x_min = has->getXMin();
        x_max = has->getXMax();
    }
    if (auto * vas = _state->verticalAxisState()) {
        y_min = vas->getYMin();
        y_max = vas->getYMax();
    }
    float const x_range = static_cast<float>(x_max - x_min);
    float const world_per_pixel_x = x_range / (_widget_width * static_cast<float>(_cached_view_state.x_zoom));
    float const y_range = static_cast<float>(y_max - y_min);
    float const world_per_pixel_y = y_range / (_widget_height * static_cast<float>(_cached_view_state.y_zoom));

    float const new_pan_x = static_cast<float>(_cached_view_state.x_pan) - delta_x * world_per_pixel_x;
    float const new_pan_y = static_cast<float>(_cached_view_state.y_pan) + delta_y * world_per_pixel_y;
    _state->setPan(new_pan_x, new_pan_y);
}

void ACFOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    float const factor = std::pow(1.1f, delta);
    if (y_only) {
        _state->setYZoom(_cached_view_state.y_zoom * factor);
    } else if (both_axes) {
        _state->setXZoom(_cached_view_state.x_zoom * factor);
        _state->setYZoom(_cached_view_state.y_zoom * factor);
    } else {
        _state->setXZoom(_cached_view_state.x_zoom * factor);
    }
}

QPointF ACFOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    float const ndc_x = (2.0f * screen_pos.x() / _widget_width) - 1.0f;
    float const ndc_y = 1.0f - (2.0f * screen_pos.y() / _widget_height);
    glm::mat4 const inv_proj = glm::inverse(_projection_matrix);
    glm::vec4 const ndc(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 const world = inv_proj * ndc;
    return QPointF(world.x, world.y);
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
