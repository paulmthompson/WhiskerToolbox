#include "EventPlotOpenGLWidget.hpp"

#include "Core/IntervalEdgeExtraction.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/DataTypes/GlyphStyleConversion.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/Mappers/RasterMapper.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreUtilities/color.hpp"
#include "DataManager/DataManager.hpp"
#include "GatherResult/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "PlottingSVG/SVGSceneRenderer.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QShowEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <numeric>
#include <utility>

namespace {

/// Parsed result from a series_trial_key like "spikes_trial_42"
struct SeriesTrialKey {
    std::string event_name;///< The event name prefix (e.g. "spikes")
    int trial_index{-1};   ///< The trial number (e.g. 42), or -1 on failure

    [[nodiscard]] bool isValid() const { return trial_index >= 0; }
};

/// Parse a series_trial_key of the form "{event_name}_trial_{N}".
/// Finds the *last* occurrence of "_trial_" to handle event names that
/// might themselves contain "_trial_" (unlikely but safe).
[[nodiscard]] SeriesTrialKey parseSeriesTrialKey(std::string const & key) {
    constexpr std::string_view delimiter = "_trial_";
    auto pos = key.rfind(delimiter);
    if (pos == std::string::npos) {
        return {};
    }
    std::string name = key.substr(0, pos);
    std::string const number_str = key.substr(pos + delimiter.size());
    try {
        int const trial_index = std::stoi(number_str);
        return {std::move(name), trial_index};
    } catch (...) {
        return {};
    }
}

}// anonymous namespace

EventPlotOpenGLWidget::EventPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent) {
    // Set widget attributes for OpenGL
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Request OpenGL 4.1 Core Profile
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);// Enable multisampling for smooth points
    setFormat(format);

    // Initialize tooltip manager
    _tooltip_mgr = std::make_unique<WhiskerToolbox::Plots::PlotTooltipManager>(this);

    // Hit test provider: reuses the same findEventNear / screenToWorld as click handling
    _tooltip_mgr->setHitTestProvider(
            [this](QPoint pos) -> std::optional<WhiskerToolbox::Plots::PlotTooltipHit> {
                auto hit = findEventNear(pos);
                if (!hit) {
                    return std::nullopt;
                }
                QPointF const world = screenToWorld(pos);
                WhiskerToolbox::Plots::PlotTooltipHit result;
                result.world_x = static_cast<float>(world.x());
                result.world_y = static_cast<float>(world.y());
                result.user_data = *hit;// std::pair<int, std::string>
                return result;
            });

    // Text provider: generates tooltip content from the hit result
    _tooltip_mgr->setTextProvider(
            [](WhiskerToolbox::Plots::PlotTooltipHit const & hit) -> QString {
                auto const & data =
                        std::any_cast<std::pair<int, std::string> const &>(hit.user_data);
                return QString("Trial %1\nTime: %2 ms")
                        .arg(data.first + 1)
                        .arg(hit.world_x, 0, 'f', 1);
            });
}

EventPlotOpenGLWidget::~EventPlotOpenGLWidget() {
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void EventPlotOpenGLWidget::setState(std::shared_ptr<EventPlotState> state) {
    // Disconnect old state signals
    if (_state) {
        _state->disconnect(this);
    }

    _state = std::move(state);

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
        connect(_state.get(), &EventPlotState::alignmentEventKeyChanged,
                this, [this](QString const & /* key */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &EventPlotState::intervalAlignmentTypeChanged,
                this, [this](IntervalAlignmentType /* type */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &EventPlotState::offsetChanged,
                this, [this](double /* offset */) {
                    _scene_dirty = true;
                    update();
                });

        // Initial sync
        _cached_view_state = _state->viewState();
        _scene_dirty = true;
    }
}

void EventPlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    _scene_dirty = true;
    update();
}

CorePlotting::ViewStateData const & EventPlotOpenGLWidget::viewState() const {
    return _cached_view_state;
}

void EventPlotOpenGLWidget::resetView() {
    if (_state) {
        _state->setXZoom(1.0);
        _state->setYZoom(1.0);
        _state->setPan(0.0, 0.0);
    }
}

void EventPlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    _tooltip_mgr->setEnabled(enabled);
}

