#ifndef PROPERTIESPANEL_HPP
#define PROPERTIESPANEL_HPP

#include <QWidget>
#include <QMap>

class QStackedWidget;
class QScrollArea;
class AbstractPlotWidget;
class GlobalPropertiesWidget;
class AbstractPlotPropertiesWidget;

class DataManager;

namespace Ui {
class PropertiesPanel;
}

/**
 * @brief Properties panel for configuring global and plot-specific settings
 * 
 * This panel shows global dashboard properties and switches to show
 * plot-specific properties when a plot is selected.
 */
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    ~PropertiesPanel() override;

    /**
     * @brief Set the data manager that this properties panel uses
     * @param data_manager Pointer to the data manager
     */
    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Show properties for the selected plot
     * @param plot_id The unique ID of the selected plot
     * @param plot_widget Pointer to the plot widget
     */
    void showPlotProperties(const QString& plot_id, AbstractPlotWidget* plot_widget);

    /**
     * @brief Show global properties (no plot selected)
     */
    void showGlobalProperties();

    /**
     * @brief Register a properties widget for a specific plot type
     * @param plot_type The plot type identifier
     * @param properties_widget The properties widget for this plot type
     */
    void registerPlotPropertiesWidget(const QString& plot_type, AbstractPlotPropertiesWidget* properties_widget);

private slots:
    /**
     * @brief Handle when plot properties are updated
     */
    void handlePropertiesChanged();

private:
    Ui::PropertiesPanel* ui;

    std::shared_ptr<DataManager> _data_manager;
    GlobalPropertiesWidget* _global_properties;
    QStackedWidget* _stacked_widget;
    QScrollArea* _scroll_area;
    
    // Map of plot type to properties widget
    QMap<QString, AbstractPlotPropertiesWidget*> _plot_properties_widgets;
    
    QString _current_plot_id;
    AbstractPlotWidget* _current_plot_widget;
    
    /**
     * @brief Initialize the properties panel
     */
    void initializePropertiesPanel();
    
    /**
     * @brief Register built-in properties widgets for standard plot types
     */
    void registerBuiltInPropertiesWidgets();
    
    /**
     * @brief Get or create properties widget for a plot type
     * @param plot_type The plot type identifier
     * @return Pointer to the properties widget
     */
    AbstractPlotPropertiesWidget* getPropertiesWidgetForPlotType(const QString& plot_type);
};

#endif // PROPERTIESPANEL_HPP 