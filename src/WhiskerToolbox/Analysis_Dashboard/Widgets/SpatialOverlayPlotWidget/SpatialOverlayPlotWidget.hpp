#ifndef SPATIALOVERLAYPLOTWIDGET_HPP
#define SPATIALOVERLAYPLOTWIDGET_HPP

#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"
#include "SelectionModes.hpp"


#include <QGraphicsSceneMouseEvent>

#include <QToolTip>
#include <QVector3D>


class PointData;
class QGraphicsProxyWidget;
class SpatialOverlayOpenGLWidget;
class TimeFrameIndex;



/**
 * @brief Spatial overlay plot widget for visualizing PointData across all time frames
 * 
 * This widget displays all points from selected PointData objects overlaid in a single
 * spatial view, with efficient rendering using OpenGL and spatial indexing for interactions.
 */
class SpatialOverlayPlotWidget : public AbstractPlotWidget {
    Q_OBJECT

public:
    explicit SpatialOverlayPlotWidget(QGraphicsItem * parent = nullptr);
    ~SpatialOverlayPlotWidget() override = default;

    QString getPlotType() const override;

    /**
     * @brief Set which PointData keys to display
     * @param point_data_keys List of PointData keys to visualize
     */
    void setPointDataKeys(QStringList const & point_data_keys);

    /**
     * @brief Get currently displayed PointData keys
     */
    QStringList getPointDataKeys() const { return _point_data_keys; }

    /**
     * @brief Get access to the OpenGL widget for advanced configuration
     * @return Pointer to the OpenGL widget
     */
    SpatialOverlayOpenGLWidget * getOpenGLWidget() const { return _opengl_widget; }

    /**
     * @brief Set the selection mode for the plot
     * @param mode The selection mode to activate
     */
    void setSelectionMode(SelectionMode mode);

    /**
     * @brief Get the current selection mode
     */
    SelectionMode getSelectionMode() const;

signals:
    /**
     * @brief Emitted when rendering properties change (point size, zoom, pan)
     */
    void renderingPropertiesChanged();

    /**
     * @brief Emitted when the selection changes
     * @param selected_count Number of currently selected points
     */
    void selectionChanged(size_t selected_count);

    /**
     * @brief Emitted when the selection mode changes
     * @param mode The new selection mode
     */
    void selectionModeChanged(SelectionMode mode);

protected:
    void paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget = nullptr) override;
    void resizeEvent(QGraphicsSceneResizeEvent * event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;

private slots:
    /**
     * @brief Update the visualization when data changes
     */
    void updateVisualization();

    /**
     * @brief Handle frame jump request from OpenGL widget
     * @param time_frame_index The time frame index to jump to
     */
    void handleFrameJumpRequest(int64_t time_frame_index, std::string const & data_key);

private:
    SpatialOverlayOpenGLWidget * _opengl_widget;
    QGraphicsProxyWidget * _proxy_widget;
    QStringList _point_data_keys;

    /**
     * @brief Load point data from DataManager
     */
    void loadPointData();

    /**
     * @brief Setup the OpenGL widget and proxy
     */
    void setupOpenGLWidget();
};

#endif// SPATIALOVERLAYPLOTWIDGET_HPP