QString EventPlotOpenGLWidget::exportToSVG() {
    if (_scene.glyph_batches.empty() && _scene.poly_line_batches.empty() &&
        _scene.rectangle_batches.empty()) {
        return {};
    }

    // Ensure the cached scene carries the current camera matrices.
    // rebuildScene() produces identity matrices; the OpenGL path passes
    // matrices directly to the shader, but SVGSceneRenderer reads them
    // from the scene struct.
    _scene.view_matrix = _view_matrix;
    _scene.projection_matrix = _projection_matrix;

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(_scene);
    renderer.setCanvasSize(_widget_width, _widget_height);

    if (_state) {
        renderer.setBackgroundColor(_state->getBackgroundColor().toStdString());
    }

    return QString::fromStdString(renderer.render());
}

EventPlotOpenGLWidget::RasterExportBundle EventPlotOpenGLWidget::collectRasterExportData() const {
    RasterExportBundle bundle;

    if (!_state || !_data_manager) {
        return bundle;
    }

    auto event_names = _state->getPlotEventNames();
    if (event_names.empty()) {
        return bundle;
    }

    for (auto const & event_name: event_names) {
        auto const opts = _state->getPlotEventOptions(event_name);
        if (!opts || opts->event_key.empty()) {
            continue;
        }

        std::shared_ptr<DigitalEventSeries> series;
        std::shared_ptr<TimeFrame const> tf;

        if (opts->interval_edge.has_value()) {
            auto interval_series = _data_manager->getData<DigitalIntervalSeries>(opts->event_key);
            if (!interval_series) {
                continue;
            }
            series = WhiskerToolbox::Plots::extractEdgeEvents(
                    interval_series, *opts->interval_edge);
            if (!series) {
                continue;
            }
            tf = series->getTimeFrame();
        } else {
            series = _data_manager->getData<DigitalEventSeries>(opts->event_key);
            if (!series) {
                continue;
            }
            tf = series->getTimeFrame();
        }

        if (!tf) {
            continue;
        }

        auto gathered = gatherTrialData(series, opts->event_key);
        if (gathered.empty()) {
            continue;
        }

        bundle.owned.push_back({opts->event_key, std::move(gathered), tf});
    }

    // Build input references pointing into owned data
    bundle.inputs.reserve(bundle.owned.size());
    for (auto const & s: bundle.owned) {
        bundle.inputs.push_back({s.event_key, &s.gathered, s.time_frame.get()});
    }

    return bundle;
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void EventPlotOpenGLWidget::initializeGL() {
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

void EventPlotOpenGLWidget::paintGL() {
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

        // Sync current camera matrices into the cached scene so that
        // exportToSVG() (which reads from _scene) uses the right transform.
        _scene.view_matrix = _view_matrix;
        _scene.projection_matrix = _projection_matrix;
    }

    // Render the scene
    _scene_renderer.render(_view_matrix, _projection_matrix);

    // Render axes and center line
    renderCenterLine();
    renderAxes();
}

void EventPlotOpenGLWidget::resizeGL(int w, int h) {
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);

    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void EventPlotOpenGLWidget::showEvent(QShowEvent * event) {
    QOpenGLWidget::showEvent(event);
    update();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void EventPlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;// Don't start panning yet - wait for drag detection
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void EventPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    // Update tooltip manager
    _tooltip_mgr->onMouseMove(event->pos(), _is_panning);

    // Check if we should start panning (drag detection)
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

    // Emit world position
    QPointF const world = screenToWorld(event->pos());
    emit mouseWorldMoved(static_cast<float>(world.x()), static_cast<float>(world.y()));

    event->accept();
}

void EventPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
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

void EventPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        QPointF const world = screenToWorld(event->pos());

        // Find event near click position
        auto hit = findEventNear(event->pos());
        if (hit) {
            int const trial_index = hit->first;
            auto const relative_time = static_cast<float>(world.x());

            // Convert relative time to absolute time
            // The alignment time is the absolute time of t=0 for this trial
            if (trial_index >= 0 &&
                static_cast<size_t>(trial_index) < _cached_alignment_times.size()) {
                int64_t const alignment_time = _cached_alignment_times[trial_index];
                int64_t const absolute_time = alignment_time + static_cast<int64_t>(relative_time);

                // Resolve the data key from the hit's event name
                // hit->second encodes the event name via the scene key
                QString series_key;
                if (_state && !hit->second.empty()) {
                    auto options = _state->getPlotEventOptions(
                            QString::fromStdString(hit->second));
                    if (options) {
                        series_key = QString::fromStdString(options->event_key);
                    }
                }

                emit eventDoubleClicked(absolute_time, series_key);
            }
        }
    }
    event->accept();
}

void EventPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    float const delta = event->angleDelta().y() / 120.0f;

    // Zoom mode based on modifiers:
    // - Default: X-axis only (time-focused exploration)
    // - Shift: Y-axis only (trial-focused exploration)
    // - Ctrl: Both axes (uniform zoom)
    bool const shift_pressed = event->modifiers() & Qt::ShiftModifier;
    bool const ctrl_pressed = event->modifiers() & Qt::ControlModifier;

    handleZoom(delta, shift_pressed, ctrl_pressed);
    event->accept();
}

void EventPlotOpenGLWidget::leaveEvent(QEvent * event) {
    _tooltip_mgr->onLeave();
    QOpenGLWidget::leaveEvent(event);
}

// =============================================================================
// Slots
// =============================================================================

void EventPlotOpenGLWidget::onStateChanged() {
    _scene_dirty = true;
    update();
}

void EventPlotOpenGLWidget::onViewStateChanged() {
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

void EventPlotOpenGLWidget::rebuildScene() {
    if (!_state || !_data_manager) {
        _cached_alignment_times.clear();
        _scene_renderer.clearScene();
        return;
    }

    // Get all event series names
    auto event_names = _state->getPlotEventNames();
    if (event_names.empty()) {
        _cached_alignment_times.clear();
        _scene_renderer.clearScene();
        return;
    }

    // -------------------------------------------------------------------------
    // Step 1: Gather a GatherResult for EACH event series
    // All series share the same alignment event, so they produce the same
    // number of trials, but each contains different event data.
    // -------------------------------------------------------------------------
    struct SeriesData {
        QString name;
        std::string event_key;
        GatherResult<DigitalEventSeries> gathered;
        std::shared_ptr<TimeFrame const> time_frame;
    };
    std::vector<SeriesData> all_series;

    for (auto const & event_name: event_names) {
        auto const opts = _state->getPlotEventOptions(event_name);
        if (!opts || opts->event_key.empty()) {
            continue;
        }

        // Resolve the source data to a DigitalEventSeries.
        // For interval sources, extract edge events; for event sources, use directly.
        std::shared_ptr<DigitalEventSeries> series;
        std::shared_ptr<TimeFrame const> tf;

        if (opts->interval_edge.has_value()) {
            // Source is a DigitalIntervalSeries — extract edge events
            auto interval_series = _data_manager->getData<DigitalIntervalSeries>(opts->event_key);
            if (!interval_series) {
                continue;
            }
            series = WhiskerToolbox::Plots::extractEdgeEvents(
                    interval_series, *opts->interval_edge);
            if (!series) {
                continue;
            }
            tf = series->getTimeFrame();
        } else {
            // Source is a DigitalEventSeries — use directly
            series = _data_manager->getData<DigitalEventSeries>(opts->event_key);
            if (!series) {
                continue;
            }
            tf = series->getTimeFrame();
        }

        if (!tf) {
            continue;
        }

        auto gathered = gatherTrialData(series, opts->event_key);
        if (gathered.empty()) {
            continue;
        }

        all_series.push_back({event_name, opts->event_key, std::move(gathered), tf});
    }

    if (all_series.empty()) {
        _cached_alignment_times.clear();
        _scene_renderer.clearScene();
        return;
    }

    // -------------------------------------------------------------------------
    // Step 2: Determine trial count and compute sort order from FIRST series
    // The sort order is computed from the first series but applied to ALL series
    // so that trial rows remain consistent across overlaid event data.
    // -------------------------------------------------------------------------
    size_t const num_trials = all_series.front().gathered.size();

    auto sorting_mode = _state->getSortingMode();
    std::vector<size_t> const sort_indices =
            computeSortIndices(all_series.front().gathered, sorting_mode);

    // Apply sort order to every series' GatherResult
    if (!sort_indices.empty()) {
        for (auto & sd: all_series) {
            sd.gathered = sd.gathered.reorder(sort_indices);
        }
    }

    // Cache alignment times (from first series, post-sort) for interaction
    auto const & ref_gathered = all_series.front().gathered;
    _cached_alignment_times.clear();
    _cached_alignment_times.reserve(num_trials);
    for (size_t i = 0; i < num_trials; ++i) {
        _cached_alignment_times.push_back(ref_gathered.alignmentTimeAt(i));
    }

    // -------------------------------------------------------------------------
    // Step 3: Build layout (one row per trial, shared by all series)
    // -------------------------------------------------------------------------
    CorePlotting::LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;

    for (size_t i = 0; i < num_trials; ++i) {
        std::string const key = "trial_" + std::to_string(i);
        layout_request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, true);
    }

    _layout_response = _layout_strategy.compute(layout_request);

    // -------------------------------------------------------------------------
    // Step 4: Build scene — separate draw call per series with own glyph style
    // -------------------------------------------------------------------------
    BoundingBox const bounds{
            static_cast<float>(_cached_view_state.x_min),
            -1.0f,
            static_cast<float>(_cached_view_state.x_max),
            1.0f};

    CorePlotting::SceneBuilder builder;
    builder.setBounds(bounds);

    // Row height in world units — used to scale glyph sizes.
    // The viewport Y range is [-1, +1] (2.0 units) divided among num_trials rows.
    float const row_height = 2.0f / static_cast<float>(num_trials);

    for (auto const & sd: all_series) {
        // Per-series glyph style
        auto const gd = _state->getEventGlyphStyle(sd.name);
        CorePlotting::GlyphStyle style;
        style.glyph_type = CorePlotting::toRenderableGlyphType(gd.glyph_type);

        // Convert user-facing size to renderer units.
        // GlyphStyleData::size is a scale factor where 1.0 = full row height.
        //  - Circles (GL_POINTS): gl_PointSize is in screen pixels, so convert
        //    row-height fraction to pixels.
        //  - Instanced glyphs (ticks, squares, crosses): u_glyph_size is a
        //    world-space multiplier, so scale by row_height.
        bool const is_circle =
                (style.glyph_type == CorePlotting::RenderableGlyphBatch::GlyphType::Circle);
        if (is_circle) {
            style.size = gd.size * static_cast<float>(_widget_height) / static_cast<float>(num_trials);
        } else {
            style.size = gd.size * row_height;
        }

        style.color = CorePlotting::hexColorToVec4(gd.hex_color, gd.alpha);

        for (size_t trial = 0; trial < num_trials; ++trial) {
            auto const & trial_view = sd.gathered[trial];
            if (!trial_view) continue;

            std::string const layout_key = "trial_" + std::to_string(trial);
            auto const * trial_layout = _layout_response.findLayout(layout_key);
            if (!trial_layout) continue;

            auto ref_abs_time = static_cast<int>(sd.gathered.alignmentTimeAt(trial));

            auto mapped = CorePlotting::RasterMapper::mapEventsInWindow(
                    *trial_view,
                    *trial_layout,
                    *sd.time_frame,
                    ref_abs_time,
                    static_cast<int>(-_cached_view_state.x_min),
                    static_cast<int>(_cached_view_state.x_max));

            // Unique key per series+trial to avoid collisions in scene builder
            std::string const series_trial_key =
                    sd.name.toStdString() + "_trial_" + std::to_string(trial);
            std::vector<CorePlotting::MappedElement> elements;
            elements.reserve(trial_view->size());// Upper bound estimate
            for (auto const & elem: mapped) {
                elements.push_back(elem);
            }

            builder.addGlyphs(series_trial_key, std::move(elements), style);
        }
    }

    // Build and upload scene
    _scene = builder.build();
    _scene_renderer.uploadScene(_scene);

    emit trialCountChanged(num_trials);
}

