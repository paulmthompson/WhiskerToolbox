#include "EventPlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "DataManager/utils/color.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "CorePlotting/Mappers/RasterMapper.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
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
#include <limits>
#include <numeric>

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
        connect(_state.get(), &EventPlotState::backgroundColorChanged,
                this, [this]() {
                    updateBackgroundColor();
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

CorePlotting::ViewStateData const & EventPlotOpenGLWidget::viewState() const
{
    return _cached_view_state;
}

void EventPlotOpenGLWidget::resetView()
{
    if (_state) {
        _state->setXZoom(1.0);
        _state->setYZoom(1.0);
        _state->setPan(0.0, 0.0);
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

    // Set clear color from state (default white)
    updateBackgroundColor();

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
    // Update background color before clearing (in case it changed)
    updateBackgroundColor();
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
        _is_panning = false;  // Don't start panning yet - wait for drag detection
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
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

    // Check if we should start panning (drag detection)
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

    // Emit world position
    QPointF world = screenToWorld(event->pos());
    emit mouseWorldMoved(static_cast<float>(world.x()), static_cast<float>(world.y()));

    event->accept();
}

void EventPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        } else {
            // This was a click (not a drag) - try to select an event
            handleClickSelection(event->pos());
        }
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
            int trial_index = hit->first;
            float relative_time = static_cast<float>(world.x());
            
            // Convert relative time to absolute TimeFrameIndex
            // The alignment time is the absolute time of t=0 for this trial
            if (trial_index >= 0 && 
                static_cast<size_t>(trial_index) < _cached_alignment_times.size()) {
                int64_t alignment_time = _cached_alignment_times[trial_index];
                int64_t absolute_time = alignment_time + static_cast<int64_t>(relative_time);
                
                // Get series key for the signal
                QString series_key;
                if (_state) {
                    auto event_names = _state->getPlotEventNames();
                    if (!event_names.empty()) {
                        auto options = _state->getPlotEventOptions(event_names.front());
                        if (options) {
                            series_key = QString::fromStdString(options->event_key);
                        }
                    }
                }
                
                emit eventDoubleClicked(absolute_time, series_key);
            }
        }
    }
    event->accept();
}

void EventPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float delta = event->angleDelta().y() / 120.0f;
    
    // Zoom mode based on modifiers:
    // - Default: X-axis only (time-focused exploration)
    // - Shift: Y-axis only (trial-focused exploration)
    // - Ctrl: Both axes (uniform zoom)
    bool const shift_pressed = event->modifiers() & Qt::ShiftModifier;
    bool const ctrl_pressed = event->modifiers() & Qt::ControlModifier;
    
    handleZoom(delta, shift_pressed, ctrl_pressed);
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
        _cached_alignment_times.clear();
        _scene_renderer.clearScene();
        return;
    }

    // Gather trial-aligned data
    GatherResult<DigitalEventSeries> gathered = gatherTrialData();

    if (gathered.empty()) {
        _cached_alignment_times.clear();
        _scene_renderer.clearScene();
        return;
    }

    // Apply trial sorting based on state
    auto sorting_mode = _state->getSortingMode();
    if (sorting_mode != TrialSortMode::TrialIndex) {
        gathered = applySorting(gathered, sorting_mode);
    }

    // Cache alignment times for relative→absolute time conversion during interaction
    // Must happen AFTER sorting so indices match the displayed trial order
    size_t num_trials = gathered.size();
    _cached_alignment_times.clear();
    _cached_alignment_times.reserve(num_trials);
    for (size_t i = 0; i < num_trials; ++i) {
        _cached_alignment_times.push_back(gathered.alignmentTimeAt(i));
    }

    // Build layout request
    CorePlotting::LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;

    for (size_t i = 0; i < num_trials; ++i) {
        std::string key = "trial_" + std::to_string(i);
        // SeriesInfo constructor: (id, type, is_stackable)
        layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
    }

    // Compute layout using RowLayoutStrategy and cache for hit testing
    _layout_response = _layout_strategy.compute(layout_request);

    // Build scene with SceneBuilder
    // BoundingBox constructor: (min_x, min_y, max_x, max_y)
    BoundingBox bounds{
        static_cast<float>(_cached_view_state.x_min),  // min_x (time start)
        -1.0f,                                          // min_y (viewport bottom)
        static_cast<float>(_cached_view_state.x_max),  // max_x (time end)
        1.0f                                            // max_y (viewport top)
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
    // Get color from EventPlotOptions (default black)
    CorePlotting::GlyphStyle style;
    // Default glyph size - can be overridden per-series via EventPlotOptions
    style.size = 3.0f;
    
    // Convert hex color to glm::vec4 (default black if not set)
    int r, g, b;
    hexToRGB(source_options->hex_color.empty() ? "#000000" : source_options->hex_color, r, g, b);
    style.color = glm::vec4(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        1.0f);

    for (size_t trial = 0; trial < num_trials; ++trial) {
        auto const & trial_view = gathered[trial];
        if (!trial_view) continue;

        std::string key = "trial_" + std::to_string(trial);
        auto const * trial_layout = _layout_response.findLayout(key);
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

    // Emit trial count signal for vertical axis update
    emit trialCountChanged(num_trials);
}

void EventPlotOpenGLWidget::updateMatrices()
{
    _projection_matrix =
        WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);
}

QPointF EventPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(
        _projection_matrix, _widget_width, _widget_height, screen_pos);
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

void EventPlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }

    WhiskerToolbox::Plots::handleZoom(
        *_state, _cached_view_state, delta, y_only, both_axes);
}

