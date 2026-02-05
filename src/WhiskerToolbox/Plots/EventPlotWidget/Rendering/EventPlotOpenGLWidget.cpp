#include "EventPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "CorePlotting/Mappers/RasterMapper.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreGeometry/boundingbox.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QToolTip>
#include <QWheelEvent>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

EventPlotOpenGLWidget::EventPlotOpenGLWidget(QWidget * parent)
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
    format.setSamples(4); // Enable multisampling for smooth points
    setFormat(format);

    // Initialize tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(500); // 500ms delay
    connect(_tooltip_timer, &QTimer::timeout, this, &EventPlotOpenGLWidget::onTooltipTimer);
}

EventPlotOpenGLWidget::~EventPlotOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void EventPlotOpenGLWidget::setState(std::shared_ptr<EventPlotState> state)
{
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = state;

    if (_state) {
        // Connect to state signals
        connect(_state.get(), &EventPlotState::stateChanged,
                this, &EventPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &EventPlotState::viewStateChanged,
                this, &EventPlotOpenGLWidget::onViewStateChanged);
        connect(_state.get(), &EventPlotState::axisOptionsChanged,
                this, [this]() {
                    _scene_dirty = true;
                    update();
                });

        // Initial sync
        _cached_view_state = _state->viewState();
        _scene_dirty = true;
    }
}

void EventPlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

EventPlotViewState const & EventPlotOpenGLWidget::viewState() const
{
    return _cached_view_state;
}

void EventPlotOpenGLWidget::resetView()
{
    if (_state) {
        _state->setViewState(EventPlotViewState{}); // Reset to defaults
    }
}

void EventPlotOpenGLWidget::setTooltipsEnabled(bool enabled)
{
    _tooltips_enabled = enabled;
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void EventPlotOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Set clear color (dark theme)
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable programmable point size
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Enable blending for smoother points
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling if available
    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize the scene renderer
    if (!_scene_renderer.initialize()) {
        qWarning() << "EventPlotOpenGLWidget: Failed to initialize SceneRenderer";
        return;
    }

    _opengl_initialized = true;
    updateMatrices();
}

void EventPlotOpenGLWidget::paintGL()
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

    // Render axes and center line
    renderCenterLine();
    renderAxes();
}

void EventPlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);

    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void EventPlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = true;
        _last_mouse_pos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    event->accept();
}

void EventPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    // Update tooltip position
    if (_tooltips_enabled && !_is_panning) {
        _pending_tooltip_pos = event->pos();
        _tooltip_timer->start();
    }

    if (_is_panning) {
        int delta_x = event->pos().x() - _last_mouse_pos.x();
        int delta_y = event->pos().y() - _last_mouse_pos.y();
        handlePanning(delta_x, delta_y);
        _last_mouse_pos = event->pos();
    }

    // Emit world position
    QPointF world = screenToWorld(event->pos());
    emit mouseWorldMoved(static_cast<float>(world.x()), static_cast<float>(world.y()));

    event->accept();
}

void EventPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    event->accept();
}

void EventPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF world = screenToWorld(event->pos());

        // Find event near click position
        auto hit = findEventNear(event->pos());
        if (hit) {
            // TODO: Get actual time frame index from gathered data
            // For now, emit position
            emit eventDoubleClicked(static_cast<int64_t>(world.x()), "");
        }
    }
    event->accept();
}

void EventPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float delta = event->angleDelta().y() / 120.0f;
    bool y_only = event->modifiers() & Qt::ShiftModifier;
    handleZoom(delta, y_only);
    event->accept();
}

void EventPlotOpenGLWidget::leaveEvent(QEvent * event)
{
    _tooltip_timer->stop();
    QToolTip::hideText();
    QOpenGLWidget::leaveEvent(event);
}

// =============================================================================
// Slots
// =============================================================================

void EventPlotOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void EventPlotOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void EventPlotOpenGLWidget::onTooltipTimer()
{
    if (!_tooltips_enabled || !_pending_tooltip_pos) {
        return;
    }

    auto hit = findEventNear(*_pending_tooltip_pos);
    if (hit) {
        QPointF world = screenToWorld(*_pending_tooltip_pos);
        QString tooltip = QString("Trial %1\nTime: %2 ms")
            .arg(hit->first + 1)
            .arg(world.x(), 0, 'f', 1);
        QToolTip::showText(mapToGlobal(*_pending_tooltip_pos), tooltip, this);
    }

    _pending_tooltip_pos.reset();
}

// =============================================================================
// Private Methods
// =============================================================================

void EventPlotOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        _scene_renderer.clearScene();
        return;
    }

    // Gather trial-aligned data
    GatherResult<DigitalEventSeries> gathered = gatherTrialData();

    if (gathered.empty()) {
        _scene_renderer.clearScene();
        return;
    }

    // Build layout request
    size_t num_trials = gathered.size();
    CorePlotting::LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;

    for (size_t i = 0; i < num_trials; ++i) {
        std::string key = "trial_" + std::to_string(i);
        // SeriesInfo constructor: (id, type, is_stackable)
        layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
    }

    // Compute layout using RowLayoutStrategy
    CorePlotting::LayoutResponse layout_response = _layout_strategy.compute(layout_request);

    // Build scene with SceneBuilder
    BoundingBox bounds{
        static_cast<float>(_cached_view_state.x_min),
        static_cast<float>(_cached_view_state.x_max),
        -1.0f,
        1.0f
    };

    CorePlotting::SceneBuilder builder;
    builder.setBounds(bounds);

    // Get time frame from SOURCE series (the spikes/events being plotted)
    // This is required for correct index→time conversion when source and alignment
    // series may have different sampling rates (e.g., 30kHz spikes, 500Hz events)
    auto event_names = _state->getPlotEventNames();
    if (event_names.empty()) {
        _scene_renderer.clearScene();
        return;
    }

    auto const source_options = _state->getPlotEventOptions(event_names.front());
    if (!source_options || source_options->event_key.empty()) {
        _scene_renderer.clearScene();
        return;
    }

    auto source_series = _data_manager->getData<DigitalEventSeries>(source_options->event_key);
    if (!source_series) {
        _scene_renderer.clearScene();
        return;
    }

    auto time_frame = source_series->getTimeFrame();
    if (!time_frame) {
        _scene_renderer.clearScene();
        return;
    }

    // Map each trial's events
    CorePlotting::GlyphStyle style;
    // Default glyph size - can be overridden per-series via EventPlotOptions
    style.size = 3.0f;
    style.color = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f); // Blue

    for (size_t trial = 0; trial < num_trials; ++trial) {
        auto const & trial_view = gathered[trial];
        if (!trial_view) continue;

        std::string key = "trial_" + std::to_string(trial);
        auto const * trial_layout = layout_response.findLayout(key);
        if (!trial_layout) continue;

        // Use alignmentTimeAt() which returns the proper alignment point:
        // - For EventExpanderAdapter: the event time (center of window)
        // - For IntervalWithAlignmentAdapter: start, end, or center based on setting
        // - For basic gather: interval.start as fallback
        auto reference_time = TimeFrameIndex(gathered.alignmentTimeAt(trial));

        // Use RasterMapper to generate mapped elements
        auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
            *trial_view,
            *trial_layout,
            *time_frame,
            reference_time,
            static_cast<int>(-_cached_view_state.x_min),
            static_cast<int>(_cached_view_state.x_max)
        );

        // Convert range to vector for builder
        std::vector<CorePlotting::MappedElement> elements;
        for (auto const & elem : mapped) {
            elements.push_back(elem);
        }

        builder.addGlyphs(key, std::move(elements), style);
    }

    // Build and upload scene
    _scene = builder.build();
    _scene_renderer.uploadScene(_scene);
}

