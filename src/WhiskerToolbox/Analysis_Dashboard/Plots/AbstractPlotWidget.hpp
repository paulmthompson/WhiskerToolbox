#ifndef ABSTRACTPLOTWIDGET_HPP
#define ABSTRACTPLOTWIDGET_HPP

#include <QGraphicsWidget>
#include <QString>

#include <memory>

class DataManager;
class QGraphicsSceneMouseEvent;

/**
 * @brief Abstract base class for all plot widgets in the Analysis Dashboard
 * 
 * This class provides the common interface and functionality for plot widgets
 * that can be displayed in the dashboard's graphics scene. Plot widgets can
 * use Qt graphics infrastructure or OpenGL for rendering.
 */
class AbstractPlotWidget : public QGraphicsWidget {
    Q_OBJECT

public:
    explicit AbstractPlotWidget(QGraphicsItem* parent = nullptr);
    ~AbstractPlotWidget() override = default;

    /**
     * @brief Get the plot type name (e.g., "Scatter Plot", "Line Plot")
     * @return The human-readable name of this plot type
     */
    virtual QString getPlotType() const = 0;

    /**
     * @brief Get the plot instance name/title
     * @return The specific name/title for this plot instance
     */
    virtual QString getPlotTitle() const;

    /**
     * @brief Set the plot instance name/title
     * @param title The new title for this plot
     */
    virtual void setPlotTitle(const QString& title);

    /**
     * @brief Set the data manager for accessing data
     * @param data_manager Shared pointer to the data manager
     */
    virtual void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Get the unique identifier for this plot instance
     * @return Unique ID string
     */
    QString getPlotId() const;

signals:
    /**
     * @brief Emitted when this plot is selected/clicked
     * @param plot_id The unique ID of this plot
     */
    void plotSelected(const QString& plot_id);

    /**
     * @brief Emitted when plot properties change and need to be updated in properties panel
     * @param plot_id The unique ID of this plot
     */
    void propertiesChanged(const QString& plot_id);

    /**
     * @brief Emitted when this plot needs to be re-rendered/updated
     * @param plot_id The unique ID of this plot
     */
    void renderUpdateRequested(const QString& plot_id);

        /**
     * @brief Emitted when user requests to jump to a specific frame
     * @param time_frame_index The time frame index to jump to
     */
    void frameJumpRequested(int64_t time_frame_index, std::string const & data_key);

protected:
    /**
     * @brief Handle mouse press events for selection
     */
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

    /**
     * @brief Generate a unique ID for this plot instance
     */
    void generateUniqueId();

    std::shared_ptr<DataManager> _data_manager;
    QString _plot_title;
    QString _plot_id;

private:
    static int _next_plot_id;
};

#endif // ABSTRACTPLOTWIDGET_HPP 