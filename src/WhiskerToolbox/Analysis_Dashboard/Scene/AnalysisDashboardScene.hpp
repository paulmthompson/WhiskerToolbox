#ifndef ANALYSISDASHBOARDSCENE_HPP
#define ANALYSISDASHBOARDSCENE_HPP

#include <QGraphicsScene>
#include <QMap>
#include <QString>

#include <memory>

class AbstractPlotWidget;
class DataManager;
class GroupManager;
class TableManager;

/**
 * @brief Custom QGraphicsScene for the Analysis Dashboard
 * 
 * This scene manages plot widgets that can be dragged, dropped, moved,
 * and resized within the dashboard's graphics view.
 */
class AnalysisDashboardScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit AnalysisDashboardScene(QObject * parent = nullptr);
    ~AnalysisDashboardScene() override = default;

    /**
     * @brief Set the group manager for plot widgets
     * @param group_manager Pointer to the group manager
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Set the table manager for plot widgets
     * @param table_manager Pointer to the table manager
     */
    void setTableManager(TableManager * table_manager);

    /**
     * @brief Add a plot widget to the scene at the specified position
     * @param plot_widget The plot widget to add
     * @param position The position where to place the plot
     */
    void addPlotWidget(AbstractPlotWidget * plot_widget, QPointF const & position = QPointF(0, 0));

    /**
     * @brief Remove a plot widget from the scene
     * @param plot_id The unique ID of the plot to remove
     */
    void removePlotWidget(QString const & plot_id);

    /**
     * @brief Get a plot widget by its ID
     * @param plot_id The unique ID of the plot
     * @return Pointer to the plot widget, or nullptr if not found
     */
    AbstractPlotWidget * getPlotWidget(QString const & plot_id) const;

    /**
     * @brief Get all plot widgets in the scene
     * @return Map of plot ID to plot widget pointer
     */
    QMap<QString, AbstractPlotWidget *> getAllPlotWidgets() const;

    /**
     * @brief Ensure all plots are visible within the scene bounds
     */
    void ensurePlotsVisible();

signals:
    /**
     * @brief Emitted when a plot widget is selected
     * @param plot_id The unique ID of the selected plot
     */
    void plotSelected(QString const & plot_id);

    /**
     * @brief Emitted when a plot widget is added to the scene
     * @param plot_id The unique ID of the added plot
     */
    void plotAdded(QString const & plot_id);

    /**
     * @brief Emitted when a plot widget is removed from the scene
     * @param plot_id The unique ID of the removed plot
     */
    void plotRemoved(QString const & plot_id);

    void frameJumpRequested(int64_t time_frame_index, std::string const & data_key);

private slots:
    /**
     * @brief Handle plot selection signals from plot widgets
     * @param plot_id The ID of the selected plot
     */
    void handlePlotSelected(QString const & plot_id);

    /**
     * @brief Handle render update requests from plot widgets
     * @param plot_id The ID of the plot requesting update
     */
    void handleRenderUpdateRequested(QString const & plot_id);

private:
    std::shared_ptr<DataManager> _data_manager;
    GroupManager * _group_manager = nullptr;
    TableManager * _table_manager = nullptr;
    QMap<QString, AbstractPlotWidget *> _plot_widgets;

};

#endif// ANALYSISDASHBOARDSCENE_HPP