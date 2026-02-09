#include "LinePlotOpenGLWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/GatherResult.hpp"
#include "Plots/Common/PlotAlignmentGather.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "CorePlotting/LineBatch/LineBatchBuilder.hpp"
#include "CorePlotting/LineBatch/CpuLineBatchIntersector.hpp"
#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "PlottingOpenGL/LineBatch/ComputeShaderIntersector.hpp"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QString>
#include <QSurfaceFormat>
#include <QWheelEvent>

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

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
    _line_renderer.cleanup();
    _line_store.cleanup();
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

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    // Initialize scene renderer (axes, grids — future use)
    if (!_scene_renderer.initialize()) {
        qWarning() << "LinePlotOpenGLWidget: Failed to initialize SceneRenderer";
    }

    // Initialize batch line store and renderer
    if (!_line_store.initialize()) {
        qWarning() << "LinePlotOpenGLWidget: Failed to initialize BatchLineStore";
    }
    if (!_line_renderer.initialize()) {
        qWarning() << "LinePlotOpenGLWidget: Failed to initialize BatchLineRenderer";
    }

    // Set visible colors for line states
    _line_renderer.setGlobalColor(glm::vec4{0.3f, 0.5f, 1.0f, 0.6f});   // Semi-transparent blue for normal lines
    _line_renderer.setSelectedColor(glm::vec4{1.0f, 0.2f, 0.2f, 1.0f}); // Bright red for selected lines
    _line_renderer.setHoverColor(glm::vec4{1.0f, 1.0f, 0.0f, 1.0f});    // Yellow for hover
    _line_renderer.setLineWidth(1.5f);

    // Pick intersector: GPU compute if GL 4.3+, CPU fallback otherwise
    bool const has_compute = [&] {
        auto * ctx = QOpenGLContext::currentContext();
        if (!ctx) return false;
        auto sf = ctx->format();
        return (sf.majorVersion() > 4) ||
               (sf.majorVersion() == 4 && sf.minorVersion() >= 3);
    }();

    if (has_compute) {
        auto gpu = std::make_unique<PlottingOpenGL::ComputeShaderIntersector>(_line_store);
        if (gpu->initialize()) {
            _intersector = std::move(gpu);
            qDebug() << "LinePlotOpenGLWidget: Using GPU compute shader intersector";
        }
    }
    if (!_intersector) {
        _intersector = std::make_unique<CorePlotting::CpuLineBatchIntersector>();
        qDebug() << "LinePlotOpenGLWidget: Using CPU intersector fallback";
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

    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    // Render scene elements (axes, grids — future)
    _scene_renderer.render(_view_matrix, _projection_matrix);

    // Render batch lines (trial data)
    _line_renderer.render(_view_matrix, _projection_matrix);

    // Render selection preview line if actively selecting
    if (_is_selecting) {
        // Disable depth test so the preview line draws on top of all lines
        glDisable(GL_DEPTH_TEST);
        auto preview = buildSelectionPreview();
        _scene_renderer.renderPreview(preview, _widget_width, _widget_height);
        glEnable(GL_DEPTH_TEST);
    }
}

void LinePlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);

    glViewport(0, 0, _widget_width, _widget_height);
    _line_renderer.setViewportSize(
        glm::vec2{static_cast<float>(_widget_width), static_cast<float>(_widget_height)});
    updateMatrices();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void LinePlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        // Ctrl+Click starts line selection (Shift+Ctrl = deselect mode)
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            bool const remove = event->modifiers().testFlag(Qt::ShiftModifier);
            startSelection(event->pos(), remove);
            event->accept();
            return;
        }

        // Don't start pan if we're in selection mode (shouldn't happen, but guard)
        if (_is_selecting) {
            event->accept();
            return;
        }

        // Normal click starts pan tracking
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void LinePlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() & Qt::LeftButton) {
        // Selection drag takes priority
        if (_is_selecting) {
            updateSelection(event->pos());
            event->accept();
            return;
        }

        // Pan drag detection
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
        if (_is_selecting) {
            completeSelection();
            event->accept();
            return;
        }

        _is_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    event->accept();
}

void LinePlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        // Don't navigate while in selection mode
        if (_is_selecting) {
            event->accept();
            return;
        }

        QPointF world = screenToWorld(event->pos());

        // world.x() is relative time (t=0 is the alignment point).
        // Convert to absolute time using the first trial's alignment time.
        // (All trials are overlaid, so we can't determine which trial was clicked.)
        if (!_cached_alignment_times.empty()) {
            int64_t alignment_time = _cached_alignment_times.front();
            int64_t absolute_time = alignment_time + static_cast<int64_t>(world.x());
            emit plotDoubleClicked(absolute_time, QString::fromStdString(_cached_series_key));
        }
    }
    event->accept();
}

void LinePlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    // Don't zoom while selecting
    if (_is_selecting) {
        event->accept();
        return;
    }

    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void LinePlotOpenGLWidget::keyReleaseEvent(QKeyEvent * event)
{
    // If Ctrl is released during selection drag, cancel the selection
    if (event->key() == Qt::Key_Control && _is_selecting) {
        cancelSelection();
        event->accept();
        return;
    }
    QOpenGLWidget::keyReleaseEvent(event);
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
        _line_renderer.clearData();
        _cached_alignment_times.clear();
        _cached_series_key.clear();
        return;
    }

    GatherResult<AnalogTimeSeries> gathered = gatherTrialData();

    if (gathered.empty()) {
        _scene_renderer.clearScene();
        _line_renderer.clearData();
        _cached_alignment_times.clear();
        _cached_series_key.clear();
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
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();

    for (size_t trial = 0; trial < num_trials; ++trial) {
        auto const & trial_view = gathered[trial];
        if (!trial_view || trial_view->getNumSamples() == 0) {
            continue;
        }

        int64_t alignment_time = gathered.alignmentTimeAt(trial);

        auto all_samples = trial_view->view();
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
            continue;
        }

        double rel_start = static_cast<double>(first_time.getValue() - alignment_time);
        double rel_end = static_cast<double>(last_time.getValue() - alignment_time);
        x_min = std::min(x_min, rel_start);
        x_max = std::max(x_max, rel_end);
    }

    // =========================================================================
    // Apply bounds to state
    // =========================================================================
    if (y_min < y_max) {
        float const y_margin = (y_max - y_min) * 0.05f;
        _state->setYBounds(
            static_cast<double>(y_min - y_margin),
            static_cast<double>(y_max + y_margin));
    }

    if (x_min < x_max) {
        _state->setXBounds(x_min, x_max);
    }

    _cached_view_state = _state->viewState();
    updateMatrices();

    // =========================================================================
    // Build LineBatchData from gathered trial data and upload to GPU
    // =========================================================================
    std::vector<std::int64_t> alignment_times;
    alignment_times.reserve(num_trials);
    for (size_t trial = 0; trial < num_trials; ++trial) {
        alignment_times.push_back(gathered.alignmentTimeAt(trial));
    }

    // Cache alignment times for relative→absolute time conversion on double-click
    _cached_alignment_times = alignment_times;

    // Cache the series key for TimeFrame resolution on double-click
    {
        auto series_names = _state->getPlotSeriesNames();
        if (!series_names.empty()) {
            auto opts = _state->getPlotSeriesOptions(series_names.front());
            if (opts) {
                _cached_series_key = opts->series_key;
            }
        }
    }

    auto batch = CorePlotting::buildLineBatchFromGatherResult(gathered, alignment_times);

    // Restore selection mask from previous selection (if trials still match)
    if (!_selected_trial_indices.empty()) {
        std::unordered_set<std::uint32_t> selected_set(
            _selected_trial_indices.begin(), _selected_trial_indices.end());
        for (std::uint32_t i = 0; i < batch.numLines(); ++i) {
            if (selected_set.contains(batch.lines[i].trial_index)) {
                batch.selection_mask[i] = 1;
            }
        }
    }

    _line_store.upload(batch);
    _line_renderer.syncFromStore();

    _scene_renderer.clearScene();
}

