#include "OnionSkinViewOpenGLWidget.hpp"

#include "Core/OnionSkinViewState.hpp"
#include "CorePlotting/DataTypes/AlphaCurve.hpp"
#include "CorePlotting/Mappers/MaskContourMapper.hpp"
#include "CorePlotting/Mappers/SpatialMapper_Window.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"
#include "Points/Point_Data.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

OnionSkinViewOpenGLWidget::OnionSkinViewOpenGLWidget(QWidget * parent)
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

OnionSkinViewOpenGLWidget::~OnionSkinViewOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void OnionSkinViewOpenGLWidget::setState(std::shared_ptr<OnionSkinViewState> state)
{
    if (_state) {
        _state->disconnect(this);
    }
    _state = state;
    if (_state) {
        _cached_view_state = _state->viewState();
        connect(_state.get(), &OnionSkinViewState::stateChanged,
                this, &OnionSkinViewOpenGLWidget::onStateChanged);
        connect(_state.get(), &OnionSkinViewState::viewStateChanged,
                this, &OnionSkinViewOpenGLWidget::onViewStateChanged);

        // Data key signals
        connect(_state.get(), &OnionSkinViewState::pointDataKeyAdded,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::pointDataKeyRemoved,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::pointDataKeysCleared,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::lineDataKeyAdded,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::lineDataKeyRemoved,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::lineDataKeysCleared,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::maskDataKeyAdded,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::maskDataKeyRemoved,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);
        connect(_state.get(), &OnionSkinViewState::maskDataKeysCleared,
                this, &OnionSkinViewOpenGLWidget::onDataKeysChanged);

        // Rendering parameter signals
        connect(_state.get(), &OnionSkinViewState::pointSizeChanged,
                this, [this](float) { _scene_dirty = true; update(); });
        connect(_state.get(), &OnionSkinViewState::lineWidthChanged,
                this, [this](float) { _scene_dirty = true; update(); });
        connect(_state.get(), &OnionSkinViewState::highlightCurrentChanged,
                this, [this](bool) { _scene_dirty = true; update(); });

        // Temporal window and alpha signals
        connect(_state.get(), &OnionSkinViewState::windowBehindChanged,
                this, [this](int) { _scene_dirty = true; update(); });
        connect(_state.get(), &OnionSkinViewState::windowAheadChanged,
                this, [this](int) { _scene_dirty = true; update(); });
        connect(_state.get(), &OnionSkinViewState::alphaCurveChanged,
                this, [this](QString const &) { _scene_dirty = true; update(); });
        connect(_state.get(), &OnionSkinViewState::minAlphaChanged,
                this, [this](float) { _scene_dirty = true; update(); });
        connect(_state.get(), &OnionSkinViewState::maxAlphaChanged,
                this, [this](float) { _scene_dirty = true; update(); });

        _scene_dirty = true;
        updateMatrices();
        update();
    }
}

void OnionSkinViewOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

void OnionSkinViewOpenGLWidget::setCurrentTime(int64_t time_index)
{
    if (_current_time != time_index) {
        _current_time = time_index;
        _scene_dirty = true;
        update();
    }
}

void OnionSkinViewOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Disable depth test — we handle draw order explicitly via temporal distance
    // sorting (back-to-front) so that alpha blending works correctly.
    glDisable(GL_DEPTH_TEST);

    if (!_scene_renderer.initialize()) {
        qWarning() << "OnionSkinViewOpenGLWidget: Failed to initialize SceneRenderer";
    }
    _opengl_initialized = true;
}

void OnionSkinViewOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (!_state || !_opengl_initialized) {
        return;
    }

    if (_scene_dirty) {
        rebuildScene();
        _scene_dirty = false;
    }

    _scene_renderer.render(_projection_matrix, _view_matrix);
}

void OnionSkinViewOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void OnionSkinViewOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void OnionSkinViewOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
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

void OnionSkinViewOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        } else {
            // Click (not drag) — brute-force nearest-point on current frame
            QPointF const world_pos = screenToWorld(event->pos());
            // Compute max pick distance in world coordinates
            // Use 15 pixels converted to world space
            float const pixel_radius = 15.0f;
            QPointF const origin_world = screenToWorld(QPoint(0, 0));
            QPointF const offset_world = screenToWorld(QPoint(static_cast<int>(pixel_radius), 0));
            float const world_pick_dist = static_cast<float>(
                std::abs(offset_world.x() - origin_world.x()));
            float const max_dist_sq = world_pick_dist * world_pick_dist;

            auto hit = findNearestPointAtCurrentTime(world_pos, max_dist_sq);
            if (hit.has_value()) {
                emit entitySelected(hit.value());
            }
        }
    }
    event->accept();
}

void OnionSkinViewOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF const world_pos = screenToWorld(event->pos());
        float const pixel_radius = 15.0f;
        QPointF const origin_world = screenToWorld(QPoint(0, 0));
        QPointF const offset_world = screenToWorld(QPoint(static_cast<int>(pixel_radius), 0));
        float const world_pick_dist = static_cast<float>(
            std::abs(offset_world.x() - origin_world.x()));
        float const max_dist_sq = world_pick_dist * world_pick_dist;

        auto hit = findNearestPointAtCurrentTime(world_pos, max_dist_sq);
        if (hit.has_value()) {
            emit entityDoubleClicked(hit.value());
        }
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OnionSkinViewOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void OnionSkinViewOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void OnionSkinViewOpenGLWidget::onViewStateChanged()
{
    if (_state) {
        _cached_view_state = _state->viewState();
    }
    updateMatrices();
    update();
    emit viewBoundsChanged();
}

void OnionSkinViewOpenGLWidget::onDataKeysChanged()
{
    _scene_dirty = true;
    _needs_bounds_update = true;
    update();
}

void OnionSkinViewOpenGLWidget::rebuildScene()
{
    if (!_state || !_data_manager) {
        return;
    }

    // === Read state parameters ===
    auto const point_keys = _state->getPointDataKeys();
    auto const line_keys = _state->getLineDataKeys();
    auto const mask_keys = _state->getMaskDataKeys();
    int const behind = _state->getWindowBehind();
    int const ahead = _state->getWindowAhead();
    float const point_size = _state->getPointSize();
    float const line_width = _state->getLineWidth();
    bool const highlight_current = _state->getHighlightCurrent();
    float const min_alpha = _state->getMinAlpha();
    float const max_alpha = _state->getMaxAlpha();
    CorePlotting::AlphaCurve const alpha_curve =
        CorePlotting::alphaCurveFromString(_state->getAlphaCurve().toStdString());

    // Half-width for alpha computation (max of behind, ahead)
    int const half_width = std::max(behind, ahead);

    TimeFrameIndex const center{_current_time};

    // Current frame highlight colors
    glm::vec4 const current_point_color{1.0f, 0.3f, 0.1f, max_alpha};  // Bright orange-red
    glm::vec4 const current_line_color{1.0f, 0.3f, 0.1f, max_alpha};
    glm::vec4 const base_point_color{0.2f, 0.5f, 0.9f, 1.0f};  // Blue base
    glm::vec4 const base_line_color{0.2f, 0.7f, 0.4f, 1.0f};   // Green base
    glm::vec4 const base_mask_color{0.8f, 0.5f, 0.2f, 1.0f};   // Orange base

    // === Map all windowed data ===
    // Points
    std::vector<CorePlotting::TimedMappedElement> all_points;
    for (auto const & key_qstr : point_keys) {
        auto point_data = _data_manager->getData<PointData>(key_qstr.toStdString());
        if (!point_data) {
            continue;
        }
        auto pts = CorePlotting::SpatialMapper::mapPointsInWindow(
            *point_data, center, behind, ahead);
        all_points.insert(all_points.end(),
                         std::make_move_iterator(pts.begin()),
                         std::make_move_iterator(pts.end()));
    }

    // Lines
    std::vector<CorePlotting::TimedOwningLineView> all_lines;
    for (auto const & key_qstr : line_keys) {
        auto line_data = _data_manager->getData<LineData>(key_qstr.toStdString());
        if (!line_data) {
            continue;
        }
        auto lns = CorePlotting::SpatialMapper::mapLinesInWindow(
            *line_data, center, behind, ahead);
        all_lines.insert(all_lines.end(),
                        std::make_move_iterator(lns.begin()),
                        std::make_move_iterator(lns.end()));
    }

    // Mask contours
    std::vector<CorePlotting::TimedOwningLineView> all_mask_contours;
    for (auto const & key_qstr : mask_keys) {
        auto mask_data = _data_manager->getData<MaskData>(key_qstr.toStdString());
        if (!mask_data) {
            continue;
        }
        auto contours = CorePlotting::SpatialMapper::mapMaskContoursInWindow(
            *mask_data, center, behind, ahead);
        all_mask_contours.insert(all_mask_contours.end(),
                                std::make_move_iterator(contours.begin()),
                                std::make_move_iterator(contours.end()));
    }

    // === Compute bounding box from data ===
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    bool has_data = false;

    for (auto const & pt : all_points) {
        min_x = std::min(min_x, pt.x);
        min_y = std::min(min_y, pt.y);
        max_x = std::max(max_x, pt.x);
        max_y = std::max(max_y, pt.y);
        has_data = true;
    }

    for (auto const & line : all_lines) {
        for (auto const & v : line.vertices()) {
            min_x = std::min(min_x, v.x);
            min_y = std::min(min_y, v.y);
            max_x = std::max(max_x, v.x);
            max_y = std::max(max_y, v.y);
            has_data = true;
        }
    }

    for (auto const & contour : all_mask_contours) {
        for (auto const & v : contour.vertices()) {
            min_x = std::min(min_x, v.x);
            min_y = std::min(min_y, v.y);
            max_x = std::max(max_x, v.x);
            max_y = std::max(max_y, v.y);
            has_data = true;
        }
    }

    // Fallback to ImageSize or default
    if (!has_data) {
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

    // Add margin
    float const margin_x = (max_x - min_x) * 0.02f;
    float const margin_y = (max_y - min_y) * 0.02f;
    min_x -= margin_x;
    min_y -= margin_y;
    max_x += margin_x;
    max_y += margin_y;

    // === Update view state bounds only when data keys change, not on every time change ===
    if (_needs_bounds_update) {
        _state->setXBounds(static_cast<double>(min_x), static_cast<double>(max_x));
        _state->setYBounds(static_cast<double>(min_y), static_cast<double>(max_y));
        _state->setXZoom(1.0);
        _state->setYZoom(1.0);
        _state->setPan(0.0, 0.0);
        _needs_bounds_update = false;
    }

    // === Build scene using SceneBuilder ===
    // Strategy: Sort elements by temporal distance (farthest first = back-to-front)
    // so that alpha blending produces correct results with closer/more-opaque
    // elements drawn on top of farther/more-transparent ones.

    CorePlotting::SceneBuilder builder;
    builder.setBounds(BoundingBox{min_x, min_y, max_x, max_y});

    // --- Group points by temporal distance for sorted rendering ---
    // Collect unique distances and sort descending (farthest first)
    std::vector<int> point_distances;
    for (auto const & pt : all_points) {
        point_distances.push_back(pt.absTemporalDistance());
    }
    std::sort(point_distances.begin(), point_distances.end(), std::greater<>());
    point_distances.erase(
        std::unique(point_distances.begin(), point_distances.end()),
        point_distances.end());

    // Add glyph batches from farthest to nearest
    for (int const dist : point_distances) {
        float const alpha = CorePlotting::computeTemporalAlpha(
            dist, half_width, alpha_curve, min_alpha, max_alpha);

        // Build a batch of points at this temporal distance
        CorePlotting::RenderableGlyphBatch batch;
        batch.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
        batch.model_matrix = glm::mat4(1.0f);

        bool const is_current = (dist == 0);
        float const this_size = (is_current && highlight_current)
                                    ? point_size * 1.5f
                                    : point_size;
        batch.size = this_size;

        for (auto const & pt : all_points) {
            if (pt.absTemporalDistance() != dist) {
                continue;
            }
            batch.positions.emplace_back(pt.x, pt.y);
            batch.entity_ids.push_back(pt.entity_id);

            glm::vec4 color = (is_current && highlight_current)
                                  ? current_point_color
                                  : glm::vec4{base_point_color.r, base_point_color.g,
                                              base_point_color.b, alpha};
            batch.colors.push_back(color);
        }

        if (!batch.positions.empty()) {
            builder.addGlyphBatch(std::move(batch));
        }
    }

    // --- Group lines by temporal distance for sorted rendering ---
    // Sort lines: farthest temporal distance drawn first (ascending abs distance
    // means we process in reverse order → draw most transparent first)
    // We use indices to avoid moving the line data twice
    std::vector<size_t> line_indices(all_lines.size());
    std::iota(line_indices.begin(), line_indices.end(), 0);
    std::sort(line_indices.begin(), line_indices.end(),
              [&all_lines](size_t a, size_t b) {
                  return all_lines[a].absTemporalDistance() >
                         all_lines[b].absTemporalDistance();
              });

    for (size_t const idx : line_indices) {
        auto const & line = all_lines[idx];
        int const dist = line.absTemporalDistance();
        float const alpha = CorePlotting::computeTemporalAlpha(
            dist, half_width, alpha_curve, min_alpha, max_alpha);

        bool const is_current = (dist == 0);

        CorePlotting::RenderablePolyLineBatch batch;
        batch.thickness = (is_current && highlight_current)
                              ? line_width * 1.5f
                              : line_width;
        batch.model_matrix = glm::mat4(1.0f);
        batch.entity_ids.push_back(line.entity_id);

        glm::vec4 color = (is_current && highlight_current)
                              ? current_line_color
                              : glm::vec4{base_line_color.r, base_line_color.g,
                                          base_line_color.b, alpha};
        batch.colors.push_back(color);

        int32_t vertex_count = 0;
        batch.line_start_indices.push_back(0);

        for (auto const & v : line.vertices()) {
            batch.vertices.push_back(v.x);
            batch.vertices.push_back(v.y);
            ++vertex_count;
        }
        batch.line_vertex_counts.push_back(vertex_count);

        if (vertex_count > 0) {
            builder.addPolyLineBatch(std::move(batch));
        }
    }

    // --- Mask contours (same pattern as lines) ---
    std::vector<size_t> mask_indices(all_mask_contours.size());
    std::iota(mask_indices.begin(), mask_indices.end(), 0);
    std::sort(mask_indices.begin(), mask_indices.end(),
              [&all_mask_contours](size_t a, size_t b) {
                  return all_mask_contours[a].absTemporalDistance() >
                         all_mask_contours[b].absTemporalDistance();
              });

    for (size_t const idx : mask_indices) {
        auto const & contour = all_mask_contours[idx];
        int const dist = contour.absTemporalDistance();
        float const alpha = CorePlotting::computeTemporalAlpha(
            dist, half_width, alpha_curve, min_alpha, max_alpha);

        bool const is_current = (dist == 0);

        CorePlotting::RenderablePolyLineBatch batch;
        batch.thickness = (is_current && highlight_current)
                              ? line_width * 1.5f
                              : line_width;
        batch.model_matrix = glm::mat4(1.0f);
        batch.entity_ids.push_back(contour.entity_id);

        glm::vec4 color = (is_current && highlight_current)
                              ? current_line_color
                              : glm::vec4{base_mask_color.r, base_mask_color.g,
                                          base_mask_color.b, alpha};
        batch.colors.push_back(color);

        int32_t vertex_count = 0;
        batch.line_start_indices.push_back(0);

        for (auto const & v : contour.vertices()) {
            batch.vertices.push_back(v.x);
            batch.vertices.push_back(v.y);
            ++vertex_count;
        }
        batch.line_vertex_counts.push_back(vertex_count);

        if (vertex_count > 0) {
            builder.addPolyLineBatch(std::move(batch));
        }
    }

    _scene = builder.build();
    _scene_renderer.uploadScene(_scene);

    // Cache current-frame points for click selection (P2.5)
    _current_frame_points.clear();
    for (auto const & pt : all_points) {
        if (pt.temporal_distance == 0) {
            _current_frame_points.push_back(pt);
        }
    }
}

void OnionSkinViewOpenGLWidget::updateMatrices()
{
    // Inverted Y-axis projection: Y increases downward (image coordinates).
    // This matches MediaWidget convention where Y=0 is at the top.
    auto const & vs = _cached_view_state;
    float const x_range = static_cast<float>(vs.x_max - vs.x_min);
    float const x_center = static_cast<float>(vs.x_min + vs.x_max) / 2.0f;
    float const y_range = static_cast<float>(vs.y_max - vs.y_min);
    float const y_center = static_cast<float>(vs.y_min + vs.y_max) / 2.0f;

    float const zoomed_x = x_range / static_cast<float>(vs.x_zoom);
    float const zoomed_y = y_range / static_cast<float>(vs.y_zoom);
    float const pan_x = static_cast<float>(vs.x_pan);
    float const pan_y = static_cast<float>(vs.y_pan);

    float const left = x_center - zoomed_x / 2.0f + pan_x;
    float const right = x_center + zoomed_x / 2.0f + pan_x;
    // Swap bottom/top so that small Y values are at the top of the screen
    float const bottom = y_center + zoomed_y / 2.0f + pan_y;
    float const top = y_center - zoomed_y / 2.0f + pan_y;

    _projection_matrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    _view_matrix = glm::mat4(1.0f);
}

void OnionSkinViewOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    // Negate delta_y because the Y-axis is inverted (image coordinates:
    // screen-down corresponds to increasing world Y, but the standard
    // panning helper assumes screen-down corresponds to decreasing world Y).
    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, -delta_y, _widget_width,
        _widget_height);
}

void OnionSkinViewOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(*_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF OnionSkinViewOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(_projection_matrix, _widget_width,
                                               _widget_height, screen_pos);
}

std::optional<EntityId> OnionSkinViewOpenGLWidget::findNearestPointAtCurrentTime(
    QPointF const & world_pos, float max_distance_sq) const
{
    float best_dist_sq = max_distance_sq;
    std::optional<EntityId> best_id;

    float const wx = static_cast<float>(world_pos.x());
    float const wy = static_cast<float>(world_pos.y());

    for (auto const & pt : _current_frame_points) {
        float const dx = pt.x - wx;
        float const dy = pt.y - wy;
        float const dist_sq = dx * dx + dy * dy;
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_id = pt.entity_id;
        }
    }

    return best_id;
}
