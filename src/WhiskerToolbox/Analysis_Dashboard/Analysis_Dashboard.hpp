#ifndef ANALYSIS_DASHBOARD_HPP
#define ANALYSIS_DASHBOARD_HPP

#include <QMainWindow>

#include <memory>

class AnalysisDashboardScene;
class DataManager;
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
                                QWidget* parent = nullptr);
    ~Analysis_Dashboard() override;

    /**
     * @brief Open and show the dashboard widget
     */
    void openWidget();

private slots:
    /**
     * @brief Handle plot selection from the graphics scene
     * @param plot_id The unique ID of the selected plot
     */
    void handlePlotSelected(const QString& plot_id);

    /**
     * @brief Handle when a plot is added to the scene
     * @param plot_id The unique ID of the added plot
     */
    void handlePlotAdded(const QString& plot_id);

    /**
     * @brief Handle when a plot is removed from the scene
     * @param plot_id The unique ID of the removed plot
     */
    void handlePlotRemoved(const QString& plot_id);

    void _changeScrollbar(int64_t time_frame_index, std::string const & active_feature);


private:
    Ui::Analysis_Dashboard* ui;
    
    std::shared_ptr<DataManager> _data_manager;
    TimeScrollBar* _time_scrollbar;
    
    // Main panels
    ToolboxPanel* _toolbox_panel;
    PropertiesPanel* _properties_panel;
    AnalysisDashboardScene* _dashboard_scene;
    QGraphicsView* _graphics_view;
    
    // Layout
    QSplitter* _main_splitter;
    
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
};

#endif // ANALYSIS_DASHBOARD_HPP
