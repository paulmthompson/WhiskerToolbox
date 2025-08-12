#include "ScatterPlotOpenGLWidget_Refactored.hpp"
#include "Visualizers/Points/ScatterPlotVisualization.hpp"
#include "../Common/TooltipManager.hpp"
#include "../Common/PlotInteractionController.hpp"
#include "ScatterPlotViewAdapter.hpp"

#include <QDebug>
#include <algorithm>
#include <limits>

ScatterPlotOpenGLWidget::ScatterPlotOpenGLWidget(QWidget* parent)
    : BasePlotOpenGLWidget(parent)
    , _data_bounds_valid(false),
    _data_bounds({0.0f, 0.0f, 0.0f, 0.0f}) {
    
    qDebug() << "ScatterPlotOpenGLWidget: Created with composition-based design";
}

ScatterPlotOpenGLWidget::~ScatterPlotOpenGLWidget() {
    // Cleanup is handled by smart pointers and base class
}

void ScatterPlotOpenGLWidget::initializeGL() {
    // Call base class initialization first
    BasePlotOpenGLWidget::initializeGL();
    
    // Create interaction controller with ScatterPlot view adapter
    if (!_interaction) {
        _interaction = std::make_unique<PlotInteractionController>(this, std::make_unique<ScatterPlotViewAdapter>(this));
        connect(_interaction.get(), &PlotInteractionController::viewBoundsChanged, this, &ScatterPlotOpenGLWidget::viewBoundsChanged);
        connect(_interaction.get(), &PlotInteractionController::mouseWorldMoved, this, &ScatterPlotOpenGLWidget::mouseWorldMoved);
    }
    
    qDebug() << "ScatterPlotOpenGLWidget::initializeGL completed with interaction controller";
}

void ScatterPlotOpenGLWidget::setScatterData(const std::vector<float>& x_data, 
                                            const std::vector<float>& y_data) {
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

    // Update view
    updateViewMatrices();
    requestThrottledUpdate();
}

void ScatterPlotOpenGLWidget::setAxisLabels(const QString& x_label, const QString& y_label) {
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

void ScatterPlotOpenGLWidget::clearSelection() {
    /*
    if (_selection_manager) {
        _selection_manager->clearSelection();
        requestThrottledUpdate();
        qDebug() << "ScatterPlotOpenGLWidget: Selection cleared";
    }
        */
}

void ScatterPlotOpenGLWidget::renderUI() {
    // TODO: Render axis labels using text rendering system
    // This would be implemented similarly to how SpatialOverlayOpenGLWidget
    // renders its UI elements, but focused just on axes
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

void ScatterPlotOpenGLWidget::onSelectionChanged(size_t total_selected) {
    // Emit the base class signal with scatter plot specific information
    emit selectionChanged(total_selected, "scatter_data", -1);
    requestThrottledUpdate();
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
        false  // Initialize OpenGL resources immediately
    );

    _visualization->setAxisLabels(_x_label, _y_label);
    
    qDebug() << "ScatterPlotOpenGLWidget: Visualization initialized with" 
             << _x_data.size() << "points";
}

void ScatterPlotOpenGLWidget::updateVisualizationData() {
    if (_visualization) {
        _visualization->updateData(_x_data, _y_data);
    }
}

void ScatterPlotOpenGLWidget::setTooltipsEnabled(bool enabled) {
    if (_tooltip_manager) {
        _tooltip_manager->setEnabled(enabled);
    }
}

std::optional<QString> ScatterPlotOpenGLWidget::generateTooltipContent(QPoint const& screen_pos) const {
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
    float tolerance = _point_size * 0.01f; // Scale with point size
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
