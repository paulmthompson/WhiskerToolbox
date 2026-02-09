#include "TemporalProjectionOpenGLWidget.hpp"

#include "Core/TemporalProjectionViewState.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/LineBatch/CpuLineBatchIntersector.hpp"
#include "CorePlotting/LineBatch/LineBatchBuilder.hpp"
#include "CorePlotting/Mappers/SpatialMapper_AllTimes.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Plots/Common/LineSelectionHelpers.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "PlottingOpenGL/LineBatch/ComputeShaderIntersector.hpp"
#include "Points/Point_Data.hpp"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <limits>

TemporalProjectionOpenGLWidget::TemporalProjectionOpenGLWidget(QWidget * parent)
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

TemporalProjectionOpenGLWidget::~TemporalProjectionOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void TemporalProjectionOpenGLWidget::setState(std::shared_ptr<TemporalProjectionViewState> state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        _cached_view_state = _state->viewState();
        connect(_state.get(), &TemporalProjectionViewState::stateChanged,
                this, &TemporalProjectionOpenGLWidget::onStateChanged);
        connect(_state.get(), &TemporalProjectionViewState::viewStateChanged,
                this, &TemporalProjectionOpenGLWidget::onViewStateChanged);
        connect(_state.get(), &TemporalProjectionViewState::pointDataKeyAdded,
                this, &TemporalProjectionOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &TemporalProjectionViewState::pointDataKeyRemoved,
                this, &TemporalProjectionOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &TemporalProjectionViewState::pointDataKeysCleared,
                this, &TemporalProjectionOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &TemporalProjectionViewState::lineDataKeyAdded,
                this, &TemporalProjectionOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &TemporalProjectionViewState::lineDataKeyRemoved,
                this, &TemporalProjectionOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &TemporalProjectionViewState::lineDataKeysCleared,
                this, &TemporalProjectionOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &TemporalProjectionViewState::pointSizeChanged,
                this, [this](float) { _scene_dirty = true; update(); });
        connect(_state.get(), &TemporalProjectionViewState::lineWidthChanged,
                this, [this](float) { _scene_dirty = true; update(); });
        _scene_dirty = true;
        updateMatrices();
        update();
    }
}

void TemporalProjectionOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

std::pair<double, double> TemporalProjectionOpenGLWidget::getViewBounds() const
{
    return {_cached_view_state.x_min, _cached_view_state.x_max};
}

void TemporalProjectionOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!_scene_renderer.initialize()) {
        qWarning() << "TemporalProjectionOpenGLWidget: Failed to initialize SceneRenderer";
    }
    // Initialize line batch store and renderer for selectable lines
    if (!_line_store.initialize()) {
        qWarning() << "TemporalProjectionOpenGLWidget: Failed to initialize BatchLineStore";
    }
    if (!_line_renderer.initialize()) {
        qWarning() << "TemporalProjectionOpenGLWidget: Failed to initialize BatchLineRenderer";
    }

    // Set visible colors for line states
    _line_renderer.setGlobalColor(glm::vec4{0.8f, 0.2f, 0.2f, 0.6f});   // Semi-transparent red for normal lines
    _line_renderer.setSelectedColor(glm::vec4{1.0f, 0.8f, 0.0f, 1.0f}); // Bright yellow for selected lines
    _line_renderer.setLineWidth(1.5f);

    // Initialize line intersector (GPU compute shader or CPU fallback)
    auto const sf = format();
    bool const has_compute = [&sf]() {
        return (sf.majorVersion() > 4) ||
               (sf.majorVersion() == 4 && sf.minorVersion() >= 3);
    }();

    if (has_compute) {
        auto gpu = std::make_unique<PlottingOpenGL::ComputeShaderIntersector>(_line_store);
        if (gpu->initialize()) {
            _intersector = std::move(gpu);
            qDebug() << "TemporalProjectionOpenGLWidget: Using GPU compute shader intersector";
        }
    }
    if (!_intersector) {
        _intersector = std::make_unique<CorePlotting::CpuLineBatchIntersector>();
        qDebug() << "TemporalProjectionOpenGLWidget: Using CPU intersector fallback";
    }    
    _opengl_initialized = true;
}

void TemporalProjectionOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_state || !_opengl_initialized) {
        return;
    }

    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    // Render points and lines via SceneRenderer
    _scene_renderer.render(_projection_matrix, _view_matrix);

    // Render selectable lines via BatchLineRenderer
    if (!_line_store.cpuData().empty()) {
        _line_renderer.render(_projection_matrix, _view_matrix);
    }

    // Render selection preview if selecting
    if (_is_selecting) {
        glDisable(GL_DEPTH_TEST);
        auto preview = buildSelectionPreview();
        _scene_renderer.renderPreview(preview, _widget_width, _widget_height);
        glEnable(GL_DEPTH_TEST);
    }
}

void TemporalProjectionOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void TemporalProjectionOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        bool const ctrl_held = (event->modifiers() & Qt::ControlModifier) != 0;
        bool const shift_held = (event->modifiers() & Qt::ShiftModifier) != 0;

        if (ctrl_held) {
            // Selection mode
            std::string const selection_mode = _state ? _state->getSelectionMode().toStdString() : "none";
            
            if (selection_mode == "point") {
                // Point selection is immediate on click
                handleClickSelection(event->pos());
            } else if (selection_mode == "line") {
                // Line selection starts a drag operation
                startLineSelection(event->pos(), shift_held /* remove_mode */);
            }
        } else {
            // Panning mode
            _is_panning = false;
            _click_start_pos = event->pos();
            _last_mouse_pos = event->pos();
        }
    }
    event->accept();
}

void TemporalProjectionOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (_is_selecting) {
        // Update line selection
        updateLineSelection(event->pos());
        event->accept();
        return;
    }

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

void TemporalProjectionOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_selecting) {
            completeLineSelection();
        } else if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void TemporalProjectionOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void TemporalProjectionOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void TemporalProjectionOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void TemporalProjectionOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void TemporalProjectionOpenGLWidget::onDataKeysChanged()
{
    _scene_dirty = true;
    update();
}

void TemporalProjectionOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        return;
    }

    // === Phase 1: Map all data (needed for both rendering and bounding box) ===
    auto const point_keys = _state->getPointDataKeys();
    auto const line_keys = _state->getLineDataKeys();

    // Map all point data across all time frames
    struct PointBatchInfo {
        std::string key;
        std::vector<CorePlotting::MappedElement> mapped;
    };
    std::vector<PointBatchInfo> point_batches;
    point_batches.reserve(point_keys.size());

    for (auto const & key_qstr : point_keys) {
        std::string const key = key_qstr.toStdString();
        auto point_data = _data_manager->getData<PointData>(key);
        if (!point_data) {
            continue;
        }
        auto mapped = CorePlotting::SpatialMapper::mapAllPoints(*point_data);
        if (!mapped.empty()) {
            point_batches.push_back({key, std::move(mapped)});
        }
    }

    // Map all line data across all time frames (only used for bounding box;
    // actual line rendering goes through BatchLineRenderer exclusively)

    // === Phase 2: Compute bounding box from actual data coordinates ===
    // ImageSize may be unset (-1,-1), so always scan the actual mapped data
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    bool has_data = false;

    // Scan points
    for (auto const & pb : point_batches) {
        for (auto const & pt : pb.mapped) {
            min_x = std::min(min_x, pt.x);
            min_y = std::min(min_y, pt.y);
            max_x = std::max(max_x, pt.x);
            max_y = std::max(max_y, pt.y);
            has_data = true;
        }
    }

    // Scan line data from the raw LineData (iterate segments)
    for (auto const & key_qstr : line_keys) {
        std::string const key = key_qstr.toStdString();
        auto line_data = _data_manager->getData<LineData>(key);
        if (!line_data || line_data->getTotalEntryCount() == 0) {
            continue;
        }
        for (auto const elem : line_data->elementsView()) {
            auto const & line = elem.data();
            for (auto const & pt : line) {
                min_x = std::min(min_x, pt.x);
                min_y = std::min(min_y, pt.y);
                max_x = std::max(max_x, pt.x);
                max_y = std::max(max_y, pt.y);
                has_data = true;
            }
        }
    }

    // Fall back to ImageSize if data iteration found nothing useful,
    // or use default 100×100
    if (!has_data) {
        // Try ImageSize as fallback
        for (auto const & key_qstr : point_keys) {
            auto pd = _data_manager->getData<PointData>(key_qstr.toStdString());
            if (pd) {
                auto sz = pd->getImageSize();
                if (sz.width > 0 && sz.height > 0) {
                    min_x = 0.0f; min_y = 0.0f;
                    max_x = std::max(max_x, static_cast<float>(sz.width));
                    max_y = std::max(max_y, static_cast<float>(sz.height));
                    has_data = true;
                }
            }
        }
        for (auto const & key_qstr : line_keys) {
            auto ld = _data_manager->getData<LineData>(key_qstr.toStdString());
            if (ld) {
                auto sz = ld->getImageSize();
                if (sz.width > 0 && sz.height > 0) {
                    min_x = 0.0f; min_y = 0.0f;
                    max_x = std::max(max_x, static_cast<float>(sz.width));
                    max_y = std::max(max_y, static_cast<float>(sz.height));
                    has_data = true;
                }
            }
        }
    }

    if (!has_data) {
        min_x = 0.0f; min_y = 0.0f;
        max_x = 100.0f; max_y = 100.0f;
    }

    // Add a small margin so points at edges aren't clipped
    float const margin_x = (max_x - min_x) * 0.02f;
    float const margin_y = (max_y - min_y) * 0.02f;
    min_x -= margin_x;
    min_y -= margin_y;
    max_x += margin_x;
    max_y += margin_y;

    // === Phase 3: Update view state bounds and reset zoom/pan ===
    auto const & vs = _state->viewState();
    bool const bounds_changed = (vs.x_min != static_cast<double>(min_x)) ||
                                (vs.x_max != static_cast<double>(max_x)) ||
                                (vs.y_min != static_cast<double>(min_y)) ||
                                (vs.y_max != static_cast<double>(max_y));
    _state->setXBounds(static_cast<double>(min_x), static_cast<double>(max_x));
    _state->setYBounds(static_cast<double>(min_y), static_cast<double>(max_y));
    if (bounds_changed) {
        // Reset zoom/pan so the view fits the new data extent
        _state->setXZoom(1.0);
        _state->setYZoom(1.0);
        _state->setPan(0.0, 0.0);
    }

    // === Phase 4: Build SceneBuilder for points only ===
    // Lines are rendered exclusively via BatchLineRenderer (avoids double-rendering)
    CorePlotting::SceneBuilder builder;
    builder.setBounds(BoundingBox{min_x, min_y, max_x, max_y});
    
    float const point_size = _state->getPointSize();

    for (auto const & pb : point_batches) {
        CorePlotting::GlyphStyle style;
        style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
        style.size = point_size;
        style.color = glm::vec4{0.2f, 0.4f, 0.8f, 1.0f}; // Blue points
        
        builder.addGlyphs("points_" + pb.key, pb.mapped, style);
    }

    // Build scene and apply selection highlighting to glyph colors
    _scene = builder.build();

    if (!_selected_entity_ids.empty()) {
        for (auto & glyph_batch : _scene.glyph_batches) {
            for (size_t i = 0; i < glyph_batch.entity_ids.size(); ++i) {
                if (_selected_entity_ids.count(glyph_batch.entity_ids[i])) {
                    glyph_batch.colors[i] = glm::vec4{1.0f, 0.8f, 0.0f, 1.0f}; // Yellow
                }
            }
        }
    }

    _scene_renderer.uploadScene(_scene);

    // === Phase 5: Build LineBatchData for BatchLineRenderer (selectable lines) ===
    // Lines are ONLY rendered via BatchLineRenderer — no SceneBuilder polylines
    CorePlotting::LineBatchData batch;
    batch.canvas_width = static_cast<float>(_widget_width);
    batch.canvas_height = static_cast<float>(_widget_height);

    for (auto const & key_qstr : line_keys) {
        std::string const key = key_qstr.toStdString();
        auto line_data = _data_manager->getData<LineData>(key);
        if (!line_data || line_data->getTotalEntryCount() == 0) {
            continue;
        }

        auto key_batch = CorePlotting::buildLineBatchFromLineData(
            *line_data,
            static_cast<float>(_widget_width),
            static_cast<float>(_widget_height));

        // Merge into the main batch
        std::uint32_t const offset_line_id = batch.numLines();
        std::uint32_t const offset_segment = batch.numSegments();

        batch.segments.insert(batch.segments.end(),
                             key_batch.segments.begin(), key_batch.segments.end());

        for (auto lid : key_batch.line_ids) {
            batch.line_ids.push_back(lid + offset_line_id);
        }

        for (auto info : key_batch.lines) {
            info.first_segment += offset_segment;
            batch.lines.push_back(info);
        }

        batch.visibility_mask.insert(batch.visibility_mask.end(),
                                     key_batch.visibility_mask.begin(),
                                     key_batch.visibility_mask.end());
        batch.selection_mask.insert(batch.selection_mask.end(),
                                    key_batch.selection_mask.begin(),
                                    key_batch.selection_mask.end());
    }

    // Restore selection mask from previous selection (if entities still exist)
    if (!_selected_entity_ids.empty()) {
        for (std::uint32_t i = 0; i < batch.numLines(); ++i) {
            if (_selected_entity_ids.count(batch.lines[i].entity_id)) {
                batch.selection_mask[i] = 1;
            }
        }
    }

    _line_store.upload(batch);
    _line_renderer.syncFromStore();
}

