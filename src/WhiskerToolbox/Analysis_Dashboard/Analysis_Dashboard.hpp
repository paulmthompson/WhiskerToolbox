#ifndef ANALYSIS_DASHBOARD_HPP
#define ANALYSIS_DASHBOARD_HPP

#include <QMainWindow>

#include <memory>

namespace ads { class CDockManager; }

class AbstractPlotOrganizer;
class DataManager;
class GroupCoordinator;
class GroupManager;
class QGraphicsView;
class QSplitter;
class TimeScrollBar;
class ToolboxPanel;
class PropertiesPanel;

namespace Ui {
class Analysis_Dashboard;
}

/**
 * @brief Main Analysis Dashboard widget for creating and managing plots
 * 
 * This dashboard provides a three-panel interface:
 * - Left: Toolbox panel with available plot types
 * - Center: Graphics view with draggable/resizable plots
 * - Right: Properties panel for plot configuration
 */
class Analysis_Dashboard : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Construct the Analysis Dashboard
     * @param data_manager Shared data manager
     * @param group_manager Existing GroupManager instance (not owned)
     * @param time_scrollbar Global time scrollbar
     * @param dock_manager Global dock manager (plots will be added as docks)
     * @param parent Parent widget
     */
    explicit Analysis_Dashboard(std::shared_ptr<DataManager> data_manager,
                                GroupManager * group_manager,
                                TimeScrollBar * time_scrollbar,
                                ads::CDockManager * dock_manager,
                                QWidget * parent = nullptr);
    ~Analysis_Dashboard() override;

    /**
     * @brief Open and show the dashboard widget
     */
    void openWidget();

    /**
     * @brief Get the group manager for this dashboard
     * @return Pointer to the group manager
     */
    GroupManager * getGroupManager() const { return _group_manager; }


protected:
    /**
     * @brief Handle window resize events to adjust graphics view
     * @param event The resize event
     */
    void resizeEvent(QResizeEvent * event) override;

private slots:
    /**
     * @brief Handle plot type selection from the toolbox panel
     * @param plot_type The type of plot to create
     */
    void handlePlotTypeSelected(QString const & plot_type);

    /**
     * @brief Handle plot selection from the graphics scene
     * 
     * When a plot is selected, it emits a signal that is caught by
     * the AnalysisDashboardScene that encloses it. This then propogates
     * the signal upward to the Analysis_Dashboard. The Analysis Dashboard
     * can then show the appropriate properties panel based on the plot ID.
     * 
     * 
     * @param plot_id The unique ID of the selected plot
     */
    void handlePlotSelected(QString const & plot_id);

    /**
     * @brief Handle when a plot is added to the scene
     * @param plot_id The unique ID of the added plot
     */
    void handlePlotAdded(QString const & plot_id);

    /**
     * @brief Handle when a plot is removed from the scene
     * @param plot_id The unique ID of the removed plot
     */
    void handlePlotRemoved(QString const & plot_id);

    void _changeScrollbar(int64_t time_frame_index, std::string const & active_feature);


private:
    Ui::Analysis_Dashboard * ui;

    std::shared_ptr<DataManager> _data_manager;
    GroupManager * _group_manager; // Not owned - managed by MainWindow
    std::unique_ptr<GroupCoordinator> _group_coordinator;
    TimeScrollBar * _time_scrollbar;
    ads::CDockManager * _dock_manager;

    // Main panels
    ToolboxPanel * _toolbox_panel;
    PropertiesPanel * _properties_panel;
    std::unique_ptr<AbstractPlotOrganizer> _plot_organizer;

    // Layout (graphics center removed in docking mode)

    /**
     * @brief Initialize the dashboard layout and components
     */
    void initializeDashboard();

    /**
     * @brief Setup the three-panel layout
     */
    void setupLayout();

    /**
     * @brief Connect signals between components
     */
    void connectSignals();

    /**
     * @brief Create a plot container for the specified type
     * 
     * This method uses the PlotFactory to create a complete plot container
     * with both plot widget and properties widget.
     * 
     * @param plot_type The type of plot to create
     * @return True if plot was created and added successfully, false otherwise
     */
    bool createAndAddPlot(QString const & plot_type);

    /**
     * @brief Update the plot organizer display
     */
    void updatePlotDisplay();
};

#endif// ANALYSIS_DASHBOARD_HPP
