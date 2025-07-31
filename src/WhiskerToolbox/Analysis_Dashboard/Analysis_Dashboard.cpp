
#include "Analysis_Dashboard.hpp"
#include "ui_Analysis_Dashboard.h"

#include "DataManager/DataManager.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/PropertiesPanel.hpp"
#include "Scene/AnalysisDashboardScene.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "Toolbox/ToolboxPanel.hpp"

#include <QGraphicsView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QPainter>

Analysis_Dashboard::Analysis_Dashboard(std::shared_ptr<DataManager> data_manager, 
    TimeScrollBar * time_scrollbar,
    QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::Analysis_Dashboard),
      _data_manager(std::move(data_manager)),
      _time_scrollbar(time_scrollbar),
      _toolbox_panel(nullptr),
      _properties_panel(nullptr),
      _dashboard_scene(nullptr),
      _graphics_view(nullptr),
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
    _toolbox_panel = new ToolboxPanel(this);
    _properties_panel = new PropertiesPanel(this);
    _dashboard_scene = new AnalysisDashboardScene(this);
    _graphics_view = new QGraphicsView(_dashboard_scene, this);
    
    // Configure the graphics view
    _graphics_view->setRenderHint(QPainter::Antialiasing);
    _graphics_view->setDragMode(QGraphicsView::RubberBandDrag);
    _graphics_view->setAcceptDrops(true);
    _graphics_view->setMinimumSize(400, 300);
    
    // Set data manager for the scene
    if (_data_manager) {
        _dashboard_scene->setDataManager(_data_manager);
        _properties_panel->setDataManager(_data_manager);
    }
    
    setupLayout();
    connectSignals();
    
    // Set initial splitter sizes (toolbox: 250px, center: remaining, properties: 250px)
    QList<int> const sizes = {250, 700, 250};
    ui->main_splitter->setSizes(sizes);
    
    qDebug() << "Analysis Dashboard initialized successfully";
}

void Analysis_Dashboard::setupLayout() {
    // Get the container widgets from the UI
    QWidget* toolbox_container = ui->toolbox_container;
    QWidget* graphics_container = ui->graphics_container;
    QWidget* properties_container = ui->properties_container;
    
    // Create layouts for each container
    auto* toolbox_layout = new QVBoxLayout(toolbox_container);
    toolbox_layout->setContentsMargins(0, 0, 0, 0);
    toolbox_layout->addWidget(_toolbox_panel);
    
    auto* graphics_layout = new QVBoxLayout(graphics_container);
    graphics_layout->setContentsMargins(0, 0, 0, 0);
    graphics_layout->addWidget(_graphics_view);
    
    auto* properties_layout = new QVBoxLayout(properties_container);
    properties_layout->setContentsMargins(0, 0, 0, 0);
    properties_layout->addWidget(_properties_panel);
}

void Analysis_Dashboard::connectSignals() {
    // Connect dashboard scene signals
    connect(_dashboard_scene, &AnalysisDashboardScene::plotSelected,
            this, &Analysis_Dashboard::handlePlotSelected);
    
    connect(_dashboard_scene, &AnalysisDashboardScene::plotAdded,
            this, &Analysis_Dashboard::handlePlotAdded);
    
    connect(_dashboard_scene, &AnalysisDashboardScene::plotRemoved,
            this, &Analysis_Dashboard::handlePlotRemoved);

    connect(_dashboard_scene, &AnalysisDashboardScene::frameJumpRequested,
            this, &Analysis_Dashboard::_changeScrollbar);
}

void Analysis_Dashboard::handlePlotSelected(const QString& plot_id) {
    qDebug() << "Plot selected:" << plot_id;
    
    // Get the plot widget and show its properties
    AbstractPlotWidget* plot_widget = _dashboard_scene->getPlotWidget(plot_id);
    if (plot_widget) {
        _properties_panel->showPlotProperties(plot_id, plot_widget);
    } else {
        _properties_panel->showGlobalProperties();
    }
}

void Analysis_Dashboard::handlePlotAdded(const QString& plot_id) {
    qDebug() << "Plot added:" << plot_id;
    
    // Update status bar or other UI elements as needed
    QString const status_text = QString("Plot added: %1").arg(plot_id);
    ui->statusbar->showMessage(status_text, 3000);
}

void Analysis_Dashboard::handlePlotRemoved(const QString& plot_id) {
    qDebug() << "Plot removed:" << plot_id;
    
    // If this was the selected plot, show global properties
    _properties_panel->showGlobalProperties();
    
    // Update status bar
    QString const status_text = QString("Plot removed: %1").arg(plot_id);
    ui->statusbar->showMessage(status_text, 3000);
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