void LinePlotOpenGLWidget::updateMatrices()
{
    _projection_matrix =
        WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
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

// =============================================================================
// Selection
// =============================================================================

void LinePlotOpenGLWidget::clearSelection()
{
    _selected_trial_indices.clear();

    // Clear selection mask on the GPU store
    auto const & cpu = _line_store.cpuData();
    if (!cpu.empty()) {
        std::vector<std::uint32_t> mask(cpu.numLines(), 0);
        _line_store.updateSelectionMask(mask);
        _line_renderer.syncFromStore();
    }

    emit trialsSelected(_selected_trial_indices);
    update();
}

glm::vec2 LinePlotOpenGLWidget::screenToNDC(QPoint const & screen_pos) const
{
    float const ndc_x = (2.0f * screen_pos.x() / _widget_width) - 1.0f;
    float const ndc_y = 1.0f - (2.0f * screen_pos.y() / _widget_height);
    return glm::vec2{ndc_x, ndc_y};
}

void LinePlotOpenGLWidget::startSelection(QPoint const & screen_pos, bool remove_mode)
{
    _is_selecting = true;
    _selection_remove_mode = remove_mode;
    _selection_start_ndc = screenToNDC(screen_pos);
    _selection_end_ndc = _selection_start_ndc;
    _selection_start_screen = screen_pos;
    _selection_end_screen = screen_pos;
    setCursor(Qt::CrossCursor);
    update();
}

void LinePlotOpenGLWidget::updateSelection(QPoint const & screen_pos)
{
    _selection_end_ndc = screenToNDC(screen_pos);
    _selection_end_screen = screen_pos;
    update();
}

void LinePlotOpenGLWidget::completeSelection()
{
    _is_selecting = false;
    setCursor(Qt::ArrowCursor);

    if (!_intersector || _line_store.cpuData().empty()) {
        return;
    }

    // Build intersection query in NDC space
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = _selection_start_ndc;
    query.end_ndc = _selection_end_ndc;
    query.tolerance = 0.02f;
    query.mvp = _projection_matrix * _view_matrix;

    auto result = _intersector->intersect(_line_store.cpuData(), query);

    applyIntersectionResults(result.intersected_line_indices, _selection_remove_mode);
    update();
}

void LinePlotOpenGLWidget::applyIntersectionResults(
    std::vector<CorePlotting::LineBatchIndex> const & hit_indices,
    bool remove)
{
    auto const & cpu = _line_store.cpuData();

    if (remove) {
        // Remove mode: remove intersected trials from current selection
        std::unordered_set<std::uint32_t> to_remove;
        for (auto idx : hit_indices) {
            if (idx < cpu.numLines()) {
                to_remove.insert(cpu.lines[idx].trial_index);
            }
        }
        std::vector<std::uint32_t> filtered;
        filtered.reserve(_selected_trial_indices.size());
        for (auto ti : _selected_trial_indices) {
            if (!to_remove.contains(ti)) {
                filtered.push_back(ti);
            }
        }
        _selected_trial_indices = std::move(filtered);
    } else {
        // Replace mode: set selection to exactly the intersected trials
        _selected_trial_indices.clear();
        _selected_trial_indices.reserve(hit_indices.size());
        for (auto idx : hit_indices) {
            if (idx < cpu.numLines()) {
                _selected_trial_indices.push_back(cpu.lines[idx].trial_index);
            }
        }
    }

    // Update selection mask on the store
    std::vector<std::uint32_t> mask(cpu.numLines(), 0);
    std::unordered_set<std::uint32_t> selected_set(
        _selected_trial_indices.begin(), _selected_trial_indices.end());
    for (std::uint32_t i = 0; i < cpu.numLines(); ++i) {
        if (selected_set.contains(cpu.lines[i].trial_index)) {
            mask[i] = 1;
        }
    }
    _line_store.updateSelectionMask(mask);
    _line_renderer.syncFromStore();

    emit trialsSelected(_selected_trial_indices);
}

void LinePlotOpenGLWidget::cancelSelection()
{
    _is_selecting = false;
    setCursor(Qt::ArrowCursor);
    update();
}

CorePlotting::Interaction::GlyphPreview LinePlotOpenGLWidget::buildSelectionPreview() const
{
    CorePlotting::Interaction::GlyphPreview preview;
    preview.type = CorePlotting::Interaction::GlyphPreview::Type::Line;

    // PreviewRenderer expects canvas pixel coordinates (top-left origin)
    preview.line_start = glm::vec2{
        static_cast<float>(_selection_start_screen.x()),
        static_cast<float>(_selection_start_screen.y())};
    preview.line_end = glm::vec2{
        static_cast<float>(_selection_end_screen.x()),
        static_cast<float>(_selection_end_screen.y())};

    // Style: white stroke for normal selection, red for remove mode
    if (_selection_remove_mode) {
        preview.stroke_color = glm::vec4{1.0f, 0.3f, 0.3f, 0.9f};
    } else {
        preview.stroke_color = glm::vec4{1.0f, 1.0f, 1.0f, 0.9f};
    }
    preview.stroke_width = 2.0f;

    return preview;
}
