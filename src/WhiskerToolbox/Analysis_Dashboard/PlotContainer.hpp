#ifndef PLOTCONTAINER_HPP
#define PLOTCONTAINER_HPP

#include <QObject>
#include <QString>

#include <memory>

class AbstractPlotWidget;
class AbstractPlotPropertiesWidget;
class DataManager;
class GroupManager;

/**
 * @brief Container that manages a plot widget and its corresponding properties widget
 * 
 * This class encapsulates the lifetime relationship between a plot and its properties,
 * ensuring they are created together, configured together, and destroyed together.
 * This design makes it easier to change plot organization strategies (QGraphicsScene,
 * dock widgets, etc.) without affecting the core plot logic.
 */
class PlotContainer : public QObject {
    Q_OBJECT

public:
    explicit PlotContainer(std::unique_ptr<AbstractPlotWidget> plot_widget,
                          std::unique_ptr<AbstractPlotPropertiesWidget> properties_widget,
                          QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     * @post Ensures proper destruction of owned plot and properties widgets
     */
    ~PlotContainer() override;

    /**
     * @brief Get the plot widget
     * @return Pointer to the plot widget (owned by this container)
     */
    AbstractPlotWidget* getPlotWidget() const { return plot_widget_.get(); }

    /**
     * @brief Get the properties widget
     * @return Pointer to the properties widget (owned by this container)
     */
    AbstractPlotPropertiesWidget* getPropertiesWidget() const { return properties_widget_.get(); }

    /**
     * @brief Get the unique plot ID
     * @return The plot's unique identifier
     */
    QString getPlotId() const;

    /**
     * @brief Get the plot type name
     * @return Human-readable plot type
     */
    QString getPlotType() const;

    /**
     * @brief Configure the plot and properties widgets with DataManager and GroupManager
     */
    void configureManagers(std::shared_ptr<DataManager> data_manager,
                           GroupManager* group_manager);

    /**
     * @brief Update properties widget from current plot state
     */
    void updatePropertiesFromPlot();

    /**
     * @brief Apply current properties to the plot
     */
    void applyPropertiesToPlot();

signals:
    /**
     * @brief Emitted when this plot is selected
     * @param plot_id The unique ID of this plot
     */
    void plotSelected(const QString& plot_id);

    /**
     * @brief Emitted when plot properties change
     * @param plot_id The unique ID of this plot
     */
    void propertiesChanged(const QString& plot_id);

    /**
     * @brief Emitted when user requests frame jump
     * @param time_frame_index The frame to jump to
     * @param data_key The data key associated with the jump
     */
    void frameJumpRequested(int64_t time_frame_index, const std::string& data_key);

private slots:
    /**
     * @brief Handle selection events from the plot widget
     */
    void onPlotSelected(const QString& plot_id);

    /**
     * @brief Handle property changes from the properties widget
     */
    void onPropertiesChanged();

    /**
     * @brief Handle frame jump requests from the plot widget
     */
    void onFrameJumpRequested(int64_t time_frame_index, const std::string& data_key);

private:
    std::unique_ptr<AbstractPlotWidget> plot_widget_;
    std::unique_ptr<AbstractPlotPropertiesWidget> properties_widget_;

    /**
     * @brief Connect internal signals between plot and properties widgets
     */
    void connectInternalSignals();
};

#endif // PLOTCONTAINER_HPP
