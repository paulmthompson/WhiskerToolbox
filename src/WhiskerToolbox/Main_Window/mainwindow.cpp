#include "mainwindow.hpp"

#include "ui_mainwindow.h"
#include "video_loader.hpp"

#include "DataManager.hpp"
#include "DataManager/Media/Image_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Media/Video_Data.hpp"

#include "Analysis_Dashboard/Analysis_Dashboard.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/PropertiesHost.hpp"
#include "EditorState/SelectionContext.hpp"
#include "ZoneManager.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "GroupManagementWidget/GroupManagementWidget.hpp"
#include "TableDesignerWidget/TableDesignerWidget.hpp"
#include "BatchProcessing_Widget/BatchProcessing_Widget.hpp"
#include "DataManager_Widget/DataManager_Widget.hpp"
#include "DataTransform_Widget/DataTransform_Widget.hpp"
#include "DataViewer_Widget/DataViewer_Widget.hpp"
#include "DockAreaWidget.h"
#include "DockSplitter.h"
#include "Export_Widgets/Export_Video_Widget/Export_Video_Widget.hpp"
#include "IO_Widgets/DigitalTimeSeries/Digital_Interval_Loader_Widget.hpp"
#include "IO_Widgets/DigitalTimeSeries/Digital_Event_Loader_Widget.hpp"
#include "IO_Widgets/Lines/Line_Loader_Widget.hpp"
#include "IO_Widgets/Masks/Mask_Loader_Widget.hpp"
#include "IO_Widgets/Points/Point_Loader_Widget.hpp"
#include "IO_Widgets/Tensors/Tensor_Loader_Widget.hpp"
#include "ML_Widget/ML_Widget.hpp"
#include "Media_Widget/Media_Widget.hpp"
#include "MediaWidgetManager/MediaWidgetManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Tongue_Widget/Tongue_Widget.hpp"
#include "Whisker_Widget.hpp"
#include "Terminal_Widget/TerminalWidget.hpp"

#include "Test_Widget/TestWidgetState.hpp"
#include "Test_Widget/TestWidgetView.hpp"
#include "Test_Widget/TestWidgetProperties.hpp"

// Module registration headers - each module defines its own factory functions
#include "Media_Widget/MediaWidgetRegistration.hpp"

#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QShortcut>
#include <QSplitter>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>

#include <filesystem>
#include <iostream>
#include <variant>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      _data_manager{std::make_shared<DataManager>()},
      _editor_registry{std::make_unique<EditorRegistry>(_data_manager, this)},
      _zone_manager(nullptr),
      _properties_host(nullptr),
      _group_manager(nullptr),
      _group_management_widget(nullptr),
      _data_manager_widget(nullptr)

