
#include "Analysis_Dashboard.hpp"
#include "ui_Analysis_Dashboard.h"

#include "DataManager/DataManager.hpp"
#include "Groups/GroupManager.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/PropertiesPanel.hpp"
#include "Scene/AnalysisDashboardScene.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "Toolbox/ToolboxPanel.hpp"
#include "Widgets/EventPlotWidget/EventPlotWidget.hpp"
#include "Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp"
#include "Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"

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
    _toolbox_panel = new ToolboxPanel(_group_manager.get(), _data_manager, this);
    _properties_panel = new PropertiesPanel(this);
    _dashboard_scene = new AnalysisDashboardScene(this);
    _graphics_view = new QGraphicsView(_dashboard_scene, this);

    // Configure the graphics view
    _graphics_view->setRenderHint(QPainter::Antialiasing);
    _graphics_view->setDragMode(QGraphicsView::RubberBandDrag);
    _graphics_view->setMinimumSize(400, 300);

    // Disable scrollbars to prevent scrolling
    _graphics_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _graphics_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Set data manager for the scene
    if (_data_manager) {
        _dashboard_scene->setDataManager(_data_manager);
        _properties_panel->setDataManager(_data_manager);
    }

    // Set group manager for the scene
    if (_group_manager) {
        _dashboard_scene->setGroupManager(_group_manager.get());
    }

    // Set table manager for the scene
    if (_toolbox_panel && _toolbox_panel->getTableManager()) {
        _dashboard_scene->setTableManager(_toolbox_panel->getTableManager());
    }

    setupLayout();
    connectSignals();

    // Set initial splitter sizes (toolbox: 250px, center: remaining, properties: 250px)
    QList<int> sizes = {250, 700, 250};
    ui->main_splitter->setSizes(sizes);

    // Update graphics view after layout is set up
    updateGraphicsView();

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
    graphics_layout->addWidget(_graphics_view);

    QVBoxLayout * properties_layout = new QVBoxLayout(properties_container);
    properties_layout->setContentsMargins(0, 0, 0, 0);
    properties_layout->addWidget(_properties_panel);
}

void Analysis_Dashboard::connectSignals() {

    connect(_toolbox_panel, &ToolboxPanel::plotTypeSelected,
            this, &Analysis_Dashboard::handlePlotTypeSelected);

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

void Analysis_Dashboard::handlePlotSelected(QString const & plot_id) {
    qDebug() << "Plot selected:" << plot_id;

    AbstractPlotWidget * plot_widget = _dashboard_scene->getPlotWidget(plot_id);
    if (plot_widget) {
        _properties_panel->showPlotProperties(plot_id, plot_widget);
    } else {
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


    AbstractPlotWidget * new_plot = createPlotWidget(plot_type);
    if (new_plot) {
        // Add the plot to the scene at a centered position
        _dashboard_scene->addPlotWidget(new_plot, QPointF(0, 0));// Will be centered by the scene

        // Ensure the plot is visible by fitting the scene content
        // Use a small delay to ensure the plot is properly added first
        QTimer::singleShot(10, [this]() {
            if (_graphics_view && _dashboard_scene) {
                _graphics_view->fitInView(_dashboard_scene->sceneRect(), Qt::KeepAspectRatio);
            }
        });
    }
}

AbstractPlotWidget * Analysis_Dashboard::createPlotWidget(QString const & plot_type) {
    // Factory pattern for creating different plot types
    if (plot_type == "scatter_plot") {
        return new ScatterPlotWidget();
    } else if (plot_type == "spatial_overlay_plot") {
        qDebug() << "Creating spatial overlay plot widget";
        return new SpatialOverlayPlotWidget();
    } else if (plot_type == "event_plot") {
        qDebug() << "Creating event plot widget";
        return new EventPlotWidget();
    }

    // Add more plot types here as they are implemented
    // else if (plot_type == "line_plot") {
    //     return new LinePlotWidget();
    // }

    qDebug() << "Unknown plot type:" << plot_type;
    return nullptr;
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

    // Update the graphics view to fit the new size
    updateGraphicsView();
}

void Analysis_Dashboard::updateGraphicsView() {
    if (_graphics_view && _dashboard_scene) {
        // Don't change the scene rect - just fit the existing content to the view
        // This preserves the positions of existing plots
        _graphics_view->fitInView(_dashboard_scene->sceneRect(), Qt::KeepAspectRatio);
        
        // Ensure all plots are still visible within the scene bounds
        _dashboard_scene->ensurePlotsVisible();
    }
}
