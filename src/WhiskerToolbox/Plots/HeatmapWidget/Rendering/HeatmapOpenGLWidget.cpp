#include "HeatmapOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

HeatmapOpenGLWidget::HeatmapOpenGLWidget(QWidget * parent)
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

HeatmapOpenGLWidget::~HeatmapOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void HeatmapOpenGLWidget::setState(std::shared_ptr<HeatmapState> state)
{
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        connect(_state.get(), &HeatmapState::stateChanged,
                this, &HeatmapOpenGLWidget::onStateChanged);
        connect(_state.get(), &HeatmapState::viewStateChanged,
                this, &HeatmapOpenGLWidget::onViewStateChanged);
        connect(_state.get(), &HeatmapState::backgroundColorChanged,
                this, [this]() {
                    updateBackgroundColor();
                    update();
                });

        _cached_view_state = _state->viewState();
        _scene_dirty = true;
    }
}

void HeatmapOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

HeatmapViewState const & HeatmapOpenGLWidget::viewState() const
{
    return _cached_view_state;
}

void HeatmapOpenGLWidget::resetView()
{
    if (_state) {
        _state->setViewState(HeatmapViewState{});
    }
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void HeatmapOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    updateBackgroundColor();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    if (!_scene_renderer.initialize()) {
        qWarning() << "HeatmapOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }
    _opengl_initialized = true;
    updateMatrices();
}

void HeatmapOpenGLWidget::paintGL()
{
    updateBackgroundColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!_opengl_initialized) {
        return;
    }
    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }
    // TODO: Render heatmap visualization here
}

void HeatmapOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void HeatmapOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void HeatmapOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() & Qt::LeftButton) {
        int dx = event->pos().x() - _click_start_pos.x();
        int dy = event->pos().y() - _click_start_pos.y();
        int distance_squared = dx * dx + dy * dy;

        if (!_is_panning && distance_squared > DRAG_THRESHOLD * DRAG_THRESHOLD) {
            _is_panning = true;
            setCursor(Qt::ClosedHandCursor);
        }

        if (_is_panning) {
            int delta_x = event->pos().x() - _last_mouse_pos.x();
            int delta_y = event->pos().y() - _last_mouse_pos.y();
            handlePanning(delta_x, delta_y);
        }
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void HeatmapOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void HeatmapOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF world = screenToWorld(event->pos());
        emit plotDoubleClicked(static_cast<int64_t>(world.x()));
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void HeatmapOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float delta = event->angleDelta().y() / 120.0f;
    bool const shift_pressed = event->modifiers() & Qt::ShiftModifier;
    bool const ctrl_pressed = event->modifiers() & Qt::ControlModifier;
    handleZoom(delta, shift_pressed, ctrl_pressed);
    event->accept();
}

// =============================================================================
// Slots
// =============================================================================

void HeatmapOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void HeatmapOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

// =============================================================================
// Private Methods
// =============================================================================

void HeatmapOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        _scene_renderer.clearScene();
        return;
    }
    // TODO: Gather trial-aligned AnalogTimeSeries data
    // TODO: Build heatmap visualization scene
    _scene_renderer.clearScene();
}

void HeatmapOpenGLWidget::updateMatrices()
{
    float x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float x_center = static_cast<float>(_cached_view_state.x_min + _cached_view_state.x_max) / 2.0f;

    float zoomed_x_range = x_range / static_cast<float>(_cached_view_state.x_zoom);
    float zoomed_y_range = 2.0f / static_cast<float>(_cached_view_state.y_zoom);

    float pan_x = static_cast<float>(_cached_view_state.x_pan);
    float pan_y = static_cast<float>(_cached_view_state.y_pan);

    float left = x_center - zoomed_x_range / 2.0f + pan_x;
    float right = x_center + zoomed_x_range / 2.0f + pan_x;
    float bottom = -zoomed_y_range / 2.0f + pan_y;
    float top = zoomed_y_range / 2.0f + pan_y;

    _projection_matrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    _view_matrix = glm::mat4(1.0f);
}

QPointF HeatmapOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    float ndc_x = (2.0f * screen_pos.x() / _widget_width) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_pos.y() / _widget_height);

    glm::mat4 inv_proj = glm::inverse(_projection_matrix);
    glm::vec4 ndc(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 world = inv_proj * ndc;

    return QPointF(world.x, world.y);
}

void HeatmapOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) return;

    float world_per_pixel_x = (_cached_view_state.x_max - _cached_view_state.x_min) /
                              (_widget_width * _cached_view_state.x_zoom);
    float world_per_pixel_y = 2.0f / (_widget_height * _cached_view_state.y_zoom);

    float new_pan_x = _cached_view_state.x_pan - delta_x * world_per_pixel_x;
    float new_pan_y = _cached_view_state.y_pan + delta_y * world_per_pixel_y;

    _state->setPan(new_pan_x, new_pan_y);
}

void HeatmapOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) return;

    float factor = std::pow(1.1f, delta);

    if (y_only) {
        _state->setYZoom(_cached_view_state.y_zoom * factor);
    } else if (both_axes) {
        _state->setXZoom(_cached_view_state.x_zoom * factor);
        _state->setYZoom(_cached_view_state.y_zoom * factor);
    } else {
        _state->setXZoom(_cached_view_state.x_zoom * factor);
    }
}

void HeatmapOpenGLWidget::updateBackgroundColor()
{
    if (!_state) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    QString hex_color = _state->getBackgroundColor();
    QColor color(hex_color);
    if (color.isValid()) {
        glClearColor(
            static_cast<float>(color.redF()),
            static_cast<float>(color.greenF()),
            static_cast<float>(color.blueF()),
            1.0f);
    } else {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