{
    ui->setupUi(this);

    // Register editor types with the factory
    _registerEditorTypes();

    // Configure dock manager BEFORE creating it
    // Using native title bars for floating widgets (works smoothly on all platforms)
    // On Linux, native title bars allow proper window dragging
    // Re-docking is handled in showDockWidget() - closed floating widgets
    // are automatically re-docked when reopened via the modules menu
    ads::CDockManager::setConfigFlags(ads::CDockManager::DefaultOpaqueConfig 
                                     | ads::CDockManager::OpaqueSplitterResize
                                     | ads::CDockManager::DragPreviewIsDynamic);

    _m_DockManager = new ads::CDockManager(this);

    // Create ZoneManager to manage standard UI zones
    _zone_manager = std::make_unique<ZoneManager>(_m_DockManager, this);

    //This is necessary to accept keyboard events
    this->setFocusPolicy(Qt::StrongFocus);

    // Create the GroupManager with the DataManager's EntityGroupManager
    auto* entity_group_manager = _data_manager->getEntityGroupManager();
    if (entity_group_manager) {
        _group_manager = std::make_unique<GroupManager>(entity_group_manager, _data_manager, this);
        _group_management_widget = new GroupManagementWidget(_group_manager.get(), this);
    }

    // Create TimeScrollBar programmatically (no longer in UI)
    _time_scrollbar = new TimeScrollBar(this);
    _time_scrollbar->setDataManager(_data_manager);

    // Create the DataManager_Widget with EditorRegistry
    _data_manager_widget = new DataManager_Widget(_data_manager, _time_scrollbar, _editor_registry.get(), this);
    
    // Set the GroupManager for the DataManager_Widget
    if (_group_manager) {
        _data_manager_widget->setGroupManager(_group_manager.get());
    }

    // Create media widget manager with EditorRegistry
    _media_manager = std::make_unique<MediaWidgetManager>(_data_manager, _editor_registry.get(), this);
    
    // Connect the group manager to the media widget manager
    if (_group_manager) {
        _media_manager->setGroupManager(_group_manager.get());
    }

    _verbose = false;

    // Create the main media widget through the manager
    _media_widget = _media_manager->createMediaWidget("main", this);

    _createActions();// Creates callback functions

    // Install event filter on the dock manager to catch events from docked widgets
    _m_DockManager->installEventFilter(this);
    // Also install on the main window for events not captured by dock manager
    this->installEventFilter(this);
    // Install application-wide event filter for targeted key interception
    qApp->installEventFilter(this);

    _buildInitialLayout();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::_buildInitialLayout() {
    // Initialize the zone manager first
    // This creates placeholder areas for Left/Center/Right/Bottom zones
    _zone_manager->initializeZones();

    // Configure zone proportions
    _zone_manager->setZoneWidthRatios(0.18f, 0.64f, 0.18f);  // Left, Center, Right
    _zone_manager->setBottomHeightRatio(0.10f);              // Bottom

    // === LEFT ZONE: Data selection and navigation ===
    
    // Add GroupManagementWidget to left zone (first, will be on top)
    if (_group_management_widget) {
        auto * group_dock = new ads::CDockWidget(QStringLiteral("group_management"));
        group_dock->setWidget(_group_management_widget);
        group_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        _zone_manager->addToZone(group_dock, Zone::Left);
    }
    
    // Add DataManager_Widget below GroupManagement in left zone
    if (_data_manager_widget) {
        auto * dm_dock = new ads::CDockWidget(QStringLiteral("data_manager"));
        dm_dock->setWidget(_data_manager_widget);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        _zone_manager->addBelowInZone(dm_dock, Zone::Left, 0.25f);  // Group gets 25%, DM gets 75%
    }

    // === CENTER ZONE: Primary visualization ===
    
    // Add media widget to center zone
    auto * media_dock = new ads::CDockWidget(QStringLiteral("media"));
    media_dock->setWidget(_media_widget);
    media_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    _zone_manager->addToZone(media_dock, Zone::Center);

    // === BOTTOM ZONE: Timeline ===
    
    // Add time scrollbar to bottom zone
    auto * scrollbar_dock = new ads::CDockWidget(QStringLiteral("scrollbar"));
    scrollbar_dock->setWidget(_time_scrollbar);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    _zone_manager->addToZone(scrollbar_dock, Zone::Bottom);

    // === RIGHT ZONE: Properties ===
    
    // Create and add PropertiesHost to right zone
    _properties_host = new PropertiesHost(_editor_registry.get(), this);
    auto * props_dock = new ads::CDockWidget(QStringLiteral("properties"));
    props_dock->setWidget(_properties_host);
    props_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    _zone_manager->addToZone(props_dock, Zone::Right);
}

void MainWindow::_createActions() {
    connect(ui->actionLoad_Video, &QAction::triggered, this, &MainWindow::Load_Video);

    connect(ui->actionLoad_Images, &QAction::triggered, this, &MainWindow::Load_Images);

    connect(ui->actionLoad_JSON_Config, &QAction::triggered, this, &MainWindow::_loadJSONConfig);

    connect(_time_scrollbar, &TimeScrollBar::timeChanged, _media_manager.get(), &MediaWidgetManager::loadFrameForAll);

    connect(ui->actionWhisker_Tracking, &QAction::triggered, this, &MainWindow::openWhiskerTracking);
    connect(ui->actionTongue_Tracking, &QAction::triggered, this, &MainWindow::openTongueTracking);
    connect(ui->actionMachine_Learning, &QAction::triggered, this, &MainWindow::openMLWidget);
    connect(ui->actionData_Viewer, &QAction::triggered, this, &MainWindow::openDataViewer);
    connect(ui->actionNew_Media_Widget, &QAction::triggered, this, &MainWindow::openNewMediaWidget);
    connect(ui->actionBatch_Processing, &QAction::triggered, this, &MainWindow::openBatchProcessingWidget);
    connect(ui->actionLoad_Points, &QAction::triggered, this, &MainWindow::openPointLoaderWidget);
    connect(ui->actionLoad_Masks, &QAction::triggered, this, &MainWindow::openMaskLoaderWidget);
    connect(ui->actionLoad_Lines, &QAction::triggered, this, &MainWindow::openLineLoaderWidget);
    connect(ui->actionLoad_Intervals, &QAction::triggered, this, &MainWindow::openIntervalLoaderWidget);
    connect(ui->actionLoad_Events, &QAction::triggered, this, &MainWindow::openEventLoaderWidget);
    connect(ui->actionLoad_Tensor, &QAction::triggered, this, &MainWindow::openTensorLoaderWidget);
    connect(ui->actionData_Manager, &QAction::triggered, this, &MainWindow::openDataManager);
    connect(ui->actionGroup_Management, &QAction::triggered, this, &MainWindow::openGroupManagement);
    connect(ui->actionExport_Video, &QAction::triggered, this, &MainWindow::openVideoExportWidget);
    connect(ui->actionData_Transforms, &QAction::triggered, this, &MainWindow::openDataTransforms);
    connect(ui->actionTerminal_Output, &QAction::triggered, this, &MainWindow::openTerminalWidget);
    connect(ui->actionAnalysis_Dashboard, &QAction::triggered, this, &MainWindow::openAnalysisDashboard);
    connect(ui->actionTime_Scroll_Bar, &QAction::triggered, this, &MainWindow::openTimeScrollBar);
    connect(ui->actionTable_Designer, &QAction::triggered, this, &MainWindow::openTableDesignerWidget);
    connect(ui->actionTest_Widget, &QAction::triggered, this, &MainWindow::openTestWidget);

    // Zoom actions (custom handling)
    if (ui->actionZoom_In && _media_widget) {
        ui->actionZoom_In->setShortcuts({}); // clear
        ui->actionZoom_In->setShortcut(QKeySequence());
        // Explicit shortcuts below; set text AFTER clearing to force display
        ui->actionZoom_In->setText("Zoom In\tCtrl+");
        connect(ui->actionZoom_In, &QAction::triggered, this, [this]() { _media_widget->zoomIn(); });
        auto * s1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this); // Ctrl++ (numpad or main with shift)
        s1->setContext(Qt::ApplicationShortcut);
        connect(s1, &QShortcut::activated, this, [this]() { _media_widget->zoomIn(); });
        auto * s2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this); // Ctrl+= (produces '+')
        s2->setContext(Qt::ApplicationShortcut);
        connect(s2, &QShortcut::activated, this, [this]() { _media_widget->zoomIn(); });
    }
    if (ui->actionZoom_Out && _media_widget) {
        ui->actionZoom_Out->setShortcuts({});
        ui->actionZoom_Out->setShortcut(QKeySequence());
        ui->actionZoom_Out->setText("Zoom Out\tCtrl-");
        connect(ui->actionZoom_Out, &QAction::triggered, this, [this]() { _media_widget->zoomOut(); });
        auto * s1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
        s1->setContext(Qt::ApplicationShortcut);
        connect(s1, &QShortcut::activated, this, [this]() { _media_widget->zoomOut(); });
    }
}

