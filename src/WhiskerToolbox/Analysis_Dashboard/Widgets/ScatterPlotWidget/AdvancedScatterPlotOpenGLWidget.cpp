#include "AdvancedScatterPlotOpenGLWidget.hpp"

#include <QDebug>
#include <QOpenGLFunctions_4_3_Core>

#include <algorithm>

AdvancedScatterPlotOpenGLWidget::AdvancedScatterPlotOpenGLWidget(QWidget* parent)
    : BasePlotOpenGLWidget(parent)
    , _data_bounds_valid(false) {
    
    qDebug() << "AdvancedScatterPlotOpenGLWidget: Created with OpenGL 4.3 requirements";
}

AdvancedScatterPlotOpenGLWidget::~AdvancedScatterPlotOpenGLWidget() {
    // Cleanup is handled by smart pointers and base class
}

void AdvancedScatterPlotOpenGLWidget::initializeGL() {
    // Call base class initialization first
    BasePlotOpenGLWidget::initializeGL();
    
    // Check if we actually got OpenGL 4.3 support
    if (!checkAdvancedFeatureSupport()) {
        qWarning() << "AdvancedScatterPlotOpenGLWidget: Advanced features not supported, falling back to basic rendering";
        _compute_clustering_enabled = false;
        _instancing_enabled = false;
        return;
    }
    
    // Initialize advanced OpenGL 4.3 features
    initializeAdvancedFeatures();
    
    qDebug() << "AdvancedScatterPlotOpenGLWidget: Advanced OpenGL 4.3 features initialized";
}

void AdvancedScatterPlotOpenGLWidget::setScatterData(const std::vector<float>& x_data, 
                                                    const std::vector<float>& y_data) {
    qDebug() << "AdvancedScatterPlotOpenGLWidget::setScatterData called with"
             << x_data.size() << "x points and" << y_data.size() << "y points";

    if (x_data.size() != y_data.size()) {
        qWarning() << "AdvancedScatterPlotOpenGLWidget::setScatterData: X and Y data vectors must have the same size";
        return;
    }

    if (x_data.empty()) {
        qWarning() << "AdvancedScatterPlotOpenGLWidget::setScatterData: Data vectors are empty";
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
        // TODO: Initialize AdvancedScatterPlotVisualization
        doneCurrent();
    }

    // Update view
    updateViewMatrices();
    requestThrottledUpdate();
}

void AdvancedScatterPlotOpenGLWidget::enableComputeShaderClustering(bool enable) {
    if (!checkAdvancedFeatureSupport()) {
        qWarning() << "AdvancedScatterPlotOpenGLWidget: Compute shader clustering requires OpenGL 4.3";
        return;
    }
    
    _compute_clustering_enabled = enable;
    qDebug() << "AdvancedScatterPlotOpenGLWidget: Compute shader clustering" << (enable ? "enabled" : "disabled");
    
    // TODO: Update visualization with clustering settings
    requestThrottledUpdate();
}

void AdvancedScatterPlotOpenGLWidget::setInstancingEnabled(bool enable) {
    if (!checkAdvancedFeatureSupport()) {
        qWarning() << "AdvancedScatterPlotOpenGLWidget: Instancing requires OpenGL 4.3";
        return;
    }
    
    _instancing_enabled = enable;
    qDebug() << "AdvancedScatterPlotOpenGLWidget: Instancing" << (enable ? "enabled" : "disabled");
    
    // TODO: Update visualization with instancing settings
    requestThrottledUpdate();
}

void AdvancedScatterPlotOpenGLWidget::renderData() {
    if (!_visualization) {
        return;
    }

    auto context = createRenderingContext();
    
    // TODO: Render using advanced visualization
    // QMatrix4x4 mvp_matrix = context.projection_matrix * context.view_matrix * context.model_matrix;
    // _visualization->render(mvp_matrix, _point_size);
    
    qDebug() << "AdvancedScatterPlotOpenGLWidget: Rendered with advanced features"
             << "Clustering:" << _compute_clustering_enabled 
             << "Instancing:" << _instancing_enabled;
}

void AdvancedScatterPlotOpenGLWidget::calculateDataBounds() {
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
}

BoundingBox AdvancedScatterPlotOpenGLWidget::getDataBounds() const {
    return _data_bounds_valid ? _data_bounds : BoundingBox();
}

void AdvancedScatterPlotOpenGLWidget::onSelectionChanged(size_t total_selected) {
    // Emit the base class signal with scatter plot specific information
    emit selectionChanged(total_selected, "advanced_scatter_data", -1);
    requestThrottledUpdate();
}

bool AdvancedScatterPlotOpenGLWidget::checkAdvancedFeatureSupport() {
    if (!context() || !context()->isValid()) {
        return false;
    }
    
    auto fmt = format();
    return (fmt.majorVersion() > 4) || 
           (fmt.majorVersion() == 4 && fmt.minorVersion() >= 3);
}

void AdvancedScatterPlotOpenGLWidget::initializeAdvancedFeatures() {
    // Initialize OpenGL 4.3 specific features
    
    // Example: Query compute shader limits
    GLint max_compute_work_group_count[3];
    GLint max_compute_work_group_size[3];
    GLint max_compute_work_group_invocations;
    
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_compute_work_group_count[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &max_compute_work_group_count[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_compute_work_group_count[2]);
    
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &max_compute_work_group_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &max_compute_work_group_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &max_compute_work_group_size[2]);
    
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);
    
    qDebug() << "AdvancedScatterPlotOpenGLWidget: Compute shader limits:";
    qDebug() << "  Max work group count:" << max_compute_work_group_count[0] << max_compute_work_group_count[1] << max_compute_work_group_count[2];
    qDebug() << "  Max work group size:" << max_compute_work_group_size[0] << max_compute_work_group_size[1] << max_compute_work_group_size[2];
    qDebug() << "  Max work group invocations:" << max_compute_work_group_invocations;
    
    // TODO: Initialize compute shaders, SSBOs, etc.
}
