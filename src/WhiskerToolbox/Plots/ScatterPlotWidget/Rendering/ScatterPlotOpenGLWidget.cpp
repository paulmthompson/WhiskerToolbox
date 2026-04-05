#include "ScatterPlotOpenGLWidget.hpp"

#include "Core/BuildScatterPoints.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Core/SourceCompatibility.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/Colormaps/Colormap.hpp"
#include "CorePlotting/DataTypes/GlyphStyleConversion.hpp"
#include "CorePlotting/FeatureColor/ApplyFeatureColors.hpp"
#include "CorePlotting/FeatureColor/FeatureColorMapping.hpp"
#include "CorePlotting/FeatureColor/FeatureColorRange.hpp"
#include "CorePlotting/FeatureColor/ResolveFeatureColors.hpp"
#include "CorePlotting/Interaction/HitTestResult.hpp"
#include "CorePlotting/Interaction/PolygonInteractionController.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Mappers/MappedElement.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"// RenderableScene
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CorePlotting/Selection/PolygonSelection.hpp"
#include "CoreUtilities/color.hpp"
#include "DataManager/DataManager.hpp"
#include "EditorState/ContextAction.hpp"
#include "EditorState/SelectionContext.hpp"
#include "GroupContextMenu/GroupContextMenuHandler.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "KeymapSystem/KeyActionAdapter.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "Plots/Common/TooltipManager/PlotTooltipManager.hpp"
#include "PlottingOpenGL/Renderers/PreviewRenderer.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "PlottingSVG/SVGSceneRenderer.hpp"
#include "Tensors/TensorData.hpp"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent) {

    _hit_tester = std::make_unique<CorePlotting::SceneHitTester>();
    _polygon_controller = std::make_unique<CorePlotting::Interaction::PolygonInteractionController>();
    _preview_renderer = std::make_unique<PlottingOpenGL::PreviewRenderer>();
    _scene = std::make_unique<CorePlotting::RenderableScene>();
    _scene_renderer = std::make_unique<PlottingOpenGL::SceneRenderer>();

    // Initialize tooltip manager
    _tooltip_mgr = std::make_unique<WhiskerToolbox::Plots::PlotTooltipManager>(this);

    _tooltip_mgr->setHitTestProvider(
            [this](QPoint pos) -> std::optional<WhiskerToolbox::Plots::PlotTooltipHit> {
                auto const hit_index = hitTestPointAt(pos);
                if (!hit_index.has_value()) {
                    return std::nullopt;
                }
                QPointF const world = screenToWorld(pos);
                WhiskerToolbox::Plots::PlotTooltipHit result;
                result.world_x = static_cast<float>(world.x());
                result.world_y = static_cast<float>(world.y());
                result.user_data = *hit_index;// std::size_t index into _scatter_data
                return result;
            });

    _tooltip_mgr->setTextProvider(
            [this](WhiskerToolbox::Plots::PlotTooltipHit const & hit) -> QString {
                auto const idx = std::any_cast<std::size_t>(hit.user_data);
                if (idx >= _scatter_data.size()) {
                    return {};
                }
                auto const tfi = _scatter_data.time_indices[idx];
                auto text = QString("X: %1\nY: %2\nIndex: %3")
                                    .arg(static_cast<double>(hit.world_x), 0, 'f', 3)
                                    .arg(static_cast<double>(hit.world_y), 0, 'f', 3)
                                    .arg(tfi.getValue());

                // Append feature color value when coloring is active
                if (_state && idx < _feature_values.size() && _feature_values[idx].has_value()) {
                    auto const & cc = _state->colorConfig();
                    auto const val = *_feature_values[idx];
                    auto const key = cc.color_source.has_value()
                                             ? QString::fromStdString(cc.color_source->data_key)
                                             : QString();

                    if (cc.mapping_mode == "threshold") {
                        auto const label = (static_cast<double>(val) >= cc.threshold)
                                                   ? QStringLiteral("above")
                                                   : QStringLiteral("below");
                        text += QString("\nColor: %1 (%2) [%3]")
                                        .arg(static_cast<double>(val), 0, 'f', 3)
                                        .arg(key, label);
                    } else {
                        text += QString("\nColor: %1 (%2)")
                                        .arg(static_cast<double>(val), 0, 'f', 3)
                                        .arg(key);
                    }
                }

                return text;
            });

    setAttribute(Qt::WA_AlwaysStackOnTop);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    setFormat(format);
}

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget() {
    makeCurrent();
    _preview_renderer->cleanup();
    _scene_renderer->cleanup();
    doneCurrent();
}