/*
The Load_Video callback is executed whenever a the load_video option is selected.
If a video is selected, that video will be loaded and the first frame will be
drawn on the video screen.

*/
void MainWindow::Load_Video() {
    auto vid_name = QFileDialog::getOpenFileName(
            this,
            "Load Video File",
            QDir::currentPath(),
            "All files (*.*) ;; MP4 (*.mp4); HDF5 (*.h5); MAT (*.mat)");

    if (vid_name.isNull()) {
        return;
    }

    // Use the conditional video loader
    if (loadVideoData(vid_name.toStdString(), _data_manager.get())) {
        loadData();
    }
}

void MainWindow::Load_Images() {
    auto dir_name = QFileDialog::getExistingDirectory(
            this,
            "Load Video File",
            QDir::currentPath());

    if (dir_name.isNull()) {
        return;
    }

    auto media = std::make_shared<ImageData>();
    media->LoadMedia(dir_name.toStdString());
    _data_manager->setData<ImageData>("media", media, TimeKey("time"));

    loadData();
}

void MainWindow::_loadJSONConfig() {
    auto filename = QFileDialog::getOpenFileName(
            this,
            "Load JSON File",
            QDir::currentPath(),
            "All files (*.*) ;; JSON (*.json)");

    if (filename.isNull()) {
        return;
    }

    // Create progress dialog without cancel button
    QProgressDialog progress("Preparing to load data...", nullptr, 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);  // Show immediately
    progress.setCancelButton(nullptr);  // Remove cancel button
    progress.setValue(0);
    progress.show();
    
    // Force the dialog to appear before loading starts
    QCoreApplication::processEvents();

    // Create progress callback
    auto progress_callback = [&progress](int current, int total, std::string const & message) -> bool {
        // Update progress bar
        if (total > 0) {
            int percent = (current * 100) / total;
            progress.setValue(percent);
        }
        
        // Update label text
        progress.setLabelText(QString::fromStdString(message));
        
        // Process events to keep UI responsive
        QCoreApplication::processEvents();
        
        // Always return true since we removed cancel functionality
        return true;
    };

    // Load data with progress tracking
    auto data_info = load_data_from_json_config(_data_manager.get(), filename.toStdString(), progress_callback);
    
    // Set to 100% when complete
    progress.setValue(100);
    
    processLoadedData(data_info);
}

