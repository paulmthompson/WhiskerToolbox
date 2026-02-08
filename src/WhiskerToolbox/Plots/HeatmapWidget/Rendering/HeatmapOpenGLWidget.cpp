#include "HeatmapOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

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
        _cached_view_state = _state->viewState();

        connect(_state.get(), &HeatmapState::stateChanged,
                this, &HeatmapOpenGLWidget::onStateChanged);
        connect(_state.get(), &HeatmapState::viewStateChanged,
                this, &HeatmapOpenGLWidget::onViewStateChanged);
        connect(_state.get(), &HeatmapState::backgroundColorChanged,
                this, [this]() {
                    updateBackgroundColor();
                    update();
                });

        _scene_dirty = true;
        updateMatrices();
    }
}

void HeatmapOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

CorePlotting::ViewStateData const & HeatmapOpenGLWidget::viewState() const
{
    return _cached_view_state;
}

void HeatmapOpenGLWidget::resetView()
{
    if (_state) {
        _state->setXZoom(1.0);
        _state->setYZoom(1.0);
        _state->setPan(0.0, 0.0);
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
        int const dx = event->pos().x() - _click_start_pos.x();
        int const dy = event->pos().y() - _click_start_pos.y();
        int const distance_squared = dx * dx + dy * dy;

        if (!_is_panning && distance_squared > DRAG_THRESHOLD * DRAG_THRESHOLD) {
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
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
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
    // Use only cached view state for X and Y ranges (single source of truth)
    float const x_range =
        static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const x_center =
        static_cast<float>(_cached_view_state.x_min + _cached_view_state.x_max) / 2.0f;

    float const y_min = static_cast<float>(_cached_view_state.y_min);
    float const y_max = static_cast<float>(_cached_view_state.y_max);
    float const y_range = y_max - y_min;
    float const y_center = (y_min + y_max) / 2.0f;

    _projection_matrix = WhiskerToolbox::Plots::computeOrthoProjection(
        _cached_view_state, x_range, x_center, y_range, y_center);
    _view_matrix = glm::mat4(1.0f);
}

QPointF HeatmapOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(
        _projection_matrix, _widget_width, _widget_height, screen_pos);
}

void HeatmapOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }

    float const x_range =
        static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const y_range =
        static_cast<float>(_cached_view_state.y_max - _cached_view_state.y_min);

    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y,
        x_range, y_range, _widget_width, _widget_height);
}

void HeatmapOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }

    WhiskerToolbox::Plots::handleZoom(
        *_state, _cached_view_state, delta, y_only, both_axes);
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