void EventPlotOpenGLWidget::updateMatrices()
{
    // Calculate view bounds from view state
    float x_range = static_cast<float>(_cached_view_state.x_max - _cached_view_state.x_min);
    float x_center = static_cast<float>(_cached_view_state.x_min + _cached_view_state.x_max) / 2.0f;

    // Apply zoom
    float zoomed_x_range = x_range / static_cast<float>(_cached_view_state.x_zoom);
    float zoomed_y_range = 2.0f / static_cast<float>(_cached_view_state.y_zoom);

    // Apply pan
    float pan_x = static_cast<float>(_cached_view_state.x_pan);
    float pan_y = static_cast<float>(_cached_view_state.y_pan);

    // Calculate final bounds
    float left = x_center - zoomed_x_range / 2.0f + pan_x;
    float right = x_center + zoomed_x_range / 2.0f + pan_x;
    float bottom = -zoomed_y_range / 2.0f + pan_y;
    float top = zoomed_y_range / 2.0f + pan_y;

    // Build orthographic projection
    _projection_matrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    _view_matrix = glm::mat4(1.0f);
}

QPointF EventPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
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

QPoint EventPlotOpenGLWidget::worldToScreen(float world_x, float world_y) const
{
    glm::vec4 world(world_x, world_y, 0.0f, 1.0f);
    glm::vec4 ndc = _projection_matrix * world;

    int screen_x = static_cast<int>((ndc.x + 1.0f) * 0.5f * _widget_width);
    int screen_y = static_cast<int>((1.0f - ndc.y) * 0.5f * _widget_height);

    return QPoint(screen_x, screen_y);
}

void EventPlotOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) return;

    // Convert pixel delta to world delta
    float world_per_pixel_x = (_cached_view_state.x_max - _cached_view_state.x_min) /
                              (_widget_width * _cached_view_state.x_zoom);
    float world_per_pixel_y = 2.0f / (_widget_height * _cached_view_state.y_zoom);

    float new_pan_x = _cached_view_state.x_pan - delta_x * world_per_pixel_x;
    float new_pan_y = _cached_view_state.y_pan + delta_y * world_per_pixel_y;

    _state->setPan(new_pan_x, new_pan_y);
}

void EventPlotOpenGLWidget::handleZoom(float delta, bool y_only)
{
    if (!_state) return;

    float factor = std::pow(1.1f, delta);

    if (y_only) {
        _state->setYZoom(_cached_view_state.y_zoom * factor);
    } else {
        _state->setXZoom(_cached_view_state.x_zoom * factor);
        _state->setYZoom(_cached_view_state.y_zoom * factor);
    }
}

std::optional<std::pair<int, int>> EventPlotOpenGLWidget::findEventNear(
    QPoint const & screen_pos, float tolerance_pixels) const
{
    // TODO: Implement using CorePlotting::SceneHitTester
    // For now, return empty
    return std::nullopt;
}

void EventPlotOpenGLWidget::renderCenterLine()
{
    // TODO: Render vertical line at t=0 using basic OpenGL
    // This will be implemented when we have the axes shader
}

void EventPlotOpenGLWidget::renderAxes()
{
    // TODO: Render X and Y axis labels
    // This will be implemented in Phase 2
}

GatherResult<DigitalEventSeries> EventPlotOpenGLWidget::gatherTrialData() const
{
    if (!_data_manager || !_state) {
        return GatherResult<DigitalEventSeries>{};
    }

    // Get the first event series key from the plot events
    // Note: In a more complete implementation, we'd gather multiple series
    auto event_names = _state->getPlotEventNames();
    if (event_names.empty()) {
        return GatherResult<DigitalEventSeries>{};
    }

    // Get the first event's options
    auto const event_options = _state->getPlotEventOptions(event_names.front());
    if (!event_options || event_options->event_key.empty()) {
        return GatherResult<DigitalEventSeries>{};
    }

    // Get alignment state
    auto alignment_state = _state->alignmentState();
    if (!alignment_state) {
        return GatherResult<DigitalEventSeries>{};
    }

    // Use the new PlotAlignmentGather API which handles:
    // 1. DigitalEventSeries alignment with window expansion
    // 2. DigitalIntervalSeries alignment with start/end selection
    return WhiskerToolbox::Plots::createAlignedGatherResult<DigitalEventSeries>(
        _data_manager,
        event_options->event_key,
        alignment_state->data());
}