void MainWindow::processLoadedData(std::vector<DataInfo> const & data_info) {
    bool hasMediaData = false;
    
    for (auto const & data: data_info) {
        if (data.data_class == "VideoData") {
            hasMediaData = true;
        } else if (data.data_class == "ImageData") {
            hasMediaData = true;
        } else if (
                (data.data_class == "PointData") ||
                (data.data_class == "MaskData") ||
                (data.data_class == "LineData")) {
            // Only set color if one is specified, otherwise let Media_Window auto-assign
            if (!data.color.empty()) {
                _media_manager->setFeatureColorForAll(data.key, data.color);
            }
        }
    }
    
    // Only update media-related components if we loaded media data
    if (hasMediaData) {
        loadData();
    } else {
        // If no media data was loaded, we might still need to update the time scrollbar
        // if new time-based data was added
        _updateFrameCount();
    }
}

void MainWindow::loadData() {

    _updateFrameCount();

    _media_manager->updateMediaForAll();
}

void MainWindow::_updateFrameCount() {
    auto media = _data_manager->getData<MediaData>("media");

    if (_data_manager->getTime()->getTotalFrameCount() != media->getTotalFrameCount()) {

        auto frame_count = media->getTotalFrameCount();

        std::cout << "There is a mismatch between the time in the time vector and number of samples in the video"
                  << "The video has " << frame_count
                  << " the time vector has " << _data_manager->getTime()->getTotalFrameCount() << std::endl;

        if (_data_manager->getTime()->getTotalFrameCount() == 0) {
            std::vector<int> t(frame_count);
            std::iota(std::begin(t), std::end(t), 0);

            auto new_timeframe = std::make_shared<TimeFrame>(t);

            _data_manager->removeTime(TimeKey("time"));
            _data_manager->setTime(TimeKey("time"), new_timeframe, true);
        } else {    
            std::cout << "The time vector is not empty, so we will not create a new time vector" << std::endl;
        
        
        }
    }

    _time_scrollbar->updateScrollBarNewMax(_data_manager->getTime()->getTotalFrameCount() - 1);

    _time_scrollbar->changeScrollBarValue(0);
}

void MainWindow::openWhiskerTracking() {
    std::string const key = "whisker_widget";

    if (!_widgets.contains(key)) {
        auto whiskerWidget = std::make_unique<Whisker_Widget>(
                _data_manager);
        connect(_time_scrollbar, &TimeScrollBar::timeChanged, whiskerWidget.get(), &Whisker_Widget::LoadFrame);

        _widgets[key] = std::move(whiskerWidget);
        registerDockWidget(key, _widgets[key].get(), ads::RightDockWidgetArea);
    }

    dynamic_cast<Whisker_Widget *>(_widgets[key].get())->openWidget();
    showDockWidget(key);
}

void MainWindow::registerDockWidget(std::string const & key, QWidget * widget, ads::DockWidgetArea area) {
    auto dock_widget = new ads::CDockWidget(QString::fromStdString(key));
    //dock_widget->setWidget(widget, ads::CDockWidget::ForceScrollArea);
    dock_widget->setWidget(widget);
    _m_DockManager->addDockWidget(area, dock_widget);
}

void MainWindow::showDockWidget(std::string const & key) {
    auto* dockWidget = _m_DockManager->findDockWidget(QString::fromStdString(key));
    if (!dockWidget) {
        return;
    }
    
    // If the widget is not visible and is in a floating container, dock it back
    // This provides better UX on Linux where native title bars are used
    if (!dockWidget->isVisible()) {
        auto* dockContainer = dockWidget->dockContainer();
        // Check if it's in a floating container (not the main dock manager)
        if (dockContainer && dockContainer->isFloating()) {
            // Determine the appropriate dock area based on widget type
            ads::DockWidgetArea dockArea = ads::RightDockWidgetArea; // default
            
            // Time scrollbar should go to the bottom
            if (key == "scrollbar") {
                dockArea = ads::BottomDockWidgetArea;
            }
            
            // Move the widget back to the main dock area before showing
            _m_DockManager->addDockWidget(dockArea, dockWidget);
        }
    }
    
    dockWidget->toggleView();
}

ads::CDockWidget * MainWindow::findDockWidget(std::string const & key) const {
    return _m_DockManager->findDockWidget(QString::fromStdString(key));
}

