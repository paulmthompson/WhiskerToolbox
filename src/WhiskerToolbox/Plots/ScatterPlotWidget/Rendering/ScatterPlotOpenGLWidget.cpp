#include "ScatterPlotOpenGLWidget.hpp"

#include "Core/BuildScatterPoints.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Core/SourceCompatibility.hpp"
#include "CorePlotting/Interaction/HitTestResult.hpp"
#include "CorePlotting/Mappers/MappedElement.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/Common/PlotInteractionHelpers.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
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

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget()
{
    makeCurrent();
    _scene_renderer.cleanup();
    doneCurrent();
}

void ScatterPlotOpenGLWidget::setState(std::shared_ptr<ScatterPlotState> state)
{
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
        _scene_dirty = true;
        updateMatrices();
        update();
    }
}

void ScatterPlotOpenGLWidget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    _scene_dirty = true;
    update();
}

void ScatterPlotOpenGLWidget::initializeGL()
{
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
    _opengl_initialized = true;
    updateMatrices();
}

void ScatterPlotOpenGLWidget::paintGL()
{
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

void ScatterPlotOpenGLWidget::resizeGL(int w, int h)
{
    _widget_width = std::max(1, w);
    _widget_height = std::max(1, h);
    glViewport(0, 0, _widget_width, _widget_height);
    updateMatrices();
}

void ScatterPlotOpenGLWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        _is_panning = false;
        _click_start_pos = event->pos();
        _last_mouse_pos = event->pos();
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseMoveEvent(QMouseEvent * event)
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

void ScatterPlotOpenGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        if (_is_panning) {
            _is_panning = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() != Qt::LeftButton || !_state || !_data_manager || _scatter_data.empty()) {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    QPointF const world = screenToWorld(event->pos());

    // Convert pixel tolerance to world units (max of X and Y to handle non-uniform aspect)
    float const world_per_pixel_x = static_cast<float>(
        (_cached_view_state.x_max - _cached_view_state.x_min)
        / (_widget_width * _cached_view_state.x_zoom));
    float const world_per_pixel_y = static_cast<float>(
        (_cached_view_state.y_max - _cached_view_state.y_min)
        / (_widget_height * _cached_view_state.y_zoom));
    float const world_tolerance = 8.0f * std::max(world_per_pixel_x, world_per_pixel_y);

    CorePlotting::HitTestConfig config;
    config.point_tolerance = world_tolerance;
    config.prioritize_discrete = true;
    _hit_tester.setConfig(config);

    auto const result = _hit_tester.queryQuadTree(
        static_cast<float>(world.x()),
        static_cast<float>(world.y()),
        _scene);

    if (result.hasHit() && result.entity_id.has_value()) {
        auto const idx = static_cast<std::size_t>(result.entity_id->id);
        if (idx < _scatter_data.size()) {
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
        }
    }
    event->accept();
}

void ScatterPlotOpenGLWidget::wheelEvent(QWheelEvent * event)
{
    float const delta = event->angleDelta().y() / 120.0f;
    bool const y_only = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const both_axes = (event->modifiers() & Qt::ControlModifier) != 0;
    handleZoom(delta, y_only, both_axes);
    event->accept();
}

void ScatterPlotOpenGLWidget::onStateChanged()
{
    _scene_dirty = true;
    update();
}

void ScatterPlotOpenGLWidget::onViewStateChanged()
{
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

void ScatterPlotOpenGLWidget::updateMatrices()
{
    _projection_matrix =
        WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
    _view_matrix = glm::mat4(1.0f);
}

void ScatterPlotOpenGLWidget::handlePanning(int delta_x, int delta_y)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handlePanning(
        *_state, _cached_view_state, delta_x, delta_y, _widget_width,
        _widget_height);
}

void ScatterPlotOpenGLWidget::handleZoom(float delta, bool y_only, bool both_axes)
{
    if (!_state) {
        return;
    }
    WhiskerToolbox::Plots::handleZoom(
        *_state, _cached_view_state, delta, y_only, both_axes);
}

QPointF ScatterPlotOpenGLWidget::screenToWorld(QPoint const & screen_pos) const
{
    return WhiskerToolbox::Plots::screenToWorld(
        _projection_matrix, _widget_width, _widget_height, screen_pos);
}

void ScatterPlotOpenGLWidget::rebuildScene()
{
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
    bool const is_default_bounds = (vs.x_min == 0.0 && vs.x_max == 100.0
                                    && vs.y_min == 0.0 && vs.y_max == 100.0);
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

    // Add scatter points as glyphs
    std::vector<CorePlotting::MappedElement> elements;
    elements.reserve(_scatter_data.size());
    for (std::size_t i = 0; i < _scatter_data.size(); ++i) {
        elements.push_back(CorePlotting::MappedElement{
            _scatter_data.x_values[i],
            _scatter_data.y_values[i],
            EntityId{static_cast<uint64_t>(i)}
        });
    }

    CorePlotting::GlyphStyle point_style;
    point_style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
    point_style.size = 5.0f;
    point_style.color = glm::vec4(0.2f, 0.6f, 1.0f, 0.8f);  // Blue with slight transparency

    builder.addGlyphs("scatter_points", elements, point_style);

    // Add highlight glyph for the most recently navigated-to point (Phase 2.3)
    if (_navigated_index.has_value() && *_navigated_index < elements.size()) {
        CorePlotting::GlyphStyle highlight_style;
        highlight_style.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Circle;
        highlight_style.size = 9.0f;
        highlight_style.color = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f);  // Yellow highlight

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
    _scene_renderer.uploadScene(_scene);
}