void ScatterPlotOpenGLWidget::setState(std::shared_ptr<ScatterPlotState> state) {
    if (_state) {
        _state->disconnect(this);
    }
    _state = std::move(state);
    if (_state) {
        _cached_view_state = _state->viewState();
        connect(_state.get(), &ScatterPlotState::stateChanged,
                this, &ScatterPlotOpenGLWidget::onStateChanged);
        connect(_state.get(), &ScatterPlotState::viewStateChanged,
                this, &ScatterPlotOpenGLWidget::onViewStateChanged);
        connect(_state.get(), &ScatterPlotState::xSourceChanged,
                this, [this]() { _scene_dirty = true; _navigated_index.reset(); update(); });
        connect(_state.get(), &ScatterPlotState::ySourceChanged,
                this, [this]() { _scene_dirty = true; _navigated_index.reset(); update(); });
        connect(_state.get(), &ScatterPlotState::referenceLineChanged,
                this, [this]() { _scene_dirty = true; update(); });
        connect(_state.get(), &ScatterPlotState::glyphStyleChanged,
                this, [this]() { _scene_dirty = true; update(); });
        connect(_state.get(), &ScatterPlotState::colorByGroupChanged,
                this, [this]() { _scene_dirty = true; update(); });
        connect(_state.get(), &ScatterPlotState::colorConfigChanged,
                this, [this]() { _scene_dirty = true; update(); });
        connect(_state.get(), &ScatterPlotState::backgroundColorChanged,
                this, [this]() {
                    updateBackgroundColor();
                    update();
                });
        connect(_state.get(), &ScatterPlotState::clusterLabelsChanged,
                this, [this]() { update(); });
        connect(_state.get(), &ScatterPlotState::selectionChanged,
                this, [this]() { _scene_dirty = true; update(); });
        connect(_state.get(), &ScatterPlotState::selectionModeChanged,
                this, [this]() {
                    // Cancel any in-progress polygon when switching modes
                    cancelPolygonSelection();
                    _scene_dirty = true;
                    update();
                });
        _scene_dirty = true;
        updateMatrices();
        update();
    }
}

void ScatterPlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
    _scene_dirty = true;
    update();
}

QString ScatterPlotOpenGLWidget::exportToSVG() {
    if (!_scene) {
        return {};
    }
    if (_scene->glyph_batches.empty() && _scene->poly_line_batches.empty() &&
        _scene->rectangle_batches.empty()) {
        return {};
    }

    // OpenGL passes matrices to SceneRenderer::render(); the CPU scene keeps defaults unless
    // we copy the live camera here (required for correct SVG projection).
    _scene->view_matrix = _view_matrix;
    _scene->projection_matrix = _projection_matrix;

    PlottingSVG::SVGSceneRenderer renderer;
    renderer.setScene(*_scene);
    renderer.setCanvasSize(_widget_width, _widget_height);

    if (_state) {
        renderer.setBackgroundColor(_state->getBackgroundColor().toStdString());
    }

    return QString::fromStdString(renderer.render());
}

void ScatterPlotOpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();
    updateBackgroundColor();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    if (!_scene_renderer->initialize()) {
        return;
    }
    _preview_renderer->initialize();
    _opengl_initialized = true;
    updateMatrices();
}

void ScatterPlotOpenGLWidget::paintGL() {
    updateBackgroundColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_opengl_initialized) {
        return;
    }

    // Check if group membership changed since last frame (generation counter pattern)
    if (_group_manager && _state && _state->colorByGroup()) {
        auto const current_gen = _group_manager->generation();
        if (current_gen != _last_group_generation) {
            _scene_dirty = true;
            _last_group_generation = current_gen;
        }
    }

    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    _scene_renderer->render(_view_matrix, _projection_matrix);

    // Render polygon selection preview overlay (in screen coordinates)
    if (_polygon_controller->isActive() && _preview_renderer && _preview_renderer->isInitialized()) {
        auto preview = _polygon_controller->getPreview();
        if (preview.isValid()) {
            _preview_renderer->render(preview, _widget_width, _widget_height);
        }
    }

    // Cluster label overlay (QPainter on top of OpenGL)
    if (_state && _state->showClusterLabels() && _group_manager && !_scatter_data.empty()) {
        drawClusterLabels();
    }
}

