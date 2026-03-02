#include "ScatterPlotOpenGLWidget.hpp"

#include "Core/BuildScatterPoints.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Core/SourceCompatibility.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/DataTypes/GlyphStyleConversion.hpp"
#include "CorePlotting/Interaction/HitTestResult.hpp"
#include "CorePlotting/Mappers/MappedElement.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CorePlotting/Selection/PolygonSelection.hpp"
#include "DataManager/DataManager.hpp"
#include "GroupContextMenu/GroupContextMenuHandler.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent) {
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
    _preview_renderer.cleanup();
    _scene_renderer.cleanup();
    doneCurrent();
}

void ScatterPlotOpenGLWidget::setState(std::shared_ptr<ScatterPlotState> state) {
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
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
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

void ScatterPlotOpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto fmt = format();
    if (fmt.samples() > 1) {
        glEnable(GL_MULTISAMPLE);
    }

    if (!_scene_renderer.initialize()) {
        return;
    }
    _preview_renderer.initialize();
    _opengl_initialized = true;
    updateMatrices();
}

void ScatterPlotOpenGLWidget::paintGL() {
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

    _scene_renderer.render(_view_matrix, _projection_matrix);

    // Render polygon selection preview overlay (in screen coordinates)
    if (_polygon_controller.isActive() && _preview_renderer.isInitialized()) {
        auto preview = _polygon_controller.getPreview();
        if (preview.isValid()) {
            _preview_renderer.render(preview, _widget_width, _widget_height);
        }
    }
}

void ScatterPlotOpenGLWidget::resizeGL(int w, int h) {
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
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
        if (!_polygon_controller.isActive()) {
            _is_panning = false;
            _pan_eligible = true;
            _click_start_pos = event->pos();
            _last_mouse_pos = event->pos();
        }
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    // Update polygon preview line during polygon selection
    if (_polygon_controller.isActive()) {
        auto const screen_x = static_cast<float>(event->pos().x());
        auto const screen_y = static_cast<float>(event->pos().y());
        _polygon_controller.updateCursorPosition(screen_x, screen_y);
        update();// Redraw to show updated preview
    }

    if ((event->buttons() & Qt::LeftButton) && _pan_eligible && !_polygon_controller.isActive()) {
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
    if (_polygon_controller.isActive()) {
        event->accept();
        return;
    }

    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void ScatterPlotOpenGLWidget::keyPressEvent(QKeyEvent * event) {
    if (_polygon_controller.isActive()) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            completePolygonSelection();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            cancelPolygonSelection();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Backspace) {
            _polygon_controller.removeLastVertex();
            if (!_polygon_vertices_world.empty()) {
                _polygon_vertices_world.pop_back();
            }
            update();
            event->accept();
            return;
        }
    }
    QOpenGLWidget::keyPressEvent(event);
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
        _scene_renderer.clearScene();
        return;
    }

    auto const & x_source = _state->xSource();
    auto const & y_source = _state->ySource();

    // Clear scatter data if sources are not configured
    if (!x_source.has_value() || !y_source.has_value()) {
        _scatter_data.clear();
        _scene_renderer.clearScene();
        return;
    }

    // Validate compatibility
    auto compat = checkSourceCompatibility(*_data_manager, *x_source, *y_source);
    if (!compat.compatible) {
        _scatter_data.clear();
        _scene_renderer.clearScene();
        return;
    }

    // Build the scatter point data
    _scatter_data = buildScatterPoints(*_data_manager, *x_source, *y_source);

    if (_scatter_data.empty()) {
        _scene_renderer.clearScene();
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
    BoundingBox bbox{x_min, y_min, x_max, y_max};
    CorePlotting::SceneBuilder builder;
    builder.setBounds(bbox);

    // Add scatter points as glyphs (using glyph style from state)
    std::vector<CorePlotting::MappedElement> elements;
    elements.reserve(_scatter_data.size());
    for (std::size_t i = 0; i < _scatter_data.size(); ++i) {
        elements.push_back(CorePlotting::MappedElement{
                _scatter_data.x_values[i],
                _scatter_data.y_values[i],
                EntityId{static_cast<uint64_t>(i)}});
    }

    auto const & glyph_data = _state->glyphStyleState()->data();

    CorePlotting::GlyphStyle point_style;
    point_style.glyph_type = CorePlotting::toRenderableGlyphType(glyph_data.glyph_type);
    point_style.size = glyph_data.size;
    point_style.color = CorePlotting::hexColorToVec4(glyph_data.hex_color, glyph_data.alpha);

    // Separate selected and unselected points into two batches
    auto const & selected_indices = _state->selectedIndices();
    std::unordered_set<std::size_t> selected_set(selected_indices.begin(), selected_indices.end());

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

        std::vector<CorePlotting::MappedElement> highlight_elem{elements[*_navigated_index]};
        builder.addGlyphs("scatter_highlight", std::move(highlight_elem), highlight_style);
    }

    // Add y=x reference line if enabled
    if (_state->showReferenceLine()) {
        float const ref_min = std::min(x_min, y_min);
        float const ref_max = std::max(x_max, y_max);

        std::vector<CorePlotting::MappedVertex> ref_vertices;
        ref_vertices.push_back(CorePlotting::MappedVertex{ref_min, ref_min});
        ref_vertices.push_back(CorePlotting::MappedVertex{ref_max, ref_max});

        CorePlotting::PolyLineStyle ref_style;
        ref_style.thickness = 1.0f;
        ref_style.color = glm::vec4(0.6f, 0.6f, 0.6f, 0.5f);

        builder.addPolyLine("reference_line", std::move(ref_vertices),
                            EntityId{0}, ref_style);
    }

    _scene = builder.build();
    applyGroupColorsToScene();
    _scene_renderer.uploadScene(_scene);
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
    for (auto & batch: _scene.glyph_batches) {
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
}

