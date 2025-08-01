
#include "Analysis_Dashboard.hpp"
#include "ui_Analysis_Dashboard.h"

#include "AbstractPlotOrganizer.hpp"
#include "GraphicsScenePlotOrganizer.hpp"
#include "PlotContainer.hpp"
#include "PlotFactory.hpp"
#include "DataManager/DataManager.hpp"
#include "Groups/GroupManager.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/AbstractPlotPropertiesWidget.hpp"
#include "Properties/PropertiesPanel.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "Toolbox/ToolboxPanel.hpp"

#include <QDebug>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QPainter>
#include <QSplitter>
#include <QVBoxLayout>
#include <QTimer>

Analysis_Dashboard::Analysis_Dashboard(std::shared_ptr<DataManager> data_manager,
                                       TimeScrollBar * time_scrollbar,
                                       QWidget * parent)
    : QMainWindow(parent),
      ui(new Ui::Analysis_Dashboard),
      _data_manager(std::move(data_manager)),
      _group_manager(std::make_unique<GroupManager>(this)),
      _time_scrollbar(time_scrollbar),
      _toolbox_panel(nullptr),
      _properties_panel(nullptr),
      _plot_organizer(nullptr),
      _main_splitter(nullptr) {

    ui->setupUi(this);

    initializeDashboard();
}

Analysis_Dashboard::~Analysis_Dashboard() {
    delete ui;
}

void Analysis_Dashboard::openWidget() {
    this->show();
}

void Analysis_Dashboard::initializeDashboard() {
    // Create the main components
    _toolbox_panel = new ToolboxPanel(_group_manager.get(), _data_manager, this);
    _properties_panel = new PropertiesPanel(this);
    
    // Create the plot organizer (using GraphicsScene implementation for now)
    _plot_organizer = std::make_unique<GraphicsScenePlotOrganizer>(this);

    // Configure the plot organizer by configuring each new plot with managers
    // This will be done when plots are added

    // Set data manager for the properties panel
    if (_data_manager) {
        _properties_panel->setDataManager(_data_manager);
    }

    setupLayout();
    connectSignals();

    // Set initial splitter sizes (toolbox: 250px, center: remaining, properties: 250px)
    QList<int> sizes = {250, 700, 250};
    ui->main_splitter->setSizes(sizes);

    // Update plot display after layout is set up
    updatePlotDisplay();

    qDebug() << "Analysis Dashboard initialized successfully";
}

void Analysis_Dashboard::setupLayout() {
    // Get the container widgets from the UI
    QWidget * toolbox_container = ui->toolbox_container;
    QWidget * graphics_container = ui->graphics_container;
    QWidget * properties_container = ui->properties_container;

    // Create layouts for each container
    QVBoxLayout * toolbox_layout = new QVBoxLayout(toolbox_container);
    toolbox_layout->setContentsMargins(0, 0, 0, 0);
    toolbox_layout->addWidget(_toolbox_panel);

    QVBoxLayout * graphics_layout = new QVBoxLayout(graphics_container);
    graphics_layout->setContentsMargins(0, 0, 0, 0);
    graphics_layout->addWidget(_plot_organizer->getDisplayWidget());

    QVBoxLayout * properties_layout = new QVBoxLayout(properties_container);
    properties_layout->setContentsMargins(0, 0, 0, 0);
    properties_layout->addWidget(_properties_panel);
}

void Analysis_Dashboard::connectSignals() {

    connect(_toolbox_panel, &ToolboxPanel::plotTypeSelected,
            this, &Analysis_Dashboard::handlePlotTypeSelected);

    // Connect plot organizer signals
    connect(_plot_organizer.get(), &AbstractPlotOrganizer::plotSelected,
            this, &Analysis_Dashboard::handlePlotSelected);

    connect(_plot_organizer.get(), &AbstractPlotOrganizer::plotAdded,
            this, &Analysis_Dashboard::handlePlotAdded);

    connect(_plot_organizer.get(), &AbstractPlotOrganizer::plotRemoved,
            this, &Analysis_Dashboard::handlePlotRemoved);

    connect(_plot_organizer.get(), &AbstractPlotOrganizer::frameJumpRequested,
            this, &Analysis_Dashboard::_changeScrollbar);
}