void EventPlotOpenGLWidget::updateMatrices() {
    _projection_matrix =
            WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);

    // Keep the cached scene in sync so exportToSVG() uses current matrices.
    _scene.view_matrix = _view_matrix;
    _scene.projection_matrix = _projection_matrix;
}

QPointF EventPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const {
    return WhiskerToolbox::Plots::screenToWorld(
            _projection_matrix, _widget_width, _widget_height, screen_pos);
}

QPoint EventPlotOpenGLWidget::worldToScreen(float world_x, float world_y) const {
    glm::vec4 const world(world_x, world_y, 0.0f, 1.0f);
    glm::vec4 const ndc = _projection_matrix * world;

    int const screen_x = static_cast<int>((ndc.x + 1.0f) * 0.5f * _widget_width);
    int const screen_y = static_cast<int>((1.0f - ndc.y) * 0.5f * _widget_height);

    return {screen_x, screen_y};
}

void EventPlotOpenGLWidget::handlePanning(int delta_x, int delta_y) {
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handlePanning(
            *_state, _cached_view_state, delta_x, delta_y, _widget_width,
            _widget_height);
}

void EventPlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes) {
    if (!_state) {
        return;
    }

    WhiskerToolbox::Plots::handleZoom(
            *_state, _cached_view_state, delta, y_only, both_axes);
}

