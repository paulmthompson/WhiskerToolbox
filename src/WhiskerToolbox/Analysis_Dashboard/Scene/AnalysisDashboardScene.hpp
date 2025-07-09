#ifndef ANALYSISDASHBOARDSCENE_HPP
#define ANALYSISDASHBOARDSCENE_HPP

#include <QGraphicsScene>
#include <QMap>
#include <QString>

#include <memory>

class AbstractPlotWidget;
class DataManager;
class QGraphicsSceneDragDropEvent;
class QMimeData;

/**
 * @brief Custom QGraphicsScene for the Analysis Dashboard
 * 
 * This scene manages plot widgets that can be dragged, dropped, moved,
 * and resized within the dashboard's graphics view.
 */
class AnalysisDashboardScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit AnalysisDashboardScene(QObject* parent = nullptr);
    ~AnalysisDashboardScene() override = default;

    /**
     * @brief Set the data manager for plot widgets
     * @param data_manager Shared pointer to the data manager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Add a plot widget to the scene at the specified position
     * @param plot_widget The plot widget to add
     * @param position The position where to place the plot
     */
    void addPlotWidget(AbstractPlotWidget* plot_widget, const QPointF& position = QPointF(0, 0));

    /**
     * @brief Remove a plot widget from the scene
     * @param plot_id The unique ID of the plot to remove
     */
    void removePlotWidget(const QString& plot_id);

    /**
     * @brief Get a plot widget by its ID
     * @param plot_id The unique ID of the plot
     * @return Pointer to the plot widget, or nullptr if not found
     */
    AbstractPlotWidget* getPlotWidget(const QString& plot_id) const;

    /**
     * @brief Get all plot widgets in the scene
     * @return Map of plot ID to plot widget pointer
     */
    QMap<QString, AbstractPlotWidget*> getAllPlotWidgets() const;

signals:
    /**
     * @brief Emitted when a plot widget is selected
     * @param plot_id The unique ID of the selected plot
     */
    void plotSelected(const QString& plot_id);

    /**
     * @brief Emitted when a plot widget is added to the scene
     * @param plot_id The unique ID of the added plot
     */
    void plotAdded(const QString& plot_id);

    /**
     * @brief Emitted when a plot widget is removed from the scene
     * @param plot_id The unique ID of the removed plot
     */
    void plotRemoved(const QString& plot_id);

protected:
    /**
     * @brief Handle drag enter events for drag and drop
     */
    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;

    /**
     * @brief Handle drag move events for drag and drop
     */
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;

    /**
     * @brief Handle drop events for drag and drop
     */
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private slots:
    /**
     * @brief Handle plot selection signals from plot widgets
     * @param plot_id The ID of the selected plot
     */
    void handlePlotSelected(const QString& plot_id);

    /**
     * @brief Handle render update requests from plot widgets
     * @param plot_id The ID of the plot requesting update
     */
    void handleRenderUpdateRequested(const QString& plot_id);

private:
    std::shared_ptr<DataManager> _data_manager;
    QMap<QString, AbstractPlotWidget*> _plot_widgets;

    /**
     * @brief Create a plot widget from mime data
     * @param mime_data The mime data containing plot type information
     * @return Pointer to the created plot widget, or nullptr if creation failed
     */
    AbstractPlotWidget* createPlotFromMimeData(const QMimeData* mime_data);
};

#endif // ANALYSISDASHBOARDSCENE_HPP 