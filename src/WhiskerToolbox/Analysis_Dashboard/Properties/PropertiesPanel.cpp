#include "PropertiesPanel.hpp"
#include "ui_PropertiesPanel.h"
#include "GlobalPropertiesWidget.hpp"
#include "AbstractPlotPropertiesWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp"
#include "Analysis_Dashboard/Widgets/EventPlotWidget/EventPlotPropertiesWidget.hpp"
#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"

#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::PropertiesPanel),
      _global_properties(nullptr),
      _stacked_widget(nullptr),
      _scroll_area(nullptr),
      _current_plot_widget(nullptr) {
    ui->setupUi(this);
    
    initializePropertiesPanel();
}

PropertiesPanel::~PropertiesPanel() {
    delete ui;
}

void PropertiesPanel::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
}

void PropertiesPanel::initializePropertiesPanel() {
    // Create the stacked widget for switching between different property views
    _stacked_widget = new QStackedWidget();
    
    // Create global properties widget
    _global_properties = new GlobalPropertiesWidget();
    _stacked_widget->addWidget(_global_properties);
    
    // Connect global properties signals
    connect(_global_properties, &GlobalPropertiesWidget::globalPropertiesChanged,
            this, &PropertiesPanel::handlePropertiesChanged);
    
    // Register plot-specific properties widgets
    registerBuiltInPropertiesWidgets();
    
    // Set the stacked widget as the scroll area content
    ui->scrollArea->setWidget(_stacked_widget);
    
    // Show global properties by default
    showGlobalProperties();
}

void PropertiesPanel::showPlotProperties(const QString& plot_id, AbstractPlotWidget* plot_widget) {
    qDebug() << "PropertiesPanel: showPlotProperties called for plot_id:" << plot_id;
    
    if (!plot_widget) {
        qDebug() << "PropertiesPanel: No plot widget provided, showing global properties";
        showGlobalProperties();
        return;
    }
    
    _current_plot_id = plot_id;
    _current_plot_widget = plot_widget;
    
    // Get the properties widget for this plot type
    QString plot_type = plot_widget->getPlotType();
    qDebug() << "PropertiesPanel: Showing properties for plot type:" << plot_type;
    AbstractPlotPropertiesWidget* properties_widget = getPropertiesWidgetForPlotType(plot_type);
    
    if (properties_widget) {
        qDebug() << "PropertiesPanel: Found properties widget for" << plot_type;
        qDebug() << "PropertiesPanel: Setting DataManager and plot widget";
        
        // Configure the properties widget for this specific plot
        properties_widget->setDataManager(_data_manager);
        properties_widget->setPlotWidget(plot_widget);
        properties_widget->updateFromPlot();
        
        qDebug() << "PropertiesPanel: Switching to properties widget";
        // Show the properties widget
        _stacked_widget->setCurrentWidget(properties_widget);
    } else {
        qDebug() << "PropertiesPanel: No properties widget found for" << plot_type << "- showing global properties";
        // Fall back to global properties if no specific widget is available
        showGlobalProperties();
    }
}

void PropertiesPanel::showGlobalProperties() {
    qDebug() << "PropertiesPanel: showGlobalProperties called";
    _current_plot_id.clear();
    _current_plot_widget = nullptr;
    _stacked_widget->setCurrentWidget(_global_properties);
}

void PropertiesPanel::registerPlotPropertiesWidget(const QString& plot_type, AbstractPlotPropertiesWidget* properties_widget) {
    if (!properties_widget) {
        return;
    }
    
    // Store the widget and add it to the stacked widget
    _plot_properties_widgets[plot_type] = properties_widget;
    _stacked_widget->addWidget(properties_widget);
    
    // Connect signals
    connect(properties_widget, &AbstractPlotPropertiesWidget::propertiesChanged,
            this, &PropertiesPanel::handlePropertiesChanged);
}

void PropertiesPanel::handlePropertiesChanged() {
    // Properties have been changed - could emit a signal here if needed
    // For now, the plot widgets handle their own updates
}

void PropertiesPanel::registerBuiltInPropertiesWidgets() {
    // Register properties widgets for built-in plot types
    
    // Spatial Overlay Plot
    SpatialOverlayPlotPropertiesWidget* spatial_properties = new SpatialOverlayPlotPropertiesWidget(this);
    if (_data_manager) {
        spatial_properties->setDataManager(_data_manager);
    }
    registerPlotPropertiesWidget("Spatial Overlay Plot", spatial_properties);
    qDebug() << "PropertiesPanel: Registered properties widget for 'Spatial Overlay Plot'";

    // Event Plot
    EventPlotPropertiesWidget* event_properties = new EventPlotPropertiesWidget(this);
    if (_data_manager) {
        event_properties->setDataManager(_data_manager);
    }
    registerPlotPropertiesWidget("Event Plot", event_properties);
    qDebug() << "PropertiesPanel: Registered properties widget for 'Event Plot'";
}

AbstractPlotPropertiesWidget* PropertiesPanel::getPropertiesWidgetForPlotType(const QString& plot_type) {
    qDebug() << "PropertiesPanel: Looking for properties widget for plot type:" << plot_type;
    qDebug() << "PropertiesPanel: Available plot types:" << _plot_properties_widgets.keys();
    
    auto it = _plot_properties_widgets.find(plot_type);
    if (it != _plot_properties_widgets.end()) {
        return it.value();
    }
    
    // TODO: Implement factory pattern to create properties widgets for different plot types
    // For now, return nullptr for unregistered plot types
    return nullptr;
} 