void MainWindow::openTongueTracking() {
    std::string const key = "tongue_widget";

    if (!_widgets.contains(key)) {
        auto tongueWidget = std::make_unique<Tongue_Widget>(_data_manager);
        tongueWidget->setObjectName(key);
        registerDockWidget(key, tongueWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(tongueWidget);
    }

    auto ptr = dynamic_cast<Tongue_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openMLWidget() {
    std::string const key = "ML_widget";

    if (!_widgets.contains(key)) {
        auto MLWidget = std::make_unique<ML_Widget>(
                _data_manager);

        MLWidget->setObjectName(key);
        registerDockWidget(key, MLWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(MLWidget);
    }

    auto ptr = dynamic_cast<ML_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openDataViewer() {
    std::string const key = "DataViewer_widget";

    if (!_widgets.contains(key)) {
        auto DataViewerWidget = std::make_unique<DataViewer_Widget>(
                _data_manager,
                _time_scrollbar);

        DataViewerWidget->setObjectName(key);
        
        // Insert DataViewer between media and scrollbar
        // First, find the scrollbar dock widget
        auto scrollbar_dock = findDockWidget("scrollbar");
        if (scrollbar_dock && scrollbar_dock->dockAreaWidget()) {
            // Create dock widget for DataViewer
            auto dataviewer_dock = new ads::CDockWidget(QString::fromStdString(key));
            dataviewer_dock->setWidget(DataViewerWidget.get());
            dataviewer_dock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
            dataviewer_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
            
            // Add DataViewer above the scrollbar (in the same dock area)
            _m_DockManager->addDockWidget(ads::TopDockWidgetArea, dataviewer_dock, scrollbar_dock->dockAreaWidget());
            
            // Adjust splitter: give DataViewer most space, scrollbar minimal space
            // The splitter now contains: media, dataviewer, scrollbar
            auto media_dock = findDockWidget("media");
            if (media_dock) {
                auto * main_splitter = ads::internal::findParent<ads::CDockSplitter *>(media_dock->widget());
                if (main_splitter) {
                    int const height = main_splitter->height();
                    // Split: 45% media, 50% dataviewer, 5% scrollbar
                    main_splitter->setSizes({height * 45 / 100, height * 50 / 100, height * 5 / 100});
                }
            }
        } else {
            // Fallback to old behavior if scrollbar dock not found
            registerDockWidget(key, DataViewerWidget.get(), ads::RightDockWidgetArea);
        }
        
        _widgets[key] = std::move(DataViewerWidget);
    }

    auto ptr = dynamic_cast<DataViewer_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openNewMediaWidget() {
    // Generate unique ID for the new media widget
    std::string const key = "MediaWidget_" + std::to_string(_media_widget_counter++);
    
    // Create the media widget through the manager
    auto* media_widget = _media_manager->createMediaWidget(key, this);
    if (!media_widget) {
        std::cerr << "Failed to create media widget with ID: " << key << std::endl;
        return;
    }
    
    // Register the dock widget in the system
    registerDockWidget(key, media_widget, ads::RightDockWidgetArea);
    
    // Find the dock widget that was just created and connect close signal
    auto* dock_widget = findDockWidget(key);
    if (dock_widget) {
        connect(dock_widget, &ads::CDockWidget::closed, this, [this, key]() {
            // Remove from media manager (this will properly destroy the widget)
            _media_manager->removeMediaWidget(key);
            std::cout << "Media widget " << key << " destroyed on close" << std::endl;
        });
    }
    
    // Show the dock widget
    showDockWidget(key);
    
    std::cout << "Created new media widget: " << key << std::endl;
}

void MainWindow::openBatchProcessingWidget() {
    std::string const key = "BatchProcessing_widget";

    if (!_widgets.contains(key)) {
        auto batchProcessingWidget = std::make_unique<BatchProcessing_Widget>(_data_manager, this, this);

        batchProcessingWidget->setObjectName(key);
        registerDockWidget(key, batchProcessingWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(batchProcessingWidget);
    }

    auto ptr = dynamic_cast<BatchProcessing_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

bool MainWindow::eventFilter(QObject * obj, QEvent * event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent * keyEvent = static_cast<QKeyEvent *>(event);
        
        // Handle spacebar for play/pause (unless in text input widget)
        if (keyEvent->key() == Qt::Key_Space && keyEvent->modifiers() == Qt::NoModifier) {
            QWidget * focusedWidget = QApplication::focusWidget();
            
            // Don't intercept spacebar if we're in a text input widget
            if (focusedWidget) {
                if (qobject_cast<QLineEdit *>(focusedWidget) || 
                    qobject_cast<QTextEdit *>(focusedWidget) || 
                    qobject_cast<QPlainTextEdit *>(focusedWidget)) {
                    // Let the text widget handle the space normally
                    return false;
                }
            }
            
            // Toggle play/pause
            _time_scrollbar->PlayButton();
            return true; // Event handled
        }
        
        // Always handle Ctrl+Left/Right for frame navigation regardless of focus
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_Right) {
                _time_scrollbar->changeScrollBarValue(_time_scrollbar->getFrameJumpValue(), true);
                return true; // Event handled, don't pass it on
            } else if (keyEvent->key() == Qt::Key_Left) {
                _time_scrollbar->changeScrollBarValue(-_time_scrollbar->getFrameJumpValue(), true);
                return true; // Event handled, don't pass it on
            }
        }
        
        // For plain arrow keys, be very selective
        if (keyEvent->modifiers() == Qt::NoModifier && 
            (keyEvent->key() == Qt::Key_Right || keyEvent->key() == Qt::Key_Left)) {
            
            QWidget * focusedWidget = QApplication::focusWidget();
            
            // If we're in a text input widget, let it handle the arrow keys for cursor movement
            if (focusedWidget) {
                // Check for text input widgets
                if (qobject_cast<QLineEdit *>(focusedWidget) || 
                    qobject_cast<QTextEdit *>(focusedWidget) || 
                    qobject_cast<QPlainTextEdit *>(focusedWidget)) {
                    // Let the text widget handle the event normally
                    return false;
                }
                
                // Check if we're in a widget that should handle arrow keys (like combo boxes, lists, etc.)
                if (qobject_cast<QComboBox *>(focusedWidget) || 
                    qobject_cast<QListWidget *>(focusedWidget) ||
                    qobject_cast<QTableWidget *>(focusedWidget) ||
                    qobject_cast<QTreeWidget *>(focusedWidget)) {
                    // Let these widgets handle their own arrow key navigation
                    return false;
                }
            }
            
            // Otherwise, use arrow keys for frame navigation
            if (keyEvent->key() == Qt::Key_Right) {
                _time_scrollbar->changeScrollBarValue(_time_scrollbar->getFrameJumpValue(), true);
                return true; // Event handled
            } else if (keyEvent->key() == Qt::Key_Left) {
                _time_scrollbar->changeScrollBarValue(-_time_scrollbar->getFrameJumpValue(), true);
                return true; // Event handled
            }
        }
        
        // For all other keys, let them pass through to the focused widget
        qDebug() << "MainWindow::eventFilter - Passing key through to focused widget";
        return false;
    }
    
    // For all other events, let them be handled normally
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent * event) {
    // The event filter handles most key events now
    // This is mainly for direct main window key events
    QMainWindow::keyPressEvent(event);
}

void MainWindow::openPointLoaderWidget() {
    std::string const key = "PointLoader_widget";

    if (!_widgets.contains(key)) {
        auto PointLoaderWidget = std::make_unique<Point_Loader_Widget>(
                _data_manager,
                this);

        PointLoaderWidget->setObjectName(key);
        registerDockWidget(key, PointLoaderWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(PointLoaderWidget);
    }

    auto ptr = dynamic_cast<Point_Loader_Widget *>(_widgets[key].get());

    showDockWidget(key);
}

void MainWindow::openMaskLoaderWidget() {
    std::string const key = "MaskLoader_widget";

    if (!_widgets.contains(key)) {
        auto MaskLoaderWidget = std::make_unique<Mask_Loader_Widget>(
                _data_manager,
                this);

        MaskLoaderWidget->setObjectName(key);
        registerDockWidget(key, MaskLoaderWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(MaskLoaderWidget);
    }

    auto ptr = dynamic_cast<Mask_Loader_Widget *>(_widgets[key].get());

    showDockWidget(key);
}

void MainWindow::openLineLoaderWidget() {
    std::string const key = "LineLoader_widget";

    if (!_widgets.contains(key)) {
        auto LineLoaderWidget = std::make_unique<Line_Loader_Widget>(
                _data_manager,
                this);

        LineLoaderWidget->setObjectName(key);
        registerDockWidget(key, LineLoaderWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(LineLoaderWidget);
    }

    auto ptr = dynamic_cast<Line_Loader_Widget *>(_widgets[key].get());

    showDockWidget(key);
}

void MainWindow::openIntervalLoaderWidget() {
    std::string const key = "IntervalLoader_widget";

    if (!_widgets.contains(key)) {
        auto DigitalIntervalLoaderWidget = std::make_unique<Digital_Interval_Loader_Widget>(
                _data_manager,
                this);

        DigitalIntervalLoaderWidget->setObjectName(key);
        registerDockWidget(key, DigitalIntervalLoaderWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(DigitalIntervalLoaderWidget);
    }

    auto ptr = dynamic_cast<Digital_Interval_Loader_Widget *>(_widgets[key].get());

    showDockWidget(key);
}

void MainWindow::openEventLoaderWidget() {
    std::string const key = "EventLoader_widget";

    if (!_widgets.contains(key)) {
        auto DigitalEventLoaderWidget = std::make_unique<Digital_Event_Loader_Widget>(
                _data_manager,
                this);

        DigitalEventLoaderWidget->setObjectName(key);
        registerDockWidget(key, DigitalEventLoaderWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(DigitalEventLoaderWidget);
    }

    auto ptr = dynamic_cast<Digital_Event_Loader_Widget *>(_widgets[key].get());

    showDockWidget(key);
}

void MainWindow::openTensorLoaderWidget() {
    std::string const key = "TensorLoader_widget";

    if (!_widgets.contains(key)) {
        auto tensor_loader_widget = std::make_unique<Tensor_Loader_Widget>(
                _data_manager,
                this);

        tensor_loader_widget->setObjectName(key);
        registerDockWidget(key, tensor_loader_widget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(tensor_loader_widget);
    }

    auto ptr = dynamic_cast<Tensor_Loader_Widget *>(_widgets[key].get());

    showDockWidget(key);
}

void MainWindow::openDataManager() {
    // Find and show the existing data manager dock widget
    auto dock_widget = findDockWidget("data_manager");
    if (dock_widget) {
        dock_widget->show();
        dock_widget->raise();
        dock_widget->setAsCurrentTab();
    }
    
    // Open the widget (call any initialization methods if needed)
    if (_data_manager_widget) {
        _data_manager_widget->openWidget();
    }

    showDockWidget("data_manager");
}

void MainWindow::openVideoExportWidget() {
    std::string const key = "VideoExport_widget";

    if (!_widgets.contains(key)) {
        auto vid_widget = std::make_unique<Export_Video_Widget>(
                _data_manager,
                _media_manager.get(),
                _time_scrollbar,
                this);

        vid_widget->setObjectName(key);
        registerDockWidget(key, vid_widget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(vid_widget);
    }

    auto ptr = dynamic_cast<Export_Video_Widget *>(_widgets[key].get());
    //connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, ptr, &DataManager_Widget::LoadFrame);
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openDataTransforms() {
    std::string const key = "DataTransform_widget";

    if (!_widgets.contains(key)) {
        auto dt_widget = std::make_unique<DataTransform_Widget>(
                _data_manager,
                _editor_registry.get(),
                this);

        dt_widget->setObjectName(key);

        // Set explicit minimum size constraints - increased for better visibility
        dt_widget->setMinimumSize(350, 700);
        dt_widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

        // Create dock widget with appropriate settings
        auto dock_widget = new ads::CDockWidget(QString::fromStdString(key));

        // Use ForceNoScrollArea to prevent adding another scroll area
        dock_widget->setWidget(dt_widget.get(), ads::CDockWidget::ForceNoScrollArea);

        // Ensure the dock widget sizes based on content
        dock_widget->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);

        _m_DockManager->addDockWidget(ads::RightDockWidgetArea, dock_widget);

        _widgets[key] = std::move(dt_widget);
    }

    auto ptr = dynamic_cast<DataTransform_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openTerminalWidget() {
    std::string const key = "Terminal_widget";

    if (!_widgets.contains(key)) {
        auto terminal_widget = std::make_unique<TerminalWidget>(this);

        terminal_widget->setObjectName(key);
        registerDockWidget(key, terminal_widget.get(), ads::BottomDockWidgetArea);
        _widgets[key] = std::move(terminal_widget);
    }

    auto ptr = dynamic_cast<TerminalWidget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openAnalysisDashboard() {
    std::string const key = "Analysis_Dashboard_widget";

    if (!_widgets.contains(key)) {
        auto analysis_dashboard_widget = std::make_unique<Analysis_Dashboard>(
                _data_manager,
                _group_manager.get(),
                _time_scrollbar,
                _m_DockManager,
                this);

        analysis_dashboard_widget->setObjectName(key);
        registerDockWidget(key, analysis_dashboard_widget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(analysis_dashboard_widget);
    }

    auto ptr = dynamic_cast<Analysis_Dashboard *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openTimeScrollBar() {
    // Toggle visibility of the time scroll bar dock widget
    showDockWidget("scrollbar");
}

void MainWindow::openTableDesignerWidget() {
    std::string const key = "TableDesigner_widget";

    if (!_widgets.contains(key)) {
        auto td_widget = std::make_unique<TableDesignerWidget>(_data_manager, this);
        td_widget->setObjectName(QString::fromStdString(key));
        registerDockWidget(key, td_widget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(td_widget);
    }

    showDockWidget(key);
}

void MainWindow::openGroupManagement() {
    // Show the group management widget if it exists
    if (_group_management_widget) {
        showDockWidget("group_management");
    }
}

void MainWindow::_registerEditorTypes() {
    // Capture 'this' and data_manager for lambdas
    auto dm = _data_manager;

    // === Module-based registration ===
    // Each module defines its own factory functions - MainWindow doesn't need
    // to know implementation details like MediaWidgetState, MediaViewWidget, etc.
    
    MediaWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    // === Test Widget (View/Properties split proof-of-concept) ===
    // This is kept inline as an example of direct registration
    _editor_registry->registerType({
        .type_id = QStringLiteral("TestWidget"),
        .display_name = QStringLiteral("Test Widget"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Development"),
        .default_zone = QStringLiteral("center"),  // View goes to center, properties go to PropertiesHost
        .allow_multiple = false,  // Single instance only
        // State factory
        .create_state = [dm]() { return std::make_shared<TestWidgetState>(dm); },
        // View factory
        .create_view = [](std::shared_ptr<EditorState> state) {
            auto test_state = std::dynamic_pointer_cast<TestWidgetState>(state);
            return new TestWidgetView(test_state);
        },
        // Properties factory (used by PropertiesHost)
        .create_properties = [](std::shared_ptr<EditorState> state) {
            auto test_state = std::dynamic_pointer_cast<TestWidgetState>(state);
            return new TestWidgetProperties(test_state);
        }
    });

    // Future: Add more module registrations here
    // DataViewerModule::registerTypes(_editor_registry.get(), _data_manager);
    // AnalysisDashboardModule::registerTypes(_editor_registry.get(), _data_manager);
}

void MainWindow::openEditor(QString const & type_id) {
    auto info = _editor_registry->typeInfo(type_id);

    if (info.type_id.isEmpty()) {
        std::cerr << "MainWindow::openEditor: Unknown editor type: "
                  << type_id.toStdString() << std::endl;
        return;
    }

    // For single-instance editors, check if already open
    if (!info.allow_multiple) {
        auto existing = _editor_registry->statesByType(type_id);
        if (!existing.empty()) {
            // Find and show the existing widget
            QString instance_id = existing[0]->getInstanceId();
            std::string key = instance_id.toStdString();
            if (_widgets.contains(key)) {
                showDockWidget(key);
                // Also set this as the active editor so PropertiesHost shows its properties
                _editor_registry->selectionContext()->setActiveEditor(instance_id);
                return;
            }
            // State exists but widget doesn't - unusual, recreate widget
            std::cerr << "MainWindow::openEditor: State exists but widget missing, recreating: "
                      << type_id.toStdString() << std::endl;
            // Unregister the orphan state
            _editor_registry->unregisterState(instance_id);
        }
    }

    // Create new instance via registry
    auto instance = _editor_registry->createEditor(type_id);

    if (!instance.state || !instance.view) {
        std::cerr << "MainWindow::openEditor: Failed to create editor: "
                  << type_id.toStdString() << std::endl;
        return;
    }

    QString instance_id = instance.state->getInstanceId();
    std::string key = instance_id.toStdString();

    // View goes to its designated zone (typically Center)
    // Properties will be shown in PropertiesHost when this editor is active
    QWidget * main_widget = instance.view;
    main_widget->setObjectName(instance_id);

    // Create dock widget for the view
    auto * dock_widget = new ads::CDockWidget(instance_id);
    dock_widget->setWidget(main_widget, ads::CDockWidget::ForceNoScrollArea);
    dock_widget->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    dock_widget->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);

    // Determine zone from type info
    Zone zone = zoneFromString(info.default_zone);
    
    // Use ZoneManager if initialized, otherwise fall back to direct dock manager
    if (_zone_manager && _zone_manager->zonesInitialized()) {
        _zone_manager->addToZone(dock_widget, zone);
    } else {
        // Fallback: convert zone to dock area
        ads::DockWidgetArea area = ads::CenterDockWidgetArea;
        switch (zone) {
        case Zone::Left:   area = ads::LeftDockWidgetArea; break;
        case Zone::Center: area = ads::CenterDockWidgetArea; break;
        case Zone::Right:  area = ads::RightDockWidgetArea; break;
        case Zone::Bottom: area = ads::BottomDockWidgetArea; break;
        }
        _m_DockManager->addDockWidget(area, dock_widget);
    }

    _widgets[key] = std::unique_ptr<QWidget>(main_widget);

    // Set this editor as active so PropertiesHost shows its properties
    _editor_registry->selectionContext()->setActiveEditor(instance_id);

    std::cout << "Created " << info.display_name.toStdString()
              << " via EditorRegistry (instance: " << key << ", zone: "
              << zoneToString(zone).toStdString() << ")" << std::endl;

    showDockWidget(key);
}

void MainWindow::openTestWidget() {
    // Delegate to generic openEditor using EditorRegistry
    openEditor(QStringLiteral("TestWidget"));
}