void Analysis_Dashboard::handlePlotSelected(QString const & plot_id) {
    qDebug() << "Analysis_Dashboard::handlePlotSelected called with plot_id:" << plot_id;

    PlotContainer* plot_container = _plot_organizer->getPlot(plot_id);
    if (plot_container) {
        qDebug() << "Analysis_Dashboard: Found plot container, showing properties";
        // Use the new method that works directly with PlotContainer's properties widget
        _properties_panel->showContainerProperties(plot_id, plot_container->getPropertiesWidget());
    } else {
        qDebug() << "Analysis_Dashboard: No plot container found, showing global properties";
        _properties_panel->showGlobalProperties();
    }
}

void Analysis_Dashboard::handlePlotAdded(QString const & plot_id) {
    qDebug() << "Plot added:" << plot_id;

    QString status_text = QString("Plot added: %1").arg(plot_id);
    ui->statusbar->showMessage(status_text, 3000);
}

void Analysis_Dashboard::handlePlotRemoved(QString const & plot_id) {
    qDebug() << "Plot removed:" << plot_id;

    // If this was the selected plot, show global properties
    _properties_panel->showGlobalProperties();

    // Update status bar
    QString status_text = QString("Plot removed: %1").arg(plot_id);
    ui->statusbar->showMessage(status_text, 3000);
}

void Analysis_Dashboard::handlePlotTypeSelected(QString const & plot_type) {
    qDebug() << "Plot type selected:" << plot_type;

    if (createAndAddPlot(plot_type)) {
        // Use a small delay to ensure the plot is properly added first
        QTimer::singleShot(10, [this]() {
            updatePlotDisplay();
        });
    }
}

bool Analysis_Dashboard::createAndAddPlot(QString const & plot_type) {
    qDebug() << "Analysis_Dashboard::createAndAddPlot: Creating plot of type:" << plot_type;
    
    // Use the plot factory to create a complete plot container
    auto plot_container = PlotFactory::createPlotContainer(plot_type);
    if (!plot_container) {
        qDebug() << "Failed to create plot container for type:" << plot_type;
        return false;
    }
    
    qDebug() << "Analysis_Dashboard::createAndAddPlot: Created plot container with ID:" << plot_container->getPlotId();
    
    // Configure the plot with all necessary managers
    plot_container->configureManagers(_data_manager, _group_manager.get(), 
                                     _toolbox_panel ? _toolbox_panel->getTableManager() : nullptr);
    
    qDebug() << "Analysis_Dashboard::createAndAddPlot: Configured managers, adding to organizer";
    
    // Add the plot to the organizer
    _plot_organizer->addPlot(std::move(plot_container));
    
    qDebug() << "Analysis_Dashboard::createAndAddPlot: Successfully added to organizer";
    return true;
}

void Analysis_Dashboard::_changeScrollbar(int64_t time_frame_index, std::string const & active_feature) {

    //auto active_feature = _highlighted_available_feature.toStdString();

    auto video_timeframe = _data_manager->getTime("time");

    auto active_feature_timeframe_key = _data_manager->getTimeFrame(active_feature);

    if (!active_feature_timeframe_key.empty()) {
        auto feature_timeframe = _data_manager->getTime(active_feature_timeframe_key);

        if (video_timeframe.get() != feature_timeframe.get()) {
            time_frame_index = feature_timeframe->getTimeAtIndex(TimeFrameIndex(time_frame_index));
            time_frame_index = video_timeframe->getIndexAtTime(time_frame_index).getValue();
        }
    }

    _time_scrollbar->changeScrollBarValue(time_frame_index);
}

void Analysis_Dashboard::resizeEvent(QResizeEvent * event) {
    QMainWindow::resizeEvent(event);

    // Update the plot display to fit the new size
    updatePlotDisplay();
}

void Analysis_Dashboard::updatePlotDisplay() {
    if (_plot_organizer) {
        // For GraphicsScene organizer, ensure plots are visible
        if (auto* graphics_organizer = dynamic_cast<GraphicsScenePlotOrganizer*>(_plot_organizer.get())) {
            graphics_organizer->ensurePlotsVisible();
        }
    }
}
