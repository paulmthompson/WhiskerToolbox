#include "LinePlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QString>
#include <QWheelEvent>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

LinePlotOpenGLWidget::LinePlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent)
{
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // Enable multisampling for smooth lines
    setFormat(format);
}

LinePlotOpenGLWidget::~LinePlotOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void LinePlotOpenGLWidget::setState(std::shared_ptr<LinePlotState> state)
{
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        _cached_view_state = _state->viewState();

        connect(_state.get(), &LinePlotState::stateChanged,
                this, &LinePlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &LinePlotState::viewStateChanged,
                this, &LinePlotOpenGLWidget::onViewStateChanged);
        connect(_state.get(), &LinePlotState::windowSizeChanged,
                this, [this](double /* window_size */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &LinePlotState::plotSeriesAdded,
                this, [this](QString const & /* series_name */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &LinePlotState::plotSeriesRemoved,
                this, [this](QString const & /* series_name */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &LinePlotState::plotSeriesOptionsChanged,
                this, [this](QString const & /* series_name */) {
                    _scene_dirty = true;
                    update();
                });

        _scene_dirty = true;
        updateMatrices();
    }
}

void LinePlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

std::pair<double, double> LinePlotOpenGLWidget::getViewBounds() const
{
    if (!_state) {
        return {-500.0, 500.0};
    }
    auto const & vs = _state->viewState();
    return {vs.x_min, vs.x_max};
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void LinePlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (dark theme)
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable blending for smooth lines
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize the scene renderer
    if (!_scene_renderer.initialize()) {
        qWarning() << "LinePlotOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
    updateMatrices();
}

void LinePlotOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_initialized) {
        return;
    }

    // Rebuild scene if needed
    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    // Render the scene
    _scene_renderer.render(_view_matrix, _projection_matrix);
}

void LinePlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);

    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void LinePlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void LinePlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
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

void LinePlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    event->accept();
}

void LinePlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF world = screenToWorld(event->pos());
        // TODO: Emit plotDoubleClicked with proper time frame index
        emit plotDoubleClicked(static_cast<int64_t>(world.x()));
    }
    event->accept();
}

void LinePlotOpenGLWidget::wheelEvent(QWheelEvent * event)
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

void LinePlotOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void LinePlotOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void LinePlotOpenGLWidget::onWindowSizeChanged(double /* window_size */)
{
    _scene_dirty = true;
    update();
}

// =============================================================================
// Private Methods
// =============================================================================

void LinePlotOpenGLWidget::rebuildScene()
{
    
}

void LinePlotOpenGLWidget::updateMatrices()
{
    float const x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const x_center = static_cast<float>(_cached_view_state.x_min + _cached_view_state.x_max) / 2.0f;

    float const zoomed_x_range = x_range / static_cast<float>(_cached_view_state.x_zoom);
    float y_min = 0.0f;
    float y_max = 100.0f;
    if (_state) {
        auto * vas = _state->verticalAxisState();
        if (vas) {
            y_min = static_cast<float>(vas->getYMin());
            y_max = static_cast<float>(vas->getYMax());
        }
    }
    float const y_range = y_max - y_min;
    float const y_center = (y_min + y_max) / 2.0f;
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

QPointF LinePlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    // Normalize screen coordinates to [-1, 1]
    float ndc_x = (2.0f * screen_pos.x() / _widget_width) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_pos.y() / _widget_height); // Flip Y

    // Invert projection to get world coordinates
    glm::mat4 inv_proj = glm::inverse(_projection_matrix);
    glm::vec4 ndc(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 world = inv_proj * ndc;

    return QPointF(world.x, world.y);
}

void LinePlotOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }

    float const x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const world_per_pixel_x = x_range / (_widget_width * static_cast<float>(_cached_view_state.x_zoom));

    float y_range = 100.0f;
    if (auto * vas = _state->verticalAxisState()) {
        y_range = static_cast<float>(vas->getYMax() - vas->getYMin());
    }
    float const world_per_pixel_y = y_range / (_widget_height * static_cast<float>(_cached_view_state.y_zoom));

    float const new_pan_x = static_cast<float>(_cached_view_state.x_pan) - delta_x * world_per_pixel_x;
    float const new_pan_y = static_cast<float>(_cached_view_state.y_pan) + delta_y * world_per_pixel_y;

    _state->setPan(new_pan_x, new_pan_y);
}

void LinePlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
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

GatherResult<AnalogTimeSeries> LinePlotOpenGLWidget::gatherTrialData() const
{
    if (!_data_manager || !_state) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Get the first series key from the plot series
    auto series_names = _state->getPlotSeriesNames();
    if (series_names.empty()) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Get the first series's options
    auto const series_options = _state->getPlotSeriesOptions(series_names.front());
    if (!series_options || series_options->series_key.empty()) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Get alignment state
    auto alignment_state = _state->alignmentState();
    if (!alignment_state) {
        return GatherResult<AnalogTimeSeries>{};
    }

    // Use the PlotAlignmentGather API for AnalogTimeSeries
    return WhiskerToolbox::Plots::createAlignedGatherResult<AnalogTimeSeries>(
        _data_manager,
        series_options->series_key,
        alignment_state->data());
}
