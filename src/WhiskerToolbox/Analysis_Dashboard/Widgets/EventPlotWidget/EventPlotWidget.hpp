#ifndef EVENTPLOTWIDGET_HPP
#define EVENTPLOTWIDGET_HPP

#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>

class EventPlotOpenGLWidget;
class TableView;

#include <memory>

/**
 * @brief Event plot widget for visualizing event data
 * 
 * This widget displays events in a time-based visualization using OpenGL
 * for efficient rendering of large datasets.
 */
class EventPlotWidget : public AbstractPlotWidget {
    Q_OBJECT

public:
    explicit EventPlotWidget(QGraphicsItem * parent = nullptr);
    ~EventPlotWidget() override = default;

    QString getPlotType() const override;

    /**
     * @brief Set which event data keys to display
     * @param event_data_keys List of event data keys to visualize
     */
    void setEventDataKeys(QStringList const & event_data_keys);

    /**
     * @brief Set which Y-axis data keys to display
     * @param y_axis_data_keys List of Y-axis data keys to visualize
     */
    void setYAxisDataKeys(QStringList const & y_axis_data_keys);

    /**
     * @brief Get currently displayed event data keys
     */
    QStringList getEventDataKeys() const { return _event_data_keys; }

    /**
     * @brief Get access to the OpenGL widget for advanced configuration
     * @return Pointer to the OpenGL widget
     */
    EventPlotOpenGLWidget * getOpenGLWidget() const { return _opengl_widget; }

    /**
     * @brief Set the X-axis range for the plot
     * @param negative_range Negative range in milliseconds (from -negative_range to -1)
     * @param positive_range Positive range in milliseconds (from 1 to positive_range)
     */
    void setXAxisRange(int negative_range, int positive_range);

    /**
     * @brief Get the current X-axis range
     * @param negative_range Output parameter for negative range
     * @param positive_range Output parameter for positive range
     */
    void getXAxisRange(int & negative_range, int & positive_range) const;

signals:
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

    /**
     * @brief Handle frame jump request from OpenGL widget
     * @param time_frame_index The time frame index to jump to
     */
    void handleFrameJumpRequest(int64_t time_frame_index, QString const & data_key);

private:
    EventPlotOpenGLWidget * _opengl_widget;
    QGraphicsProxyWidget * _proxy_widget;
    QStringList _event_data_keys;
    QStringList _y_axis_data_keys;
    TableView * _table_view;

    /**
     * @brief Load event data from DataManager
     */
    void loadEventData();

    /**
     * @brief Setup the OpenGL widget and proxy
     */
    void setupOpenGLWidget();
};

#endif// EVENTPLOTWIDGET_HPP