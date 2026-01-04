#include "ScatterPlotOpenGLWidget_Refactored.hpp"

#include "Visualizers/Points/ScatterPlotVisualization.hpp"
#include "Widgets/Common/GenericViewAdapter.hpp"
#include "Widgets/Common/PlotInteractionController.hpp"
#include "Widgets/Common/TooltipManager.hpp"

#include <QDebug>

#include <algorithm>
#include <limits>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget * parent)
    : BasePlotOpenGLWidget(parent),
      _data_bounds_valid(false),
      _data_bounds({0.0f, 0.0f, 0.0f, 0.0f}) {

        _selection_callback = [this]() {
            makeSelection();
        };
}

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget() = default;

void ScatterPlotOpenGLWidget::initializeGL() {

    BasePlotOpenGLWidget::initializeGL();

    if (!_interaction) {
        _interaction = std::make_unique<PlotInteractionController>(this, std::make_unique<GenericViewAdapter>(this));
        connect(_interaction.get(), &PlotInteractionController::viewBoundsChanged, this, &ScatterPlotOpenGLWidget::viewBoundsChanged);
        connect(_interaction.get(), &PlotInteractionController::mouseWorldMoved, this, &ScatterPlotOpenGLWidget::mouseWorldMoved);
    }

    initializeVisualization();
}

void ScatterPlotOpenGLWidget::initializeVisualization() {
    if (_x_data.empty() || _y_data.empty()) {
        return;
    }

    _visualization = std::make_unique<ScatterPlotVisualization>(
            "scatter_data",
            _x_data,
            _y_data,
            _group_manager,
            false// Initialize OpenGL resources immediately
    );

    _visualization->setAxisLabels(_x_label, _y_label);

    qDebug() << "ScatterPlotOpenGLWidget: Visualization initialized with"
             << _x_data.size() << "points";
}

// ========== Data ==========

void ScatterPlotOpenGLWidget::setScatterData(std::vector<float> const & x_data,
                                             std::vector<float> const & y_data) {
    qDebug() << "ScatterPlotOpenGLWidget::setScatterData called with"
             << x_data.size() << "x points and" << y_data.size() << "y points";

    if (x_data.size() != y_data.size()) {
        qWarning() << "ScatterPlotOpenGLWidget::setScatterData: X and Y data vectors must have the same size";
        return;
    }

    if (x_data.empty()) {
        qWarning() << "ScatterPlotOpenGLWidget::setScatterData: Data vectors are empty";
        return;
    }

    // Store data
    _x_data = x_data;
    _y_data = y_data;

    // Calculate bounds
    calculateDataBounds();

    // Initialize or update visualization
    if (_opengl_resources_initialized) {
        makeCurrent();
        initializeVisualization();
        doneCurrent();
    }

    updateViewMatrices();
    requestThrottledUpdate();
}

void ScatterPlotOpenGLWidget::setAxisLabels(QString const & x_label, QString const & y_label) {
    _x_label = x_label;
    _y_label = y_label;

    if (_visualization) {
        _visualization->setAxisLabels(x_label, y_label);
    }

    requestThrottledUpdate();
}

void ScatterPlotOpenGLWidget::renderData() {
    if (!_visualization) {
        return;
    }

    auto context = createRenderingContext();

    // Calculate MVP matrix from the rendering context
    QMatrix4x4 mvp_matrix = context.projection_matrix * context.view_matrix * context.model_matrix;

    _visualization->render(mvp_matrix, _point_size);
}

void ScatterPlotOpenGLWidget::calculateDataBounds() {
    if (_x_data.empty() || _y_data.empty()) {
        _data_bounds_valid = false;
        return;
    }

    auto [min_x_it, max_x_it] = std::minmax_element(_x_data.begin(), _x_data.end());
    auto [min_y_it, max_y_it] = std::minmax_element(_y_data.begin(), _y_data.end());

    float min_x = *min_x_it;
    float max_x = *max_x_it;
    float min_y = *min_y_it;
    float max_y = *max_y_it;

    // Add padding
    float padding_x = (max_x - min_x) * 0.1f;
    float padding_y = (max_y - min_y) * 0.1f;

    _data_bounds = BoundingBox(min_x - padding_x, min_y - padding_y,
                               max_x + padding_x, max_y + padding_y);
    _data_bounds_valid = true;

    qDebug() << "ScatterPlotOpenGLWidget: Data bounds calculated:"
             << _data_bounds.min_x << "," << _data_bounds.min_y << "to"
             << _data_bounds.max_x << "," << _data_bounds.max_y;
}

BoundingBox ScatterPlotOpenGLWidget::getDataBounds() const {
    return _data_bounds_valid ? _data_bounds : BoundingBox(0.0f, 0.0f, 0.0f, 0.0f);
}

