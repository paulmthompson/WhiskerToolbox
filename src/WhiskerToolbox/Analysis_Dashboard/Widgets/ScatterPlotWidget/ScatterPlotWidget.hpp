#ifndef SCATTERPLOTWIDGET_HPP
#define SCATTERPLOTWIDGET_HPP

#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"

#include <memory>

class ScatterPlotOpenGLWidget;
class QGraphicsProxyWidget;
class QGraphicsSceneMouseEvent;

/**
 * @brief Scatter plot widget for visualizing X vs Y data
 * 
 * This widget displays scatter plots using OpenGL for efficient rendering
 * of large datasets with hover, selection, and grouping capabilities.
 */
class ScatterPlotWidget : public AbstractPlotWidget {
    Q_OBJECT

public:
    explicit ScatterPlotWidget(QGraphicsItem * parent = nullptr);
    ~ScatterPlotWidget() override;

    QString getPlotType() const override;

    /**
     * @brief Set the scatter plot data
     * @param x_data Vector of X coordinates
     * @param y_data Vector of Y coordinates
     */
    void setScatterData(std::vector<float> const & x_data, 
                       std::vector<float> const & y_data);

    /**
     * @brief Set axis labels for display
     * @param x_label Label for X axis
     * @param y_label Label for Y axis
     */
    void setAxisLabels(QString const & x_label, QString const & y_label);

    /**
     * @brief Set the group manager for color coding points
     * @param group_manager The group manager instance
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Set the point size for rendering
     * @param point_size Point size in pixels
     */
    void setPointSize(float point_size);

    /**
     * @brief Get current point size
     */
    float getPointSize() const;

    /**
     * @brief Set zoom level (1.0 = default, >1.0 = zoomed in, <1.0 = zoomed out)
     * @param zoom_level The zoom level
     */
    void setZoomLevel(float zoom_level);

    /**
     * @brief Get current zoom level
     */
    float getZoomLevel() const;

    /**
     * @brief Set pan offset
     * @param offset_x X offset in normalized coordinates
     * @param offset_y Y offset in normalized coordinates
     */
    void setPanOffset(float offset_x, float offset_y);

    /**
     * @brief Get current pan offset
     */
    QVector2D getPanOffset() const;

    /**
     * @brief Enable or disable tooltips
     * @param enabled Whether tooltips should be enabled
     */
    void setTooltipsEnabled(bool enabled);

    /**
     * @brief Get current tooltip enabled state
     */
    bool getTooltipsEnabled() const;

    /**
     * @brief Get access to the OpenGL widget for advanced configuration
     * @return Pointer to the OpenGL widget
     */
    ScatterPlotOpenGLWidget * getOpenGLWidget() const { return _opengl_widget; }

signals:
    /**
     * @brief Emitted when a point is clicked
     * @param point_index The index of the clicked point
     */
    void pointClicked(size_t point_index);

    /**
     * @brief Emitted when rendering properties change
     */
    void renderingPropertiesChanged();

protected:
    void paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget = nullptr) override;
    void resizeEvent(QGraphicsSceneResizeEvent * event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;

private slots:
    /**
     * @brief Update the visualization when data changes
     */
    void updateVisualization();

private:
    ScatterPlotOpenGLWidget * _opengl_widget;
    QGraphicsProxyWidget * _proxy_widget;

    /**
     * @brief Setup the OpenGL widget and proxy
     */
    void setupOpenGLWidget();

    /**
     * @brief Connect OpenGL widget signals (called after initialization)
     */
    void connectOpenGLSignals();
};

#endif// SCATTERPLOTWIDGET_HPP