void TemporalProjectionOpenGLWidget::updateMatrices()
{
    _projection_matrix =
        WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);
}

void TemporalProjectionOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y, _widget_width,
        _widget_height);
}

void TemporalProjectionOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(*_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF TemporalProjectionOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(_projection_matrix, _widget_width,
                                               _widget_height, screen_pos);
}

void TemporalProjectionOpenGLWidget::keyReleaseEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Escape && _is_selecting) {
        cancelLineSelection();
        event->accept();
    } else {
        QOpenGLWidget::keyReleaseEvent(event);
    }
}

// =============================================================================
// Selection
// =============================================================================

void TemporalProjectionOpenGLWidget::clearSelection()
{
    _selected_entity_ids.clear();

    // Clear line selection mask (cheap GPU update)
    auto const & batch = _line_store.cpuData();
    if (batch.numLines() > 0) {
        std::vector<std::uint32_t> mask(batch.numLines(), 0);
        _line_store.updateSelectionMask(mask);
        _line_renderer.syncFromStore();
    }

    _scene_dirty = true;  // Need rebuild for point color reset
    update();
    emit entitiesSelected(_selected_entity_ids);
}

glm::vec2 TemporalProjectionOpenGLWidget::screenToNDC(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToNDC(screen_pos, _widget_width,
                                             _widget_height);
}

