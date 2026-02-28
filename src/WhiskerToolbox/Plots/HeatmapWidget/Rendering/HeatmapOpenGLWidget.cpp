#include "HeatmapOpenGLWidget.hpp"

#include "CorePlotting/Colormaps/Colormap.hpp"
#include "CorePlotting/Mappers/HeatmapMapper.hpp"
#include "DataManager/DataManager.hpp"
#include "GatherResult/GatherResult.hpp"
#include "Plots/Common/EventRateEstimation/EventRateEstimation.hpp"
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
    _scene_renderer.render(_view_matrix, _projection_matrix);
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

    auto const & unit_keys = _state->unitKeys();
    if (unit_keys.empty()) {
        _scene_renderer.clearScene();
        emit unitCountChanged(0);
        return;
    }

    auto * alignment_state = _state->alignmentState();
    if (!alignment_state) {
        _scene_renderer.clearScene();
        return;
    }

    double const window_size = _state->getWindowSize();
    if (window_size <= 0.0) {
        _scene_renderer.clearScene();
        return;
    }

    // 1. Gather trial-aligned DigitalEventSeries data for all selected units
    auto contexts = WhiskerToolbox::Plots::createUnitGatherContexts(
        _data_manager, unit_keys, alignment_state->data());

    if (contexts.empty()) {
        _scene_renderer.clearScene();
        emit unitCountChanged(0);
        return;
    }

    // 2. Estimate firing rates for all units
    auto rate_estimates = WhiskerToolbox::Plots::estimateRates(
        contexts, window_size);

    if (rate_estimates.empty()) {
        _scene_renderer.clearScene();
        emit unitCountChanged(0);
        return;
    }

    // 3. Apply scaling in-place using the selected scaling mode
    auto const scaling = _state->scaling();
    // TODO: make time_units_per_second configurable if data uses non-ms units
    constexpr double time_units_per_second = 1000.0;
    for (auto & est : rate_estimates) {
        WhiskerToolbox::Plots::applyScaling(est, scaling, time_units_per_second);
    }

    // 4. Convert RateEstimate to CorePlotting::HeatmapRowData for the mapper
    //    times[] are bin centers; reconstruct left edges from sample_spacing.
    std::vector<CorePlotting::HeatmapRowData> rows;
    rows.reserve(rate_estimates.size());
    for (auto & est : rate_estimates) {
        double const spacing = est.metadata.sample_spacing;
        double const left_edge = est.times.empty()
            ? 0.0 : est.times.front() - spacing / 2.0;
        rows.push_back(CorePlotting::HeatmapRowData{
            .values = std::move(est.values),
            .bin_start = left_edge,
            .bin_width = spacing,
        });
    }

    // 5. Build the colored rectangle scene with appropriate colormap and range
    auto const & color_range_config = _state->colorRange();

    // Use Coolwarm for z-score, Inferno otherwise
    auto const colormap_preset =
        (scaling == WhiskerToolbox::Plots::ScalingMode::ZScore)
            ? CorePlotting::Colormaps::ColormapPreset::Coolwarm
            : CorePlotting::Colormaps::ColormapPreset::Inferno;
    auto colormap = CorePlotting::Colormaps::getColormap(colormap_preset);

    // Map state color range config to CorePlotting::HeatmapColorRange
    CorePlotting::HeatmapColorRange mapper_range;
    switch (color_range_config.mode) {
        case HeatmapColorRangeConfig::Mode::Auto:
            mapper_range.mode = CorePlotting::HeatmapColorRange::Mode::Auto;
            break;
        case HeatmapColorRangeConfig::Mode::Manual:
            mapper_range.mode = CorePlotting::HeatmapColorRange::Mode::Manual;
            mapper_range.vmin = static_cast<float>(color_range_config.vmin);
            mapper_range.vmax = static_cast<float>(color_range_config.vmax);
            break;
        case HeatmapColorRangeConfig::Mode::Symmetric:
            mapper_range.mode = CorePlotting::HeatmapColorRange::Mode::Symmetric;
            break;
    }

    auto scene = CorePlotting::HeatmapMapper::buildScene(
        rows, colormap, mapper_range);

    _scene_renderer.uploadScene(scene);

    // 6. Update Y-axis to reflect unit count
    auto const num_units = rows.size();
    if (num_units != _unit_count) {
        _unit_count = num_units;
        emit unitCountChanged(_unit_count);
    }
}

void HeatmapOpenGLWidget::updateMatrices()
{
    _projection_matrix =
        WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
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
    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y, _widget_width,
        _widget_height);
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