void ScatterPlotOpenGLWidget::renderUI() {
    // TODO: Render axis labels using text rendering system
    // This would be implemented similarly to how SpatialOverlayOpenGLWidget
    // renders its UI elements, but focused just on axes
}

void ScatterPlotOpenGLWidget::doSetGroupManager(GroupManager * group_manager) {
    if (_visualization) {
        _visualization->setGroupManager(group_manager);
    }
}

size_t ScatterPlotOpenGLWidget::getDataPointCount() const {
    return _x_data.size();
}

std::pair<float, float> ScatterPlotOpenGLWidget::getDataPoint(size_t index) const {
    if (index >= _x_data.size()) {
        return {0.0f, 0.0f};
    }
    return {_x_data[index], _y_data[index]};
}



void ScatterPlotOpenGLWidget::updateVisualizationData() {
    if (_visualization) {
        _visualization->updateData(_x_data, _y_data);
    }
}

// ========== Tooltips ==========

void ScatterPlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    if (_tooltip_manager) {
        _tooltip_manager->setEnabled(enabled);
    }
}

std::optional<QString> ScatterPlotOpenGLWidget::generateTooltipContent(QPoint const & screen_pos) const {
    if (_x_data.empty() || _y_data.empty() || !_tooltips_enabled) {
        return std::nullopt;
    }

    // Convert screen position to world coordinates
    auto world_pos = screenToWorld(screen_pos);
    float world_x = world_pos.x();
    float world_y = world_pos.y();

    // Find closest point within reasonable distance
    float min_distance_sq = std::numeric_limits<float>::max();
    size_t closest_index = 0;
    bool found_point = false;

    // Define tolerance in world coordinates (could be made configurable)
    float tolerance = _point_size * 0.01f;// Scale with point size
    float tolerance_sq = tolerance * tolerance;

    for (size_t i = 0; i < _x_data.size(); ++i) {
        float dx = _x_data[i] - world_x;
        float dy = _y_data[i] - world_y;
        float distance_sq = dx * dx + dy * dy;

        if (distance_sq < tolerance_sq && distance_sq < min_distance_sq) {
            min_distance_sq = distance_sq;
            closest_index = i;
            found_point = true;
        }
    }

    if (!found_point) {
        return std::nullopt;
    }

    // Generate tooltip text
    QString tooltip = QString("Point %1\n%2: %3\n%4: %5")
                              .arg(closest_index)
                              .arg(_x_label.isEmpty() ? "X" : _x_label)
                              .arg(_x_data[closest_index], 0, 'f', 3)
                              .arg(_y_label.isEmpty() ? "Y" : _y_label)
                              .arg(_y_data[closest_index], 0, 'f', 3);

    return tooltip;
}

//========== Selection ==========

void ScatterPlotOpenGLWidget::makeSelection() {
    auto context = createRenderingContext();

    if (!_selection_handler) {
        return;
    }

    // Determine if we should clear selection. For point selection, there is no region
    // concept, so we should NOT clear. Only clear when selection mode is None or when
    // region-based handlers have no active region.
    bool should_clear = false;
    if (_selection_mode == SelectionMode::None) {
        should_clear = true;
    } else {
        should_clear = false;
    }

    if (should_clear) {
        clearSelection();
        return;
    }

    if (_selection_handler) {
        _visualization->applySelection(*_selection_handler);
    }

    // Emit selection changed signal with updated counts
    size_t total_selected = getTotalSelectedPoints();
    emit selectionChanged(total_selected, QString(), 0);

    requestThrottledUpdate();
}

size_t ScatterPlotOpenGLWidget::getTotalSelectedPoints() const {
    return _visualization->m_selected_points.size();
}

void ScatterPlotOpenGLWidget::setSelectionMode(SelectionMode mode) {
    auto old_mode = _selection_mode;
    BasePlotOpenGLWidget::setSelectionMode(mode);
    if (old_mode != mode) {
        //updateContextMenuState();
    }
}

void ScatterPlotOpenGLWidget::onSelectionChanged(size_t total_selected) {
    // Emit the base class signal with scatter plot specific information
    emit selectionChanged(total_selected, "scatter_data", -1);
    requestThrottledUpdate();
}

void ScatterPlotOpenGLWidget::clearSelection() {
    
    bool had_selection = false;

    if (!_visualization->m_selected_points.empty()) {
        _visualization->clearSelection();
        had_selection = true;
    }

    if (had_selection) {
        size_t total_selected = getTotalSelectedPoints();
        emit selectionChanged(total_selected, QString(), 0);
        requestThrottledUpdate();

        qDebug() << "ScatterPlotOpenGLWidget: Selection cleared";
    }
}