std::optional<std::pair<int, int>> EventPlotOpenGLWidget::findEventNear(
    QPoint const & screen_pos, float tolerance_pixels) const
{
    // Convert screen position to world coordinates
    QPointF world = screenToWorld(screen_pos);
    
    // Configure hit tester with pixel tolerance converted to world units
    // X tolerance: convert pixels to time units
    float world_per_pixel_x = (_cached_view_state.x_max - _cached_view_state.x_min) /
                              (_widget_width * _cached_view_state.x_zoom);
    float world_tolerance = tolerance_pixels * world_per_pixel_x;
    
    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;
    
    CorePlotting::SceneHitTester tester(config);
    
    // Use queryQuadTree for discrete elements (events)
    auto result = tester.queryQuadTree(
        static_cast<float>(world.x()),
        static_cast<float>(world.y()),
        _scene);
    
    if (result.hasHit() && result.hit_type == CorePlotting::HitType::DigitalEvent) {
        // Extract trial index from series_key (format: "trial_N")
        if (result.series_key.starts_with("trial_")) {
            int trial_index = std::stoi(result.series_key.substr(6));
            // The event index within the trial is not tracked by the hit tester
            // Return trial and a placeholder event index (0)
            return std::make_pair(trial_index, 0);
        }
    }
    
    return std::nullopt;
}

void EventPlotOpenGLWidget::handleClickSelection(QPoint const & screen_pos)
{
    // Convert screen position to world coordinates
    QPointF world = screenToWorld(screen_pos);
    
    // Configure hit tester with reasonable tolerance
    float world_per_pixel_x = (_cached_view_state.x_max - _cached_view_state.x_min) /
                              (_widget_width * _cached_view_state.x_zoom);
    float world_tolerance = 10.0f * world_per_pixel_x;  // 10 pixel tolerance
    
    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;
    
    CorePlotting::SceneHitTester tester(config);
    
    // Perform full hit test (includes QuadTree and other strategies)
    auto result = tester.hitTest(
        static_cast<float>(world.x()),
        static_cast<float>(world.y()),
        _scene,
        _layout_response);
    
    if (result.hasHit() && result.hit_type == CorePlotting::HitType::DigitalEvent) {
        // Extract trial index from series_key (format: "trial_N")
        if (result.series_key.starts_with("trial_")) {
            int trial_index = std::stoi(result.series_key.substr(6));
            
            // Get series key from state for the signal
            QString series_key;
            auto event_names = _state ? _state->getPlotEventNames() : std::vector<QString>{};
            if (!event_names.empty()) {
                auto options = _state->getPlotEventOptions(event_names.front());
                if (options) {
                    series_key = QString::fromStdString(options->event_key);
                }
            }
            
            // Emit selection signal with trial index and relative time
            emit eventSelected(trial_index, result.world_x, series_key);
        }
    }
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

void EventPlotOpenGLWidget::updateBackgroundColor()
{
    if (!_state) {
        // Default to white if no state
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    QString hex_color = _state->getBackgroundColor();
    int r, g, b;
    hexToRGB(hex_color.toStdString(), r, g, b);
    glClearColor(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        1.0f);
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

GatherResult<DigitalEventSeries> EventPlotOpenGLWidget::applySorting(
    GatherResult<DigitalEventSeries> const& gathered,
    TrialSortMode mode) const
{
    if (gathered.empty() || mode == TrialSortMode::TrialIndex) {
        return gathered;  // No sorting needed
    }

    size_t num_trials = gathered.size();
    std::vector<size_t> sort_indices(num_trials);
    std::iota(sort_indices.begin(), sort_indices.end(), 0);

    switch (mode) {
        case TrialSortMode::FirstEventLatency: {
            // Sort by latency to first positive event (ascending)
            // Events with time > alignment time (t=0) are "positive"
            // Trials with no positive events go to the end
            std::vector<double> latencies(num_trials);
            
            for (size_t i = 0; i < num_trials; ++i) {
                auto const& trial_view = gathered[i];
                int64_t alignment_time = gathered.alignmentTimeAt(i);
                
                double first_positive_latency = std::numeric_limits<double>::infinity();
                
                if (trial_view) {
                    for (auto const& event : trial_view->view()) {
                        int64_t event_time = event.time().getValue();
                        // Relative time (positive = after alignment)
                        double relative_time = static_cast<double>(event_time - alignment_time);
                        if (relative_time >= 0.0 && relative_time < first_positive_latency) {
                            first_positive_latency = relative_time;
                            break;  // Events are time-ordered, so first positive is the answer
                        }
                    }
                }
                latencies[i] = first_positive_latency;
            }
            
            // Sort by latency (ascending - smallest latency first, NaN/infinity last)
            std::sort(sort_indices.begin(), sort_indices.end(),
                [&latencies](size_t a, size_t b) {
                    // Handle infinity: put at end
                    if (std::isinf(latencies[a]) && !std::isinf(latencies[b])) return false;
                    if (!std::isinf(latencies[a]) && std::isinf(latencies[b])) return true;
                    return latencies[a] < latencies[b];
                });
            break;
        }
        
        case TrialSortMode::EventCount: {
            // Sort by total event count (descending - most events first)
            std::vector<size_t> counts(num_trials);
            
            for (size_t i = 0; i < num_trials; ++i) {
                auto const& trial_view = gathered[i];
                counts[i] = trial_view ? trial_view->size() : 0;
            }
            
            // Sort by count (descending - highest count first)
            std::sort(sort_indices.begin(), sort_indices.end(),
                [&counts](size_t a, size_t b) {
                    return counts[a] > counts[b];
                });
            break;
        }
        
        case TrialSortMode::TrialIndex:
            // Should not reach here due to early return above
            break;
    }

    // Apply reordering to the GatherResult
    return gathered.reorder(sort_indices);
}
