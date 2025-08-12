#pragma once

#include "../Common/BasePlotOpenGLWidget.hpp"
#include "../Common/PlotSelectionAdapters.hpp"
#include <vector>
#include <memory>

class ScatterPlotVisualization;
class ScatterPlotViewAdapter; // adapter (friend)

/**
 * Refactored ScatterPlotOpenGLWidget using composition-based design
 * Inherits common functionality from BasePlotOpenGLWidget
 */
class ScatterPlotOpenGLWidget : public BasePlotOpenGLWidget {
    Q_OBJECT

public:
    explicit ScatterPlotOpenGLWidget(QWidget* parent = nullptr);
    ~ScatterPlotOpenGLWidget() override;

    // Data management
    void setScatterData(const std::vector<float>& x_data, const std::vector<float>& y_data);
    void setAxisLabels(const QString& x_label, const QString& y_label);

    // Query methods
    size_t getDataPointCount() const;
    std::pair<float, float> getDataPoint(size_t index) const;

    /**
     * @brief Enable or disable tooltips
     * @param enabled Whether tooltips should be enabled
     */
    void setTooltipsEnabled(bool enabled);

signals:

    /**
     * @brief Emitted when a point is clicked
     * @param point_index The index of the clicked point
     */
    void pointClicked(size_t point_index);

protected:
    // BasePlotOpenGLWidget interface implementation
    void renderData() override;
    void calculateDataBounds() override;
    BoundingBox getDataBounds() const override;
    std::unique_ptr<SelectionManager> createSelectionManager() override;

    // OpenGL lifecycle
    void initializeGL() override;

    // Optional overrides
    void renderUI() override;  // For axis labels
    std::optional<QString> generateTooltipContent(QPoint const& screen_pos) const override;

private slots:
    void onSelectionChanged(size_t total_selected);

private:
    // Data storage
    std::vector<float> _x_data;
    std::vector<float> _y_data;
    BoundingBox _data_bounds;
    bool _data_bounds_valid = false;

    // Visualization
    std::unique_ptr<ScatterPlotVisualization> _visualization;
    
    // Axis labels
    QString _x_label;
    QString _y_label;

    friend class ScatterPlotViewAdapter;

    void initializeVisualization();
    void updateVisualizationData();
};