std::optional<std::pair<int, std::string>> EventPlotOpenGLWidget::findEventNear(
        QPoint const & screen_pos, float tolerance_pixels) const {
    // Convert screen position to world coordinates
    QPointF const world = screenToWorld(screen_pos);

    // Configure hit tester with pixel tolerance converted to world units
    // X tolerance: convert pixels to time units
    float const world_per_pixel_x = (_cached_view_state.x_max - _cached_view_state.x_min) /
                                    (_widget_width * _cached_view_state.x_zoom);
    float const world_tolerance = tolerance_pixels * world_per_pixel_x;

    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;

    CorePlotting::SceneHitTester const tester(config);

    // Use queryQuadTree for discrete elements (events)
    auto result = tester.queryQuadTree(
            static_cast<float>(world.x()),
            static_cast<float>(world.y()),
            _scene);

    if (result.hasHit() && result.hit_type == CorePlotting::HitType::DigitalEvent) {
        // Extract trial index from series_key (format: "{event_name}_trial_{N}")
        auto parsed = parseSeriesTrialKey(result.series_key);
        if (parsed.isValid()) {
            return std::make_pair(parsed.trial_index, parsed.event_name);
        }
    }

    return std::nullopt;
}

void EventPlotOpenGLWidget::handleClickSelection(QPoint const & screen_pos) {
    // Convert screen position to world coordinates
    QPointF const world = screenToWorld(screen_pos);

    // Configure hit tester with reasonable tolerance
    float const world_per_pixel_x = (_cached_view_state.x_max - _cached_view_state.x_min) /
                                    (_widget_width * _cached_view_state.x_zoom);
    float const world_tolerance = 10.0f * world_per_pixel_x;// 10 pixel tolerance

    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;

    CorePlotting::SceneHitTester const tester(config);

    // Perform full hit test (includes QuadTree and other strategies)
    auto result = tester.hitTest(
            static_cast<float>(world.x()),
            static_cast<float>(world.y()),
            _scene,
            _layout_response);

    if (result.hasHit() && result.hit_type == CorePlotting::HitType::DigitalEvent) {
        // Extract trial index and event name from series_key
        // (format: "{event_name}_trial_{N}")
        auto parsed = parseSeriesTrialKey(result.series_key);
        if (parsed.isValid()) {
            // Resolve the DataManager key from the event name
            QString series_key;
            if (_state) {
                auto options = _state->getPlotEventOptions(
                        QString::fromStdString(parsed.event_name));
                if (options) {
                    series_key = QString::fromStdString(options->event_key);
                }
            }

            // Emit selection signal with trial index and relative time
            emit eventSelected(parsed.trial_index, result.world_x, series_key);
        }
    }
}

