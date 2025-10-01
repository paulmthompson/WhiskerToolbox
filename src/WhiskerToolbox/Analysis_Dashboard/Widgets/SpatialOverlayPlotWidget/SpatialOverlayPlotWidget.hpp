#ifndef SPATIALOVERLAYPLOTWIDGET_HPP
#define SPATIALOVERLAYPLOTWIDGET_HPP

#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"
#include "Selection/SelectionModes.hpp"
#include "Entity/EntityTypes.hpp"


#include <QGraphicsSceneMouseEvent>

#include <QToolTip>
#include <QVector3D>


class QGraphicsProxyWidget;
class SpatialOverlayOpenGLWidget;
class QKeyEvent;

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
    ~SpatialOverlayPlotWidget() override;

    QString getPlotType() const override;

    /**
     * @brief Set all data keys in a single call to avoid multiple updates
     * @param point_data_keys List of PointData keys to visualize
     * @param mask_data_keys List of MaskData keys to visualize
     * @param line_data_keys List of LineData keys to visualize
     */
    void setDataKeys(QStringList const & point_data_keys,
                     QStringList const & mask_data_keys,
                     QStringList const & line_data_keys);

    /**
     * @brief Get currently displayed PointData keys
     */
    QStringList getPointDataKeys() const { return _point_data_keys; }

    /**
     * @brief Get currently displayed MaskData keys
     */
    QStringList getMaskDataKeys() const { return _mask_data_keys; }

    /**
     * @brief Get currently displayed LineData keys
     */
    QStringList getLineDataKeys() const { return _line_data_keys; }

    /**
     * @brief Get access to the OpenGL widget for advanced configuration
     * @return Pointer to the OpenGL widget
     */
    SpatialOverlayOpenGLWidget * getOpenGLWidget() const { return _opengl_widget; }

    /**
     * @brief Set the group manager for data grouping
     * @param group_manager Pointer to the group manager
     */
    void setGroupManager(GroupManager * group_manager) override;

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
    void keyPressEvent(QKeyEvent * event) override;
    void onGroupPropertiesChanged(int group_id) override;
    void onGroupCreated(int group_id, QString const & name, QColor const & color) override;
    void onGroupRemoved(int group_id) override;

private slots:
    /**
     * @brief Update the visualization when data changes
     */
    void updateVisualization();

    /**
     * @brief Handle frame jump request from OpenGL widget
     * @param time_frame_index The time frame index to jump to
     */
    void handleFrameJumpRequest(EntityId entity_id, QString const & data_key);

private:
    SpatialOverlayOpenGLWidget * _opengl_widget;
    QGraphicsProxyWidget * _proxy_widget;
    QStringList _point_data_keys;
    QStringList _mask_data_keys;
    QStringList _line_data_keys;
    // Reentrancy guard to avoid duplicate updates when data-setting triggers callbacks
    bool _is_updating_visualization {false};
    /**
     * @brief Load point data from DataManager
     */
    void loadPointData();

    /**
     * @brief Load mask data from DataManager
     */
    void loadMaskData();

    /**
     * @brief Load mask data from DataManager
     */
    void loadLineData();

    /**
     * @brief Setup the OpenGL widget and proxy
     */
    void setupOpenGLWidget();
    
    // Internal helpers to coalesce duplicate updates
    void scheduleRenderUpdate();
    bool _render_update_pending {false};
};

#endif// SPATIALOVERLAYPLOTWIDGET_HPP