void ScatterPlotOpenGLWidget::contextMenuEvent(QContextMenuEvent * event) {
    if (!_context_menu || !_group_manager) {
        QOpenGLWidget::contextMenuEvent(event);
        return;
    }

    _group_menu_handler->updateMenuState(_context_menu);
    _context_menu->popup(event->globalPos());
    event->accept();
}

std::optional<std::size_t> ScatterPlotOpenGLWidget::hitTestPointAt(QPoint const & screen_pos) const {
    if (!_state || !_data_manager || _scatter_data.empty()) {
        return std::nullopt;
    }

    QPointF const world = screenToWorld(screen_pos);

    // Convert pixel tolerance to world units
    float const world_per_pixel_x = static_cast<float>(
            (_cached_view_state.x_max - _cached_view_state.x_min) / (_widget_width * _cached_view_state.x_zoom));
    float const world_per_pixel_y = static_cast<float>(
            (_cached_view_state.y_max - _cached_view_state.y_min) / (_widget_height * _cached_view_state.y_zoom));
    float const world_tolerance = 8.0f * std::max(world_per_pixel_x, world_per_pixel_y);

    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;

    // Note: We need a mutable hit tester for the query, but this is safe
    // as the config doesn't persist and queryQuadTree is conceptually const
    auto & mutable_tester = const_cast<CorePlotting::SceneHitTester &>(_hit_tester);
    mutable_tester.setConfig(config);

    auto const result = mutable_tester.queryQuadTree(
            static_cast<float>(world.x()),
            static_cast<float>(world.y()),
            _scene);

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

    TimeFrameIndex const tfi = _scatter_data.time_indices[index];
    return EntityId{static_cast<uint64_t>(tfi.getValue())};
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

    if (!_polygon_controller.isActive()) {
        // Start a new polygon
        _polygon_vertices_world.clear();
        _polygon_vertices_world.emplace_back(
                static_cast<float>(world.x()), static_cast<float>(world.y()));
        _polygon_controller.start(screen_x, screen_y, "scatter_polygon_selection");
    } else {
        // Add a vertex
        _polygon_vertices_world.emplace_back(
                static_cast<float>(world.x()), static_cast<float>(world.y()));
        auto result = _polygon_controller.addVertex(screen_x, screen_y);
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

    _polygon_controller.complete();

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
    _polygon_controller.cancel();
    _polygon_vertices_world.clear();
    update();
}
