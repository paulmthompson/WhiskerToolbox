#pragma once

#include "../Common/BasePlotOpenGLWidget.hpp"
#include <vector>
#include <memory>

class AdvancedScatterPlotVisualization;

/**
 * Example of a scatter plot widget that requires OpenGL 4.3 for advanced features
 * Demonstrates how to override OpenGL context requirements
 */
class AdvancedScatterPlotOpenGLWidget : public BasePlotOpenGLWidget {
    Q_OBJECT

public:
    explicit AdvancedScatterPlotOpenGLWidget(QWidget* parent = nullptr);
    ~AdvancedScatterPlotOpenGLWidget() override;

    // Data management - same as regular scatter plot
    void setScatterData(const std::vector<float>& x_data, const std::vector<float>& y_data);
    void setAxisLabels(const QString& x_label, const QString& y_label);

    // Advanced features that require OpenGL 4.3
    void enableComputeShaderClustering(bool enable);
    void setInstancingEnabled(bool enable);
    

protected:
    // Override OpenGL requirements for advanced features
    std::pair<int, int> getRequiredOpenGLVersion() const override { return {4, 3}; }
    int getRequiredSamples() const override { return 8; } // Higher quality antialiasing

    // BasePlotOpenGLWidget interface implementation
    void renderData() override;
    void calculateDataBounds() override;
    BoundingBox getDataBounds() const override;

    // Advanced initialization
    void initializeGL() override;

private slots:
    void onSelectionChanged(size_t total_selected);

private:
    // Data storage
    std::vector<float> _x_data;
    std::vector<float> _y_data;
    BoundingBox _data_bounds;
    bool _data_bounds_valid = false;

    // Advanced visualization with compute shaders
    std::unique_ptr<AdvancedScatterPlotVisualization> _visualization;
    
    // Advanced features
    bool _compute_clustering_enabled = false;
    bool _instancing_enabled = false;
    
    // Axis labels
    QString _x_label;
    QString _y_label;

    void initializeAdvancedFeatures();
    bool checkAdvancedFeatureSupport();
};