void TemporalProjectionOpenGLWidget::handleClickSelection(QPoint const & screen_pos)
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
    
    // Perform QuadTree query for point selection
    auto result = tester.queryQuadTree(
        static_cast<float>(world.x()),
        static_cast<float>(world.y()),
        _scene);
    
    if (result.hasHit()) {
        // Toggle selection of the clicked entity
        if (!result.entity_id.has_value()) {
            return; // No EntityId means it's not selectable (e.g. background hit)
        }
        EntityId const entity_id = result.entity_id.value();
        
        bool const shift_held = QApplication::keyboardModifiers() & Qt::ShiftModifier;
        if (shift_held) {
            // Remove mode: deselect the entity
            _selected_entity_ids.erase(entity_id);
        } else {
            // Add mode: toggle the entity
            if (_selected_entity_ids.count(entity_id)) {
                _selected_entity_ids.erase(entity_id);
            } else {
                _selected_entity_ids.insert(entity_id);
            }
        }
        
        _scene_dirty = true;
        update();
        emit entitiesSelected(_selected_entity_ids);
    }
}

void TemporalProjectionOpenGLWidget::startLineSelection(QPoint const & screen_pos, bool remove_mode)
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

void TemporalProjectionOpenGLWidget::updateLineSelection(QPoint const & screen_pos)
{
    _selection_end_ndc = screenToNDC(screen_pos);
    _selection_end_screen = screen_pos;
    update();
}

void TemporalProjectionOpenGLWidget::completeLineSelection()
{
    _is_selecting = false;
    setCursor(Qt::ArrowCursor);

    if (!_intersector) {
        update();
        return;
    }
    std::vector<CorePlotting::LineBatchIndex> const hit_indices =
        WhiskerToolbox::Plots::runLineSelectionIntersection(
            *_intersector, _line_store.cpuData(), _selection_start_ndc,
            _selection_end_ndc, _projection_matrix, _view_matrix);
    applyLineIntersectionResults(hit_indices, _selection_remove_mode);
    update();
}

void TemporalProjectionOpenGLWidget::cancelLineSelection()
{
    _is_selecting = false;
    setCursor(Qt::ArrowCursor);
    update();
}

CorePlotting::Interaction::GlyphPreview
TemporalProjectionOpenGLWidget::buildSelectionPreview() const
{
    auto preview = WhiskerToolbox::Plots::buildLineSelectionPreview(
        _selection_start_screen, _selection_end_screen, _selection_remove_mode);
    // Override stroke color: light background needs dark line (not white)
    if (!_selection_remove_mode) {
        preview.stroke_color = glm::vec4{0.0f, 0.0f, 0.0f, 0.9f}; // Black
    }
    return preview;
}


void TemporalProjectionOpenGLWidget::applyLineIntersectionResults(
    std::vector<CorePlotting::LineBatchIndex> const & hit_indices,
    bool remove)
{
    if (hit_indices.empty() && !remove) {
        return;
    }

    // Extract EntityIDs from the intersected lines
    auto const & batch = _line_store.cpuData();
    
    for (auto const line_idx : hit_indices) {
        if (line_idx < batch.lines.size()) {
            EntityId const entity_id = batch.lines[line_idx].entity_id;
            
            if (remove) {
                _selected_entity_ids.erase(entity_id);
            } else {
                _selected_entity_ids.insert(entity_id);
            }
        }
    }
    
    // Update selection mask on the store (cheap GPU-only update, no full rebuild)
    std::vector<std::uint32_t> mask(batch.numLines(), 0);
    for (std::uint32_t i = 0; i < batch.numLines(); ++i) {
        if (_selected_entity_ids.count(batch.lines[i].entity_id)) {
            mask[i] = 1;
        }
    }
    _line_store.updateSelectionMask(mask);
    _line_renderer.syncFromStore();

    emit entitiesSelected(_selected_entity_ids);
}
