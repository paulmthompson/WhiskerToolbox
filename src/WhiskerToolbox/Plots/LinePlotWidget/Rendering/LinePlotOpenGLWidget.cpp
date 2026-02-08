#include "LinePlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
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
    if (!_state || !_data_manager) {
        _scene_renderer.clearScene();
        return;
    }

    // Gather trial-aligned analog time series data
    GatherResult<AnalogTimeSeries> gathered = gatherTrialData();

    if (gathered.empty()) {
        _scene_renderer.clearScene();
        return;
    }

    size_t const num_trials = gathered.size();

    // =========================================================================
    // Compute Y bounds (min/max signal values across all trials)
    // =========================================================================
    float y_min = std::numeric_limits<float>::max();
    float y_max = std::numeric_limits<float>::lowest();

    for (size_t trial = 0; trial < num_trials; ++trial) {
        auto const & trial_view = gathered[trial];
        if (!trial_view || trial_view->getNumSamples() == 0) {
            continue;
        }

        auto span = trial_view->getAnalogTimeSeries();
        if (span.empty()) {
            continue;
        }

        auto [min_it, max_it] = std::minmax_element(span.begin(), span.end());
        y_min = std::min(y_min, *min_it);
        y_max = std::max(y_max, *max_it);
    }

    // =========================================================================
    // Compute X bounds (relative time range across all trials)
    // =========================================================================
    // Each trial's data is expressed in absolute TimeFrameIndex.
    // We subtract the alignment time to get relative time, then find the
    // overall min/max relative time across all trials.
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();

    for (size_t trial = 0; trial < num_trials; ++trial) {
        auto const & trial_view = gathered[trial];
        if (!trial_view || trial_view->getNumSamples() == 0) {
            continue;
        }

        int64_t alignment_time = gathered.alignmentTimeAt(trial);

        // Get first and last time indices in this view
        auto all_samples = trial_view->view();
        // Views are time-ordered, so first and last give the range
        bool first = true;
        TimeFrameIndex first_time{0};
        TimeFrameIndex last_time{0};
        for (auto const & sample : all_samples) {
            if (first) {
                first_time = sample.time();
                first = false;
            }
            last_time = sample.time();
        }

        if (first) {
            // No samples
            continue;
        }

        double rel_start = static_cast<double>(first_time.getValue() - alignment_time);
        double rel_end = static_cast<double>(last_time.getValue() - alignment_time);
        x_min = std::min(x_min, rel_start);
        x_max = std::max(x_max, rel_end);
    }

    // =========================================================================
    // Apply bounds to state (updates view state and axes)
    // =========================================================================
    if (y_min < y_max) {
        // Add a small margin (5%) so lines don't sit on the edge
        float const y_margin = (y_max - y_min) * 0.05f;
        _state->setYBounds(
            static_cast<double>(y_min - y_margin),
            static_cast<double>(y_max + y_margin));
    }

    if (x_min < x_max) {
        _state->setXBounds(x_min, x_max);
    }

    // Cache updated view state
    _cached_view_state = _state->viewState();
    updateMatrices();

    // NOTE: Line rendering not yet implemented — scene is cleared for now.
    // Future: build line batches from gathered data using CorePlotting::SceneBuilder
    _scene_renderer.clearScene();
}

void LinePlotOpenGLWidget::updateMatrices()
{
    // Compute data ranges from view state (both X and Y bounds are in ViewStateData)
    float const x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const x_center = static_cast<float>(_cached_view_state.x_min + _cached_view_state.x_max) / 2.0f;

    float const y_min = static_cast<float>(_cached_view_state.y_min);
    float const y_max = static_cast<float>(_cached_view_state.y_max);
    float const y_range = y_max - y_min;
    float const y_center = (y_min + y_max) / 2.0f;

    // Use shared helper to compute projection matrix
    _projection_matrix = WhiskerToolbox::Plots::computeOrthoProjection(
        _cached_view_state, x_range, x_center, y_range, y_center);
    _view_matrix = glm::mat4(1.0f);
}

QPointF LinePlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(
        _projection_matrix, _widget_width, _widget_height, screen_pos);
}

void LinePlotOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }

    // Compute data ranges from view state (both X and Y bounds are in ViewStateData)
    float const x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float const y_range = static_cast<float>(_cached_view_state.y_max - _cached_view_state.y_min);

    // Use shared helper for panning logic
    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y,
        x_range, y_range, _widget_width, _widget_height);
}

void LinePlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }

    // Use shared helper for zoom logic
    WhiskerToolbox::Plots::handleZoom(
        *_state, _cached_view_state, delta, y_only, both_axes);
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