void EventPlotOpenGLWidget::renderCenterLine() {
    // TODO: Render vertical line at t=0 using basic OpenGL
    // This will be implemented when we have the axes shader
}

void EventPlotOpenGLWidget::renderAxes() {
    // TODO: Render X and Y axis labels
    // This will be implemented in Phase 2
}

void EventPlotOpenGLWidget::updateBackgroundColor() {
    if (!_state) {
        // Default to white if no state
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    QString const hex_color = _state->getBackgroundColor();
    int r, g, b;
    hexToRGB(hex_color.toStdString(), r, g, b);
    glClearColor(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);
}

GatherResult<DigitalEventSeries> EventPlotOpenGLWidget::gatherTrialData(
        std::shared_ptr<DigitalEventSeries> const & source,
        std::string const & source_key) const {
    if (!_data_manager || !_state || !source || source_key.empty()) {
        return GatherResult<DigitalEventSeries>{};
    }

    auto alignment_state = _state->alignmentState();
    if (!alignment_state) {
        return GatherResult<DigitalEventSeries>{};
    }

    auto const & alignment_data = alignment_state->data();

    // Get alignment source (event or interval series for trial boundaries)
    auto alignment_source = WhiskerToolbox::Plots::getAlignmentSource(
            _data_manager, alignment_data.alignment_event_key);
    if (!alignment_source.isValid()) {
        return GatherResult<DigitalEventSeries>{};
    }

    // Use the low-level gather functions directly with the pre-resolved source.
    // This supports both DataManager-owned and derived (edge-extracted) series.
    if (alignment_source.is_event_series) {
        double const half_window = alignment_data.window_size / 2.0;
        return WhiskerToolbox::Plots::gatherWithEventAlignment<DigitalEventSeries>(
                source,
                alignment_source.event_series,
                half_window,
                half_window);
    } else {
        double const half_window = alignment_data.window_size / 2.0;
        auto align = WhiskerToolbox::Plots::toAlignmentPoint(
                alignment_data.interval_alignment_type);
        return WhiskerToolbox::Plots::gatherWithIntervalAlignment<DigitalEventSeries>(
                source,
                alignment_source.interval_series,
                align,
                half_window,
                half_window);
    }
}

std::vector<size_t> EventPlotOpenGLWidget::computeSortIndices(
        GatherResult<DigitalEventSeries> const & gathered,
        TrialSortMode mode) {
    if (gathered.empty() || mode == TrialSortMode::TrialIndex) {
        return {};// Empty = no reordering
    }

    size_t const num_trials = gathered.size();
    std::vector<size_t> sort_indices(num_trials);
    std::iota(sort_indices.begin(), sort_indices.end(), 0);

    switch (mode) {
        case TrialSortMode::FirstEventLatency: {
            // Sort by latency to first positive event (ascending)
            // Events with time > alignment time (t=0) are "positive"
            // Trials with no positive events go to the end
            std::vector<double> latencies(num_trials);

            for (size_t i = 0; i < num_trials; ++i) {
                auto const & trial_view = gathered[i];
                int64_t const alignment_time_abs = gathered.alignmentTimeAt(i);

                double first_positive_latency = std::numeric_limits<double>::infinity();

                if (trial_view) {
                    auto trial_tf = trial_view->getTimeFrame();
                    for (auto const & event: trial_view->view()) {
                        int64_t const event_time_abs = trial_tf
                                                               ? trial_tf->getTimeAtIndex(event.time())
                                                               : event.time().getValue();
                        // Relative time (positive = after alignment)
                        auto const relative_time = static_cast<double>(event_time_abs - alignment_time_abs);
                        if (relative_time >= 0.0 && relative_time < first_positive_latency) {
                            first_positive_latency = relative_time;
                            break;// Events are time-ordered, so first positive is the answer
                        }
                    }
                }
                latencies[i] = first_positive_latency;
            }

            // Sort by latency (ascending - smallest latency first, NaN/infinity last)
            // Use stable_sort to preserve original trial order among ties
            std::stable_sort(sort_indices.begin(), sort_indices.end(),
                             [&latencies](size_t a, size_t b) {
                                 // Handle infinity: put at end
                                 if (std::isinf(latencies[a]) && !std::isinf(latencies[b])) return false;
                                 if (!std::isinf(latencies[a]) && std::isinf(latencies[b])) return true;
                                 return latencies[a] < latencies[b];
                             });
            break;
        }

        case TrialSortMode::SecondEventLatency: {
            // Sort by latency to second positive event (ascending)
            // Useful when the first event is nearly identical across trials
            // and structure in the second event timing is of interest.
            // Trials with fewer than 2 positive events go to the end.
            std::vector<double> latencies(num_trials);

            for (size_t i = 0; i < num_trials; ++i) {
                auto const & trial_view = gathered[i];
                int64_t const alignment_time_abs = gathered.alignmentTimeAt(i);

                double second_positive_latency = std::numeric_limits<double>::infinity();
                int positive_count = 0;

                if (trial_view) {
                    auto trial_tf = trial_view->getTimeFrame();
                    for (auto const & event: trial_view->view()) {
                        int64_t const event_time_abs = trial_tf
                                                               ? trial_tf->getTimeAtIndex(event.time())
                                                               : event.time().getValue();
                        auto const relative_time = static_cast<double>(event_time_abs - alignment_time_abs);
                        if (relative_time >= 0.0) {
                            ++positive_count;
                            if (positive_count == 2) {
                                second_positive_latency = relative_time;
                                break;
                            }
                        }
                    }
                }
                latencies[i] = second_positive_latency;
            }

            std::stable_sort(sort_indices.begin(), sort_indices.end(),
                             [&latencies](size_t a, size_t b) {
                                 if (std::isinf(latencies[a]) && !std::isinf(latencies[b])) return false;
                                 if (!std::isinf(latencies[a]) && std::isinf(latencies[b])) return true;
                                 return latencies[a] < latencies[b];
                             });
            break;
        }

        case TrialSortMode::AlignmentInterval: {
            // Sort by the temporal gap between consecutive alignment events.
            // Trial 0 has no predecessor, so its interval is infinity (sorted last).
            // Short gaps first, long gaps last.
            std::vector<double> intervals(num_trials);
            intervals[0] = std::numeric_limits<double>::infinity();

            for (size_t i = 1; i < num_trials; ++i) {
                int64_t const curr = gathered.alignmentTimeAt(i);
                int64_t const prev = gathered.alignmentTimeAt(i - 1);
                intervals[i] = static_cast<double>(curr - prev);
            }

            std::stable_sort(sort_indices.begin(), sort_indices.end(),
                             [&intervals](size_t a, size_t b) {
                                 if (std::isinf(intervals[a]) && !std::isinf(intervals[b])) return false;
                                 if (!std::isinf(intervals[a]) && std::isinf(intervals[b])) return true;
                                 return intervals[a] < intervals[b];
                             });
            break;
        }

        case TrialSortMode::EventCount: {
            // Sort by total event count (descending - most events first)
            std::vector<size_t> counts(num_trials);

            for (size_t i = 0; i < num_trials; ++i) {
                auto const & trial_view = gathered[i];
                counts[i] = trial_view ? trial_view->size() : 0;
            }

            // Sort by count (descending - highest count first)
            // Use stable_sort to preserve original trial order among ties
            std::stable_sort(sort_indices.begin(), sort_indices.end(),
                             [&counts](size_t a, size_t b) {
                                 return counts[a] > counts[b];
                             });
            break;
        }

        case TrialSortMode::TrialIndex:
            // Should not reach here due to early return above
            break;
    }

    // Return the computed permutation
    return sort_indices;
}