void ScatterPlotOpenGLWidget::resizeGL(int w, int h) {
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void ScatterPlotOpenGLWidget::showEvent(QShowEvent * event) {
    QOpenGLWidget::showEvent(event);
    update();
}

void ScatterPlotOpenGLWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        bool const ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
        bool const shift = (event->modifiers() & Qt::ShiftModifier) != 0;

        if (_state && (ctrl || shift)) {
            auto const mode = _state->selectionMode();
            if (mode == ScatterSelectionMode::SinglePoint) {
                if (ctrl) {
                    handleSinglePointCtrlClick(event->pos());
                } else if (shift) {
                    handleSinglePointShiftClick(event->pos());
                }
                event->accept();
                return;
            }
            if (mode == ScatterSelectionMode::Polygon && ctrl) {
                handlePolygonCtrlClick(event);
                event->accept();
                return;
            }
        }

        // Default: start potential pan (only when no polygon is active)
        if (!_polygon_controller->isActive()) {
            _is_panning = false;
            _pan_eligible = true;
            _click_start_pos = event->pos();
            _last_mouse_pos = event->pos();
        }
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    // Update tooltip manager
    _tooltip_mgr->onMouseMove(event->pos(), _is_panning || _polygon_controller->isActive());

    // Update polygon preview line during polygon selection
    if (_polygon_controller->isActive()) {
        auto const screen_x = static_cast<float>(event->pos().x());
        auto const screen_y = static_cast<float>(event->pos().y());
        _polygon_controller->updateCursorPosition(screen_x, screen_y);
        update();// Redraw to show updated preview
    }

    if ((event->buttons() & Qt::LeftButton) && _pan_eligible && !_polygon_controller->isActive()) {
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

void ScatterPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
        _pan_eligible = false;
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton || !_state || !_data_manager) {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    auto const hit_index = hitTestPointAt(event->pos());
    if (!hit_index.has_value()) {
        event->accept();
        return;
    }

    auto const idx = *hit_index;
    _navigated_index = idx;
    _scene_dirty = true;
    update();

    // Build TimePosition from the x-axis data source's TimeFrame
    auto const & x_source = _state->xSource();
    if (x_source.has_value()) {
        auto const time_key = _data_manager->getTimeKey(x_source->data_key);
        auto time_frame = _data_manager->getTime(time_key);
        TimeFrameIndex const tfi = _scatter_data.time_indices[idx];
        emit pointDoubleClicked(TimePosition{tfi, std::move(time_frame)});
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::wheelEvent(QWheelEvent * event) {
    // Disable zoom while a polygon selection is in progress to avoid
    // world-coordinate mismatch with the screen-space preview overlay.
    if (_polygon_controller->isActive()) {
        event->accept();
        return;
    }

    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void ScatterPlotOpenGLWidget::setKeymapManager(KeymapSystem::KeymapManager * manager) {
    if (!manager || _key_adapter) {
        return;
    }

    _key_adapter = new KeymapSystem::KeyActionAdapter(this);
    _key_adapter->setTypeId(EditorLib::EditorTypeId(QStringLiteral("ScatterPlotWidget")));

    _key_adapter->setHandler([this](QString const & action_id) -> bool {
        if (!_polygon_controller->isActive()) {
            return false;
        }
        if (action_id == QStringLiteral("scatter_plot.polygon_complete")) {
            completePolygonSelection();
            return true;
        }
        if (action_id == QStringLiteral("scatter_plot.polygon_cancel")) {
            cancelPolygonSelection();
            return true;
        }
        if (action_id == QStringLiteral("scatter_plot.polygon_undo_vertex")) {
            _polygon_controller->removeLastVertex();
            if (!_polygon_vertices_world.empty()) {
                _polygon_vertices_world.pop_back();
            }
            update();
            return true;
        }
        return false;
    });

    manager->registerAdapter(_key_adapter);
}

void ScatterPlotOpenGLWidget::leaveEvent(QEvent * event) {
    _tooltip_mgr->onLeave();
    QOpenGLWidget::leaveEvent(event);
}

void ScatterPlotOpenGLWidget::onStateChanged() {
    _scene_dirty = true;
    update();
}

void ScatterPlotOpenGLWidget::onViewStateChanged() {
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    // Reference line needs rebuild on view change (endpoints depend on visible range)
    if (_state && _state->showReferenceLine()) {
        _scene_dirty = true;
    }
    update();
    emit viewBoundsChanged();
}

void ScatterPlotOpenGLWidget::updateBackgroundColor() {
    if (!_state) {
        int r = 0;
        int g = 0;
        int b = 0;
        hexToRGB("#1A1A1A", r, g, b);
        glClearColor(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                1.0f);
        return;
    }

    QString const hex_color = _state->getBackgroundColor();
    int r = 0;
    int g = 0;
    int b = 0;
    hexToRGB(hex_color.toStdString(), r, g, b);
    glClearColor(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);
}

void ScatterPlotOpenGLWidget::updateMatrices() {
    _projection_matrix =
            WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);
}

void ScatterPlotOpenGLWidget::handlePanning(int delta_x, int delta_y) {
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handlePanning(
            *_state, _cached_view_state, delta_x, delta_y, _widget_width,
            _widget_height);
}

void ScatterPlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes) {
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(
            *_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF ScatterPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const {
    return WhiskerToolbox::Plots::screenToWorld(
            _projection_matrix, _widget_width, _widget_height, screen_pos);
}

void ScatterPlotOpenGLWidget::rebuildScene() {
    if (!_state || !_data_manager) {
        _scene_renderer->clearScene();
        return;
    }

    auto const & x_source = _state->xSource();
    auto const & y_source = _state->ySource();

    // Clear scatter data if sources are not configured
    if (!x_source.has_value() || !y_source.has_value()) {
        _scatter_data.clear();
        _scene_renderer->clearScene();
        return;
    }

    // Validate compatibility
    auto compat = checkSourceCompatibility(*_data_manager, *x_source, *y_source);
    if (!compat.compatible) {
        _scatter_data.clear();
        _scene_renderer->clearScene();
        return;
    }

    // Build the scatter point data
    _scatter_data = buildScatterPoints(*_data_manager, *x_source, *y_source);

    if (_scatter_data.empty()) {
        _scene_renderer->clearScene();
        return;
    }

    // Compute bounds for auto-fit (with 5% padding)
    float x_min = *std::min_element(_scatter_data.x_values.begin(), _scatter_data.x_values.end());
    float x_max = *std::max_element(_scatter_data.x_values.begin(), _scatter_data.x_values.end());
    float y_min = *std::min_element(_scatter_data.y_values.begin(), _scatter_data.y_values.end());
    float y_max = *std::max_element(_scatter_data.y_values.begin(), _scatter_data.y_values.end());

    float const x_range = x_max - x_min;
    float const y_range = y_max - y_min;
    float const x_pad = (x_range > 0.0f) ? x_range * 0.05f : 1.0f;
    float const y_pad = (y_range > 0.0f) ? y_range * 0.05f : 1.0f;

    x_min -= x_pad;
    x_max += x_pad;
    y_min -= y_pad;
    y_max += y_pad;

    // Auto-fit bounds on first data load (when bounds are at defaults)
    auto const & vs = _state->viewState();
    bool const is_default_bounds = (vs.x_min == 0.0 && vs.x_max == 100.0 && vs.y_min == 0.0 && vs.y_max == 100.0);
    if (is_default_bounds) {
        _state->setXBounds(static_cast<double>(x_min), static_cast<double>(x_max));
        _state->setYBounds(static_cast<double>(y_min), static_cast<double>(y_max));
        _cached_view_state = _state->viewState();
        updateMatrices();
    }

    // Build the scene
    BoundingBox const bbox{x_min, y_min, x_max, y_max};
    CorePlotting::SceneBuilder builder;
    builder.setBounds(bbox);

    // Add scatter points as glyphs (using glyph style from state)
    std::vector<CorePlotting::MappedElement> elements;
    elements.reserve(_scatter_data.size());
    for (std::size_t i = 0; i < _scatter_data.size(); ++i) {
        elements.emplace_back(
                _scatter_data.x_values[i],
                _scatter_data.y_values[i],
                EntityId{static_cast<uint64_t>(i)});
    }

    auto const & glyph_data = _state->glyphStyleState()->data();

    CorePlotting::GlyphStyle point_style;
    point_style.glyph_type = CorePlotting::toRenderableGlyphType(glyph_data.glyph_type);
    point_style.size = glyph_data.size;
    point_style.color = CorePlotting::hexColorToVec4(glyph_data.hex_color, glyph_data.alpha);

    // Separate selected and unselected points into two batches
    auto const & selected_indices = _state->selectedIndices();
    std::unordered_set<std::size_t> const selected_set(selected_indices.begin(), selected_indices.end());

    std::vector<CorePlotting::MappedElement> unselected_elements;
    std::vector<CorePlotting::MappedElement> selected_elements;
    unselected_elements.reserve(elements.size());

    for (std::size_t i = 0; i < elements.size(); ++i) {
        if (selected_set.contains(i)) {
            selected_elements.push_back(elements[i]);
        } else {
            unselected_elements.push_back(elements[i]);
        }
    }

    // Render unselected points with the normal style
    if (!unselected_elements.empty()) {
        builder.addGlyphs("scatter_points", std::move(unselected_elements), point_style);
    }

    // Render selected points with a highlighted style (larger, orange/yellow ring)
    if (!selected_elements.empty()) {
        CorePlotting::GlyphStyle selected_style;
        selected_style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
        selected_style.size = glyph_data.size + 4.0f;
        selected_style.color = glm::vec4(1.0f, 0.6f, 0.0f, 0.9f);// Orange highlight

        builder.addGlyphs("scatter_selected", std::move(selected_elements), selected_style);
    }

    // Add highlight glyph for the most recently navigated-to point
    if (_navigated_index.has_value() && *_navigated_index < elements.size()) {
        CorePlotting::GlyphStyle highlight_style;
        highlight_style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
        highlight_style.size = 9.0f;
        highlight_style.color = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f);// Yellow highlight

        std::vector<CorePlotting::MappedElement> const highlight_elem{elements[*_navigated_index]};
        builder.addGlyphs("scatter_highlight", highlight_elem, highlight_style);
    }

    // Add y=x reference line if enabled
    if (_state->showReferenceLine()) {
        float const ref_min = std::min(x_min, y_min);
        float const ref_max = std::max(x_max, y_max);

        std::vector<CorePlotting::MappedVertex> ref_vertices;
        ref_vertices.emplace_back(ref_min, ref_min);
        ref_vertices.emplace_back(ref_max, ref_max);

        CorePlotting::PolyLineStyle ref_style;
        ref_style.thickness = 1.0f;
        ref_style.color = glm::vec4(0.6f, 0.6f, 0.6f, 0.5f);

        builder.addPolyLine("reference_line", std::move(ref_vertices),
                            EntityId{0}, ref_style);
    }

    auto scene = builder.build();
    _scene = std::make_unique<CorePlotting::RenderableScene>(std::move(scene));
    applyFeatureColorsToSceneImpl();
    applyGroupColorsToScene();
    _scene_renderer->uploadScene(*_scene);
}

void ScatterPlotOpenGLWidget::applyFeatureColorsToSceneImpl() {
    _feature_values.clear();

    if (!_state || !_data_manager || !_scene || _scatter_data.empty()) {
        return;
    }

    auto const & cc = _state->colorConfig();
    if (cc.color_mode != "by_feature" || !cc.color_source.has_value()) {
        return;
    }

    // Obtain the time frame from the X-axis source
    std::shared_ptr<TimeFrame> point_time_frame;
    auto const & x_src = _state->xSource();
    if (x_src.has_value() && !x_src->data_key.empty()) {
        if (auto ats = _data_manager->getData<AnalogTimeSeries>(x_src->data_key)) {
            point_time_frame = ats->getTimeFrame();
        } else if (auto td = _data_manager->getData<TensorData>(x_src->data_key)) {
            point_time_frame = td->getTimeFrame();
        }
    }

    // Resolve per-point float values from the feature source
    _feature_values = CorePlotting::FeatureColor::resolveFeatureColors(
            *_data_manager, *cc.color_source,
            _scatter_data.time_indices, point_time_frame);

    // Build the mapping
    CorePlotting::FeatureColor::FeatureColorMapping mapping;
    if (cc.mapping_mode == "threshold") {
        mapping = CorePlotting::FeatureColor::ThresholdMapping{
                static_cast<float>(cc.threshold),
                CorePlotting::hexColorToVec4(cc.above_color, cc.above_alpha),
                CorePlotting::hexColorToVec4(cc.below_color, cc.below_alpha)};
    } else {
        CorePlotting::FeatureColor::ColorRangeConfig range_config;
        if (cc.color_range_mode == "manual") {
            range_config.mode = CorePlotting::FeatureColor::ColorRangeMode::Manual;
        } else if (cc.color_range_mode == "symmetric") {
            range_config.mode = CorePlotting::FeatureColor::ColorRangeMode::Symmetric;
        } else {
            range_config.mode = CorePlotting::FeatureColor::ColorRangeMode::Auto;
        }
        range_config.manual_vmin = cc.color_range_vmin;
        range_config.manual_vmax = cc.color_range_vmax;

        auto const range = CorePlotting::FeatureColor::computeEffectiveColorRange(
                _feature_values, range_config);
        if (!range.has_value()) {
            return;// No valid values — cannot compute range
        }

        auto const preset = CorePlotting::Colormaps::presetFromName(cc.colormap_preset);
        mapping = CorePlotting::FeatureColor::ContinuousMapping{
                preset, range->first, range->second};
    }

    // Build skip set (selected + navigated points keep their dedicated colors)
    auto const & selected_indices = _state->selectedIndices();
    std::unordered_set<std::size_t> skip_set(selected_indices.begin(), selected_indices.end());
    if (_navigated_index.has_value()) {
        skip_set.insert(*_navigated_index);
    }

    CorePlotting::FeatureColor::applyFeatureColorsToScene(
            *_scene, _feature_values, mapping, _scatter_data.size(), skip_set);
}

void ScatterPlotOpenGLWidget::applyGroupColorsToScene() {
    if (!_state || !_state->colorByGroup() || !_group_manager) {
        return;
    }

    if (_scatter_data.empty()) {
        return;
    }

    // Color priority (highest → lowest):
    //   1. Navigated-to highlight (yellow) — scatter_highlight batch
    //   2. Selected (orange)               — scatter_selected batch
    //   3. Group color                     — applied here to scatter_points batch
    //   4. Default glyph style color       — already set by addGlyphs
    //
    // Build skip sets so we only recolor unselected, un-navigated points.
    auto const & selected_indices = _state->selectedIndices();
    std::unordered_set<std::size_t> skip_set(selected_indices.begin(), selected_indices.end());
    if (_navigated_index.has_value()) {
        skip_set.insert(*_navigated_index);
    }

    // Apply group colors to glyph batches that contain scatter points.
    for (auto & batch: _scene->glyph_batches) {
        for (std::size_t i = 0; i < batch.entity_ids.size(); ++i) {
            auto const scatter_idx = static_cast<std::size_t>(batch.entity_ids[i].id);
            if (scatter_idx >= _scatter_data.size()) {
                continue;
            }

            // Skip selected / navigated points — they keep their dedicated colors
            if (skip_set.contains(scatter_idx)) {
                continue;
            }

            // Map scatter index → EntityId via TimeFrameIndex
            auto const entity_id = getEntityIdForPoint(scatter_idx);
            if (!entity_id.has_value()) {
                continue;
            }

            int const group_id = _group_manager->getEntityGroup(*entity_id);
            if (group_id == -1) {
                continue;// Not in any group — keep default color
            }

            QColor const group_color = _group_manager->getEntityColor(*entity_id, QColor());
            if (!group_color.isValid()) {
                continue;
            }

            batch.colors[i] = glm::vec4(
                    static_cast<float>(group_color.redF()),
                    static_cast<float>(group_color.greenF()),
                    static_cast<float>(group_color.blueF()),
                    static_cast<float>(group_color.alphaF()));
        }
    }
}

void ScatterPlotOpenGLWidget::setGroupManager(GroupManager * group_manager) {
    // Disconnect from previous group manager if any
    if (_group_manager) {
        disconnect(_group_manager, nullptr, this, nullptr);
    }

    _group_manager = group_manager;

    // Create context menu if it doesn't exist
    if (!_context_menu) {
        createContextMenu();
    }

    // Update the group menu handler with the new group manager
    if (_group_menu_handler) {
        _group_menu_handler->setGroupManager(group_manager);
    }

    // Connect to group manager signals if available
    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated, this, [this]() {
            _scene_dirty = true;
            update();
        });
        connect(_group_manager, &GroupManager::groupRemoved, this, [this]() {
            _scene_dirty = true;
            update();
        });
        connect(_group_manager, &GroupManager::groupModified, this, [this]() {
            _scene_dirty = true;
            update();
        });
    }
}

void ScatterPlotOpenGLWidget::setSelectionContext(SelectionContext * selection_context) {
    _selection_context = selection_context;

    // Ensure context menu exists so ContextActions can be shown
    if (!_context_menu) {
        createContextMenu();
    }
}

void ScatterPlotOpenGLWidget::createContextMenu() {
    _context_menu = new QMenu(this);

    // Create the group context menu handler
    _group_menu_handler = std::make_unique<GroupContextMenuHandler>(this);

    // Setup callbacks for the group handler
    GroupContextMenuCallbacks callbacks;
    callbacks.getSelectedEntities = [this]() {
        return getSelectedEntities();
    };
    callbacks.clearSelection = [this]() {
        if (_state) {
            _state->clearSelection();
        }
    };
    callbacks.hasSelection = [this]() {
        return _state && !_state->selectedIndices().empty();
    };
    callbacks.onGroupOperationCompleted = [this]() {
        if (_state) {
            _state->clearSelection();
        }
        _scene_dirty = true;
        update();
    };

    _group_menu_handler->setCallbacks(callbacks);

    if (_group_manager) {
        _group_menu_handler->setGroupManager(_group_manager);
    }

    _group_menu_handler->setupGroupMenuSection(_context_menu, true);

    auto * clear_selection_action = new QAction("Clear Selection", this);
    _context_menu->addAction(clear_selection_action);
    connect(clear_selection_action, &QAction::triggered, this, [this]() {
        if (_state) {
            _state->clearSelection();
        }
    });

    // "Cluster Selection..." action — creates filtered tensor from selected points
    _context_menu->addSeparator();
    _cluster_selection_action = new QAction("Cluster Selection...", this);
    _cluster_selection_action->setEnabled(false);
    _context_menu->addAction(_cluster_selection_action);
    connect(_cluster_selection_action, &QAction::triggered, this, [this]() {
        _executeClusterSelection();
    });
}

void ScatterPlotOpenGLWidget::contextMenuEvent(QContextMenuEvent * event) {
    if (!_context_menu) {
        QOpenGLWidget::contextMenuEvent(event);
        return;
    }

    if (_group_menu_handler) {
        _group_menu_handler->updateMenuState(_context_menu);
    }

    // Enable "Cluster Selection..." only when we have a selection and a tensor source
    if (_cluster_selection_action) {
        bool const has_selection = _state && !_state->selectedIndices().empty();
        bool const has_tensor_source = !_scatter_data.source_data_key.empty() && _data_manager && _data_manager->getData<TensorData>(_scatter_data.source_data_key) != nullptr;
        _cluster_selection_action->setEnabled(has_selection && has_tensor_source && _selection_context);
    }

    // Remove previously added dynamic ContextAction items
    for (auto * action: _dynamic_context_actions) {
        _context_menu->removeAction(action);
        action->deleteLater();
    }
    _dynamic_context_actions.clear();

    // Add applicable ContextActions from SelectionContext
    if (_selection_context) {
        auto const applicable = _selection_context->applicableActions();
        if (!applicable.empty()) {
            _dynamic_context_actions.push_back(_context_menu->addSeparator());
            for (auto const * ctx_action: applicable) {
                auto * menu_action = _context_menu->addAction(ctx_action->display_name);
                auto const * action_ptr = ctx_action;
                connect(menu_action, &QAction::triggered, this, [this, action_ptr]() {
                    action_ptr->execute(*_selection_context);
                });
                _dynamic_context_actions.push_back(menu_action);
            }
        }
    }

    _context_menu->popup(event->globalPos());
    event->accept();
}

std::optional<std::size_t> ScatterPlotOpenGLWidget::hitTestPointAt(QPoint const & screen_pos) const {
    if (!_state || !_data_manager || _scatter_data.empty()) {
        return std::nullopt;
    }

    QPointF const world = screenToWorld(screen_pos);

    // Convert pixel tolerance to world units
    auto const world_per_pixel_x = static_cast<float>(
            (_cached_view_state.x_max - _cached_view_state.x_min) / (_widget_width * _cached_view_state.x_zoom));
    auto const world_per_pixel_y = static_cast<float>(
            (_cached_view_state.y_max - _cached_view_state.y_min) / (_widget_height * _cached_view_state.y_zoom));
    float const world_tolerance = 8.0f * std::max(world_per_pixel_x, world_per_pixel_y);

    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;

    // Note: We need a mutable hit tester for the query, but this is safe
    // as the config doesn't persist and queryQuadTree is conceptually const
    auto & mutable_tester = const_cast<CorePlotting::SceneHitTester &>(*_hit_tester);
    mutable_tester.setConfig(config);

    auto const result = mutable_tester.queryQuadTree(
            static_cast<float>(world.x()),
            static_cast<float>(world.y()),
            *_scene);

    if (result.hasHit() && result.entity_id.has_value()) {
        auto const idx = static_cast<std::size_t>(result.entity_id->id);
        if (idx < _scatter_data.size()) {
            return idx;
        }
    }

    return std::nullopt;
}

std::unordered_set<EntityId> ScatterPlotOpenGLWidget::getSelectedEntities() const {
    std::unordered_set<EntityId> result;
    if (!_state || _scatter_data.empty()) {
        return result;
    }
    for (auto const idx: _state->selectedIndices()) {
        if (idx < _scatter_data.size()) {
            auto eid = getEntityIdForPoint(idx);
            if (eid.has_value()) {
                result.insert(*eid);
            }
        }
    }
    return result;
}

std::optional<EntityId> ScatterPlotOpenGLWidget::getEntityIdForPoint(std::size_t index) const {
    if (index >= _scatter_data.size() || !_state || !_data_manager) {
        return std::nullopt;
    }

    // Ordinal sources have no entity mapping
    if (_scatter_data.source_row_type == ScatterSourceRowType::TensorOrdinal || _scatter_data.source_row_type == ScatterSourceRowType::Unknown) {
        return std::nullopt;
    }

    if (_scatter_data.source_data_key.empty()) {
        return std::nullopt;
    }

    TimeFrameIndex const tfi = _scatter_data.time_indices[index];
    auto const time_key = _data_manager->getTimeKey(_scatter_data.source_data_key);
    return _data_manager->ensureTimeEntityId(time_key, tfi);
}

// === Single-point selection ===

void ScatterPlotOpenGLWidget::handleSinglePointCtrlClick(QPoint const & screen_pos) {
    if (!_state) return;

    auto const hit_index = hitTestPointAt(screen_pos);
    if (!hit_index.has_value()) {
        return;
    }
    auto const idx = *hit_index;

    // Toggle: if already selected, deselect; otherwise, add to selection
    if (_state->isSelected(idx)) {
        _state->removeSelectedIndex(idx);
    } else {
        _state->addSelectedIndex(idx);
    }
}

void ScatterPlotOpenGLWidget::handleSinglePointShiftClick(QPoint const & screen_pos) {
    if (!_state) return;

    auto const hit_index = hitTestPointAt(screen_pos);
    if (!hit_index.has_value()) {
        return;
    }
    // Shift+Click always removes from selection
    _state->removeSelectedIndex(*hit_index);
}

// === Polygon selection ===

void ScatterPlotOpenGLWidget::handlePolygonCtrlClick(QMouseEvent * event) {
    auto const screen_x = static_cast<float>(event->pos().x());
    auto const screen_y = static_cast<float>(event->pos().y());
    QPointF const world = screenToWorld(event->pos());

    if (!_polygon_controller->isActive()) {
        // Start a new polygon
        _polygon_vertices_world.clear();
        _polygon_vertices_world.emplace_back(
                static_cast<float>(world.x()), static_cast<float>(world.y()));
        _polygon_controller->start(screen_x, screen_y, "scatter_polygon_selection");
    } else {
        // Add a vertex
        _polygon_vertices_world.emplace_back(
                static_cast<float>(world.x()), static_cast<float>(world.y()));
        auto result = _polygon_controller->addVertex(screen_x, screen_y);
        if (result == CorePlotting::Interaction::AddVertexResult::ClosedPolygon) {
            completePolygonSelection();
            return;
        }
    }
    update();
}

void ScatterPlotOpenGLWidget::completePolygonSelection() {
    if (!_state || _polygon_vertices_world.size() < 3) {
        cancelPolygonSelection();
        return;
    }

    _polygon_controller->complete();

    // Run point-in-polygon selection on the scatter data
    auto selection_result = CorePlotting::Selection::selectPointsInPolygon(
            _polygon_vertices_world,
            _scatter_data.x_values,
            _scatter_data.y_values);

    // Merge into the current selection (additive)
    for (auto idx: selection_result.selected_indices) {
        _state->addSelectedIndex(idx);
    }

    _polygon_vertices_world.clear();
    _scene_dirty = true;
    update();
}

void ScatterPlotOpenGLWidget::cancelPolygonSelection() {
    _polygon_controller->cancel();
    _polygon_vertices_world.clear();
    update();
}

void ScatterPlotOpenGLWidget::_executeClusterSelection() {
    if (!_state || !_data_manager || !_selection_context) {
        return;
    }

    auto const & selected = _state->selectedIndices();
    if (selected.empty()) {
        return;
    }

    auto const & source_key = _scatter_data.source_data_key;
    auto source_tensor = _data_manager->getData<TensorData>(source_key);
    if (!source_tensor) {
        return;
    }

    auto const num_cols = source_tensor->numColumns();
    if (num_cols == 0) {
        return;
    }

    // Build flat row-major data for selected rows
    std::vector<float> filtered_data;
    filtered_data.reserve(selected.size() * num_cols);
    for (auto const idx: selected) {
        if (idx >= source_tensor->numRows()) {
            continue;
        }
        auto row_data = source_tensor->row(idx);
        filtered_data.insert(filtered_data.end(), row_data.begin(), row_data.end());
    }

    auto const actual_rows = filtered_data.size() / num_cols;
    if (actual_rows == 0) {
        return;
    }

    // Create filtered tensor (ordinal — no time semantics for the subset)
    auto filtered_tensor = TensorData::createOrdinal2D(
            filtered_data, actual_rows, num_cols,
            source_tensor->columnNames());

    // Store in DataManager with a derived key
    std::string const selection_key = source_key + "_selection";
    auto filtered_ptr = std::make_shared<TensorData>(std::move(filtered_tensor));
    auto const time_key = _data_manager->getTimeKey(source_key);
    _data_manager->setData<TensorData>(selection_key, filtered_ptr, time_key);

    // Set data focus to the new selection tensor
    SelectionSource const source{EditorLib::EditorInstanceId(QStringLiteral("scatter_cluster_selection")), QStringLiteral("ScatterPlotWidget")};
    _selection_context->setDataFocus(
            SelectedDataKey(QString::fromStdString(selection_key)),
            QStringLiteral("TensorData"),
            source);

    // Find and execute the cluster ContextAction
    auto const applicable = _selection_context->applicableActions();
    for (auto const * action: applicable) {
        if (action->action_id == QStringLiteral("mlcore.cluster_tensor")) {
            action->execute(*_selection_context);
            return;
        }
    }
}

void ScatterPlotOpenGLWidget::drawClusterLabels() {
    // Accumulate centroid (sum_x, sum_y, count) per group
    struct GroupAccum {
        double sum_x = 0.0;
        double sum_y = 0.0;
        int count = 0;
        QColor color;
        QString name;
    };
    std::map<int, GroupAccum> groups;

    for (std::size_t i = 0; i < _scatter_data.size(); ++i) {
        auto const entity_id = getEntityIdForPoint(i);
        if (!entity_id.has_value()) {
            continue;
        }
        int const gid = _group_manager->getEntityGroup(*entity_id);
        if (gid == -1) {
            continue;
        }
        auto & accum = groups[gid];
        accum.sum_x += static_cast<double>(_scatter_data.x_values[i]);
        accum.sum_y += static_cast<double>(_scatter_data.y_values[i]);
        accum.count++;
    }

    if (groups.empty()) {
        return;
    }

    // Fill group metadata
    for (auto & [gid, accum]: groups) {
        auto const group_info = _group_manager->getGroup(gid);
        if (group_info.has_value()) {
            accum.color = group_info->color;
            accum.name = group_info->name;
        }
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont label_font;
    label_font.setPointSize(10);
    label_font.setBold(true);
    painter.setFont(label_font);

    for (auto const & [gid, accum]: groups) {
        if (accum.count == 0) {
            continue;
        }

        auto const cx = static_cast<float>(accum.sum_x / accum.count);
        auto const cy = static_cast<float>(accum.sum_y / accum.count);

        QPoint const screen = WhiskerToolbox::Plots::worldToScreen(
                _projection_matrix, _widget_width, _widget_height, cx, cy);

        QString const label = QStringLiteral("%1 (n=%2)").arg(accum.name).arg(accum.count);

        // Draw text with a dark outline for readability
        QRect const text_rect(screen.x() + 6, screen.y() - 8, 200, 20);
        painter.setPen(QPen(Qt::black, 2));
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
        painter.setPen(accum.color.isValid() ? accum.color : Qt::white);
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    painter.end();
}
