#include "HeatmapOpenGLWidget.hpp"

#include "CorePlotting/Colormaps/Colormap.hpp"
#include "CorePlotting/Mappers/HeatmapMapper.hpp"
#include "DataManager/DataManager.hpp"
#include "EditorState/SelectionContext.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "Plots/HeatmapWidget/Core/HeatmapDataPipeline.hpp"
#include "PlottingSVG/SVGSceneRenderer.hpp"

#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QShowEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

HeatmapOpenGLWidget::HeatmapOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent) {
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    setFormat(format);

    setupTooltip();
}

HeatmapOpenGLWidget::~HeatmapOpenGLWidget() {
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void HeatmapOpenGLWidget::setState(std::shared_ptr<HeatmapState> state) {
    if (_state) {
        _state->disconnect(this);
    }

    _state = std::move(state);

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
        connect(_state.get(), &HeatmapState::alignmentEventKeyChanged,
                this, [this](QString const & /* key */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &HeatmapState::intervalAlignmentTypeChanged,
                this, [this](IntervalAlignmentType /* type */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &HeatmapState::offsetChanged,
                this, [this](double /* offset */) {
                    _scene_dirty = true;
                    update();
                });
        connect(_state.get(), &HeatmapState::windowSizeChanged,
                this, [this](double /* window_size */) {
                    _scene_dirty = true;
                    update();
                });

        _scene_dirty = true;
        updateMatrices();
    }
}

void HeatmapOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    _scene_dirty = true;
    update();
}

CorePlotting::ViewStateData const & HeatmapOpenGLWidget::viewState() const {
    return _cached_view_state;
}

void HeatmapOpenGLWidget::setSelectionContext(SelectionContext * selection_context) {
    _selection_context = selection_context;
}

QString HeatmapOpenGLWidget::exportToSVG() {
    if (_scene.rectangle_batches.empty()) {
        return {};
    }

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

HeatmapOpenGLWidget::HeatmapExportBundle HeatmapOpenGLWidget::collectHeatmapExportData() const {
    HeatmapExportBundle bundle;

    if (!_state || !_data_manager) {
        return bundle;
    }

    auto const & unit_keys = _state->unitKeys();
    if (unit_keys.empty()) {
        return bundle;
    }

    auto * alignment_state = _state->alignmentState();
    if (!alignment_state) {
        return bundle;
    }

    double const window_size = _state->getWindowSize();
    if (window_size <= 0.0) {
        return bundle;
    }

    WhiskerToolbox::Plots::HeatmapPipelineConfig config;
    config.window_size = window_size;
    config.scaling = _state->scaling();
    config.estimation_params = _state->estimationParams();
    config.time_units_per_second = 1000.0;

    auto pipeline_result = WhiskerToolbox::Plots::runHeatmapPipeline(
            _data_manager, unit_keys, alignment_state->data(), config);

    if (!pipeline_result.success || pipeline_result.rows.empty()) {
        return bundle;
    }

    bundle.unit_keys = unit_keys;
    auto const sort_mode = _state->sortMode();
    if (sort_mode != HeatmapSortMode::Manual) {
        auto sort_indices = WhiskerToolbox::Plots::computeSortOrder(
                pipeline_result, bundle.unit_keys, sort_mode, _state->sortAscending());
        WhiskerToolbox::Plots::applySortOrder(
                pipeline_result, bundle.unit_keys, sort_indices);
    }

    bundle.rows = std::move(pipeline_result.rows);
    return bundle;
}

void HeatmapOpenGLWidget::resetView() {
    if (_state) {
        _state->setXZoom(1.0);
        _state->setYZoom(1.0);
        _state->setPan(0.0, 0.0);
    }
}

// =============================================================================
// OpenGL Lifecycle
// =============================================================================

void HeatmapOpenGLWidget::initializeGL() {
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

void HeatmapOpenGLWidget::paintGL() {
    updateBackgroundColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!_opengl_initialized) {
        return;
    }
    if (_scene_dirty) {
        rebuildScene();
        // Note: rebuildScene() may re-trigger _scene_dirty via unitCountChanged
        // signal chain. Only clear the flag if no new state changes occurred.
        // If _scene_dirty was re-set during rebuild, an update() is already
        // scheduled and the next paintGL will handle it.
        _scene_dirty = false;
    }
    _scene_renderer.render(_view_matrix, _projection_matrix);
}

void HeatmapOpenGLWidget::resizeGL(int w, int h) {
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void HeatmapOpenGLWidget::showEvent(QShowEvent * event) {
    QOpenGLWidget::showEvent(event);
    // Schedule a repaint so the FBO content is composited after a tab switch.
    // Note: under WSLg the compositor may still delay the blit; a deferred
    // QTimer::singleShot(0, this, [this]{ repaint(); window()->update(); })
    // was confirmed to fix that, but is not needed on native Linux/Windows/macOS.
    update();
}

// =============================================================================
// Mouse Interaction
// =============================================================================

void HeatmapOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void HeatmapOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
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

    _tooltip_mgr->onMouseMove(event->pos(), _is_panning);
    event->accept();
}

void HeatmapOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void HeatmapOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        QPointF const world = screenToWorld(event->pos());
        emit plotDoubleClicked(static_cast<int64_t>(world.x()));
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void HeatmapOpenGLWidget::wheelEvent(QWheelEvent * event) {
    float const delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void HeatmapOpenGLWidget::leaveEvent(QEvent * event) {
    _tooltip_mgr->onLeave();
    QOpenGLWidget::leaveEvent(event);
}

void HeatmapOpenGLWidget::contextMenuEvent(QContextMenuEvent * event) {
    if (!_selection_context) {
        QOpenGLWidget::contextMenuEvent(event);
        return;
    }

    QPointF const world = screenToWorld(event->pos());
    int const unit_index = worldToUnitIndex(world);

    // Only show menu when hovering over a valid unit row
    if (unit_index < 0) {
        QOpenGLWidget::contextMenuEvent(event);
        return;
    }

    auto const & unit_key = _display_unit_keys[static_cast<size_t>(unit_index)];

    // Set data focus to the unit under the mouse so ContextActions can evaluate applicability
    SelectionSource const source{
            EditorLib::EditorInstanceId(QStringLiteral("heatmap_context_menu")),
            QStringLiteral("HeatmapWidget")};
    _selection_context->setDataFocus(
            SelectedDataKey(QString::fromStdString(unit_key)),
            QStringLiteral("DigitalEventSeries"),
            source);

    // Remove previously added dynamic ContextAction items
    for (auto * action: _dynamic_context_actions) {
        if (_context_menu) {
            _context_menu->removeAction(action);
        }
        action->deleteLater();
    }
    _dynamic_context_actions.clear();

    // Query applicable ContextActions and build the menu dynamically
    auto const applicable = _selection_context->applicableActions();
    if (applicable.empty()) {
        QOpenGLWidget::contextMenuEvent(event);
        return;
    }

    if (!_context_menu) {
        _context_menu = new QMenu(this);
    }
    _context_menu->clear();

    for (auto const * ctx_action: applicable) {
        auto * menu_action = _context_menu->addAction(ctx_action->display_name);
        auto const * action_ptr = ctx_action;
        connect(menu_action, &QAction::triggered, this, [this, action_ptr]() {
            action_ptr->execute(*_selection_context);
        });
        _dynamic_context_actions.push_back(menu_action);
    }

    _context_menu->popup(event->globalPos());
    event->accept();
}

// =============================================================================
// Slots
// =============================================================================

void HeatmapOpenGLWidget::onStateChanged() {
    _scene_dirty = true;
    update();
}

void HeatmapOpenGLWidget::onViewStateChanged() {
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

void HeatmapOpenGLWidget::rebuildScene() {
    _tooltip_mgr->hide();

    auto const clearCachedScene = [this]() {
        _scene = CorePlotting::RenderableScene{};
        _scene_renderer.clearScene();
    };

    if (!_state || !_data_manager) {
        clearCachedScene();
        return;
    }

    auto const & unit_keys = _state->unitKeys();
    if (unit_keys.empty()) {
        clearCachedScene();
        _display_unit_keys.clear();
        emit unitCountChanged(0);
        return;
    }

    auto * alignment_state = _state->alignmentState();
    if (!alignment_state) {
        clearCachedScene();
        return;
    }

    double const window_size = _state->getWindowSize();
    if (window_size <= 0.0) {
        clearCachedScene();
        return;
    }

    // Run the pure-data pipeline (gather → estimate → scale → convert)
    WhiskerToolbox::Plots::HeatmapPipelineConfig config;
    config.window_size = window_size;
    config.scaling = _state->scaling();
    config.estimation_params = _state->estimationParams();
    // TODO: make time_units_per_second configurable if data uses non-ms units
    config.time_units_per_second = 1000.0;

    auto pipeline_result = WhiskerToolbox::Plots::runHeatmapPipeline(
            _data_manager, unit_keys, alignment_state->data(), config);

    if (!pipeline_result.success || pipeline_result.rows.empty()) {
        clearCachedScene();
        _display_unit_keys.clear();
        emit unitCountChanged(0);
        return;
    }

    // Apply row sorting if a non-Manual sort mode is selected
    auto sorted_keys = std::vector<std::string>(unit_keys);
    auto const sort_mode = _state->sortMode();
    if (sort_mode != HeatmapSortMode::Manual) {
        auto sort_indices = WhiskerToolbox::Plots::computeSortOrder(
                pipeline_result, sorted_keys, sort_mode, _state->sortAscending());
        WhiskerToolbox::Plots::applySortOrder(
                pipeline_result, sorted_keys, sort_indices);
    }

    // Cache the display-order keys for tooltip lookup
    _display_unit_keys = sorted_keys;

    // Build the colored rectangle scene with appropriate colormap and range
    auto const & color_range_config = _state->colorRange();

    // Use colormap preset from state
    auto const colormap_preset = _state->colormapPreset();
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

    _scene = CorePlotting::HeatmapMapper::buildScene(
            pipeline_result.rows, colormap, mapper_range);

    _scene_renderer.uploadScene(_scene);

    // Update Y-axis to reflect unit count
    auto const num_units = pipeline_result.rows.size();
    if (num_units != _unit_count) {
        _unit_count = num_units;
        emit unitCountChanged(_unit_count);
    }
}

void HeatmapOpenGLWidget::updateMatrices() {
    _projection_matrix =
            WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);
}

QPointF HeatmapOpenGLWidget::screenToWorld(QPoint const & screen_pos) const {
    return WhiskerToolbox::Plots::screenToWorld(
            _projection_matrix, _widget_width, _widget_height, screen_pos);
}

void HeatmapOpenGLWidget::handlePanning(int delta_x, int delta_y) {
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handlePanning(
            *_state, _cached_view_state, delta_x, delta_y, _widget_width,
            _widget_height);
}

void HeatmapOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes) {
    if (!_state) {
        return;
    }

    WhiskerToolbox::Plots::handleZoom(
            *_state, _cached_view_state, delta, y_only, both_axes);
}

void HeatmapOpenGLWidget::updateBackgroundColor() {
    if (!_state) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    QString const hex_color = _state->getBackgroundColor();
    QColor const color(hex_color);
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

void HeatmapOpenGLWidget::setupTooltip() {
    _tooltip_mgr = std::make_unique<WhiskerToolbox::Plots::PlotTooltipManager>(this);

    _tooltip_mgr->setHitTestProvider(
            [this](QPoint pos) -> std::optional<WhiskerToolbox::Plots::PlotTooltipHit> {
                QPointF const world = screenToWorld(pos);
                int const unit_index = worldToUnitIndex(world);
                if (unit_index < 0) {
                    return std::nullopt;
                }
                WhiskerToolbox::Plots::PlotTooltipHit hit;
                hit.world_x = static_cast<float>(world.x());
                hit.world_y = static_cast<float>(world.y());
                hit.user_data = unit_index;
                return hit;
            });

    _tooltip_mgr->setTextProvider(
            [this](WhiskerToolbox::Plots::PlotTooltipHit const & hit) -> QString {
                auto const unit_index = std::any_cast<int>(hit.user_data);
                if (unit_index < 0 ||
                    static_cast<size_t>(unit_index) >= _display_unit_keys.size()) {
                    return {};
                }
                return QString::fromStdString(_display_unit_keys[static_cast<size_t>(unit_index)]);
            });
}

int HeatmapOpenGLWidget::worldToUnitIndex(QPointF const & world_pos) const {
    // Each heatmap row occupies one unit of Y space: row 0 → [0, 1), row 1 → [1, 2), etc.
    auto const index = static_cast<int>(std::floor(world_pos.y()));
    if (index < 0 || static_cast<size_t>(index) >= _display_unit_keys.size()) {
        return -1;
    }
    return index;
}
