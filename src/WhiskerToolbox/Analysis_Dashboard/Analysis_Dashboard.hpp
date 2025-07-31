#ifndef ANALYSIS_DASHBOARD_HPP
#define ANALYSIS_DASHBOARD_HPP

#include <QMainWindow>

#include <memory>

class AnalysisDashboardScene;
class AbstractPlotWidget;
class DataManager;
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
    explicit Analysis_Dashboard(std::shared_ptr<DataManager> data_manager,
                                TimeScrollBar * time_scrollbar,
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
    GroupManager * getGroupManager() const { return _group_manager.get(); }

private slots:
    /**
     * @brief Handle plot type selection from the toolbox panel
     * @param plot_type The type of plot to create
     */
    void handlePlotTypeSelected(QString const & plot_type);

    /**
     * @brief Handle plot selection from the graphics scene
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
    std::unique_ptr<GroupManager> _group_manager;
    TimeScrollBar * _time_scrollbar;

    // Main panels
    ToolboxPanel * _toolbox_panel;
    PropertiesPanel * _properties_panel;
    AnalysisDashboardScene * _dashboard_scene;
    QGraphicsView * _graphics_view;

    // Layout
    QSplitter * _main_splitter;

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
     * @brief Create a plot widget of the specified type
     * @param plot_type The type of plot to create
     * @return Pointer to the created plot widget, or nullptr if creation failed
     */
    AbstractPlotWidget * createPlotWidget(QString const & plot_type);
};

#endif// ANALYSIS_DASHBOARD_HPP
