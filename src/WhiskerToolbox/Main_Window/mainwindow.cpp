#include "mainwindow.hpp"

#include "ui_mainwindow.h"
#include "video_loader.hpp"

#include "DataManager.hpp"
#include "DataManager/Media/Image_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Media/Video_Data.hpp"

#include "Analysis_Dashboard/Analysis_Dashboard.hpp"
#include "DataManager_Widget/DataManager_Widget.hpp"
#include "DataTransform_Widget/DataTransform_Widget.hpp"
#include "DockAreaWidget.h"
#include "DockSplitter.h"
#include "EditorCreationController.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/SelectionContext.hpp"
#include "GroupManagementWidget/GroupManagementWidget.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "Media_Widget/DisplayOptionsRegistry.hpp"
#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "ZoneManager.hpp"


// Module registration headers - each module defines its own factory functions
#include "BatchProcessing_Widget/BatchProcessingWidgetRegistration.hpp"
#include "DataImport_Widget/DataImportWidgetRegistration.hpp"
#include "DataInspector_Widget/DataInspectorWidgetRegistration.hpp"
#include "DataManager_Widget/DataManagerWidgetRegistration.hpp"
#include "DataTransform_Widget/DataTransformWidgetRegistration.hpp"
#include "DataViewer_Widget/DataViewerWidgetRegistration.hpp"
#include "Export_Widgets/Export_Video_Widget/ExportVideoWidgetRegistration.hpp"
#include "GroupManagementWidget/GroupManagementWidgetRegistration.hpp"
#include "Media_Widget/MediaWidgetRegistration.hpp"
#include "ML_Widget/MLWidgetRegistration.hpp"
#include "Terminal_Widget/TerminalWidgetRegistration.hpp"
#include "TableDesignerWidget/TableDesignerWidgetRegistration.hpp"
#include "Test_Widget/TestWidgetRegistration.hpp"
#include "TimeScrollBar/TimeScrollBarRegistration.hpp"
#include "Tongue_Widget/TongueWidgetRegistration.hpp"
#include "Whisker_Widget/WhiskerWidgetRegistration.hpp"
#include "ZoneManagerWidgetRegistration.hpp"

#include "TimeScrollBar/TimeScrollBar.hpp"
#include "TimeScrollBar/TimeScrollBarState.hpp"

#include "utils/DataLoadUtils.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QShortcut>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>

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
      _group_manager(nullptr)

{
    ui->setupUi(this);

    // === FIX START ===
    // If the UI file created a central widget, delete it so ADS can take over the full window.
    // ADS manages its own central container.
    if (centralWidget()) {
        auto * central = takeCentralWidget();
        delete central;
    }
    // === FIX END ===

    // Configure dock manager BEFORE creating it
    // Using native title bars for floating widgets (works smoothly on all platforms)
    // On Linux, native title bars allow proper window dragging
    // Re-docking is handled in showDockWidget() - closed floating widgets
    // are automatically re-docked when reopened via the modules menu
    ads::CDockManager::setConfigFlags(ads::CDockManager::DefaultOpaqueConfig | ads::CDockManager::OpaqueSplitterResize | ads::CDockManager::DragPreviewIsDynamic);

    _m_DockManager = new ads::CDockManager(this);

    // Create ZoneManager to manage standard UI zones
    _zone_manager = std::make_unique<ZoneManager>(_m_DockManager, this);

    // Create EditorCreationController to handle unified editor creation and zone placement
    _editor_creation_controller = std::make_unique<EditorCreationController>(
            _editor_registry.get(), _zone_manager.get(), _m_DockManager, this);

    //This is necessary to accept keyboard events
    this->setFocusPolicy(Qt::StrongFocus);

    // Create the GroupManager with the DataManager's EntityGroupManager
    // Note: GroupManagementWidget is created via EditorCreationController in _buildInitialLayout
    auto * entity_group_manager = _data_manager->getEntityGroupManager();
    if (entity_group_manager) {
        _group_manager = std::make_unique<GroupManager>(entity_group_manager, _data_manager, this);
    }

    // Create TimeScrollBar with state support
    // Note: TimeScrollBar is created here (before registration) because other modules
    // need a reference to it during their registration (e.g., ExportVideoWidget, WhiskerWidget)
    _time_scrollbar_state = std::make_shared<TimeScrollBarState>();
    _time_scrollbar = new TimeScrollBar(_data_manager, _time_scrollbar_state, this);

    // Register editor types with the factory
    // Must be called AFTER creating dependencies (TimeScrollBar, GroupManager)
    _registerEditorTypes();

    // Register the TimeScrollBar state that was created before registration
    // This is done after registration so the type is known
    _editor_registry->registerState(_time_scrollbar_state);

    _verbose = false;

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
    _zone_manager->setZoneWidthRatios(0.20f, 0.58f, 0.22f);// Left, Center, Right
    _zone_manager->setBottomHeightRatio(0.14f);            // Bottom

    // === LEFT ZONE: Data selection and navigation ===
    // Layout: GroupManagementWidget (30% height) on top, DataManager_Widget (70% height) below

    // Create GroupManagementWidget first (goes to top of left zone)
    auto placed_group = _editor_creation_controller->createAndPlaceWithTitle(
            EditorLib::EditorTypeId(QStringLiteral("GroupManagementWidget")),
            QStringLiteral("Group Manager"),
            true);// raise_view

    if (placed_group.view_dock) {
        // Mark as non-closable since it's a core navigation widget
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
    }

    // Create DataManager_Widget - we need manual placement with split
    // Create via EditorRegistry but don't use controller's automatic placement
    auto dm_instance = _editor_registry->createEditor(EditorLib::EditorTypeId(QStringLiteral("DataManagerWidget")));
    if (dm_instance.state && dm_instance.view) {
        auto * dm_dock = new ads::CDockWidget(QStringLiteral("Data Manager"));
        dm_dock->setWidget(dm_instance.view);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);

        // Add below GroupManagement with 30/70 split (0.3 = top widget gets 30%)
        _zone_manager->addBelowInZone(dm_dock, Zone::Left, 0.30f);
    }

    // === CENTER ZONE: Primary visualization ===

    // Create and add media widget to center zone using EditorCreationController
    // This respects EditorTypeInfo zone preferences and handles dock widget creation
    auto placed_media = _editor_creation_controller->createAndPlaceWithTitle(
            EditorLib::EditorTypeId(QStringLiteral("MediaWidget")),
            QStringLiteral("Media Viewer"),
            true);// raise_view

    if (placed_media.view_dock) {
        // Mark the initial media widget as non-closable
        placed_media.view_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        placed_media.view_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
    }

    // === BOTTOM ZONE: Timeline ===

    // Add time scrollbar to bottom zone
    // Note: TimeScrollBar is created in the constructor (with state) because other modules
    // need it during registration. Here we just wrap it in a dock widget.
    auto * scrollbar_dock = new ads::CDockWidget(QStringLiteral("Timeline"));
    scrollbar_dock->setWidget(_time_scrollbar);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
    _zone_manager->addToZone(scrollbar_dock, Zone::Bottom);

    // === RIGHT ZONE: Properties ===
    // Properties widgets are now placed directly in Zone::Right as persistent tabs
    // by EditorCreationController when editors are created.
    // No more PropertiesHost container - properties persist independently.

    // === Apply Zone Ratios ===
    // Defer the splitter size application to after the window is shown and laid out
    // This ensures the splitters have valid dimensions when sizes are applied
    _zone_manager->reapplySplitterSizes(200);// 200ms delay for layout to stabilize
}

void MainWindow::_createActions() {
    connect(ui->actionLoad_Video, &QAction::triggered, this, &MainWindow::Load_Video);

    connect(ui->actionLoad_Images, &QAction::triggered, this, &MainWindow::Load_Images);

    connect(ui->actionLoad_JSON_Config, &QAction::triggered, this, &MainWindow::_loadJSONConfig);

    // Connect TimeScrollBar to EditorRegistry for global time propagation
    connect(_time_scrollbar, &TimeScrollBar::timeChanged,
            _editor_registry.get(), &EditorRegistry::setCurrentTime);

    connect(ui->actionWhisker_Tracking, &QAction::triggered, this, &MainWindow::openWhiskerTracking);
    connect(ui->actionTongue_Tracking, &QAction::triggered, this, &MainWindow::openTongueTracking);
    connect(ui->actionMachine_Learning, &QAction::triggered, this, &MainWindow::openMLWidget);
    connect(ui->actionData_Viewer, &QAction::triggered, this, &MainWindow::openDataViewer);
    connect(ui->actionNew_Media_Widget, &QAction::triggered, this, &MainWindow::openNewMediaWidget);
    connect(ui->actionBatch_Processing, &QAction::triggered, this, &MainWindow::openBatchProcessingWidget);
    connect(ui->actionData_Manager, &QAction::triggered, this, &MainWindow::openDataManager);
    connect(ui->actionGroup_Management, &QAction::triggered, this, &MainWindow::openGroupManagement);
    connect(ui->actionExport_Video, &QAction::triggered, this, &MainWindow::openVideoExportWidget);
    connect(ui->actionData_Transforms, &QAction::triggered, this, &MainWindow::openDataTransforms);
    connect(ui->actionTerminal_Output, &QAction::triggered, this, &MainWindow::openTerminalWidget);
    connect(ui->actionAnalysis_Dashboard, &QAction::triggered, this, &MainWindow::openAnalysisDashboard);
    connect(ui->actionTime_Scroll_Bar, &QAction::triggered, this, &MainWindow::openTimeScrollBar);
    connect(ui->actionTable_Designer, &QAction::triggered, this, &MainWindow::openTableDesignerWidget);
    connect(ui->actionTest_Widget, &QAction::triggered, this, &MainWindow::openTestWidget);
    connect(ui->actionZone_Layout_Manager, &QAction::triggered, this, &MainWindow::openZoneLayoutManager);
    connect(ui->actionData_Import, &QAction::triggered, this, &MainWindow::openDataImport);

    // Zoom actions - operates on the focused Media_Widget (via SelectionContext)
    // Lambda to find the active Media_Widget based on SelectionContext::activeEditorId
    auto getActiveMediaWidget = [this]() -> Media_Widget * {
        auto * ctx = _editor_registry->selectionContext();
        if (!ctx) return nullptr;

        auto active_id = ctx->activeEditorId();
        if (!active_id.isValid()) return nullptr;

        // Find the Media_Widget with matching state instance_id
        for (auto * dock: _m_DockManager->dockWidgetsMap()) {
            if (auto * mw = dynamic_cast<Media_Widget *>(dock->widget())) {
                if (mw->getState() && mw->getState()->getInstanceId() == active_id.toString()) {
                    return mw;
                }
            }
        }
        return nullptr;
    };

    if (ui->actionZoom_In) {
        ui->actionZoom_In->setShortcuts({});// clear
        ui->actionZoom_In->setShortcut(QKeySequence());
        // Explicit shortcuts below; set text AFTER clearing to force display
        ui->actionZoom_In->setText("Zoom In\tCtrl+");
        connect(ui->actionZoom_In, &QAction::triggered, this, [getActiveMediaWidget]() {
            if (auto * mw = getActiveMediaWidget()) mw->zoomIn();
        });
        auto * s1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);// Ctrl++ (numpad or main with shift)
        s1->setContext(Qt::ApplicationShortcut);
        connect(s1, &QShortcut::activated, this, [getActiveMediaWidget]() {
            if (auto * mw = getActiveMediaWidget()) mw->zoomIn();
        });
        auto * s2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);// Ctrl+= (produces '+')
        s2->setContext(Qt::ApplicationShortcut);
        connect(s2, &QShortcut::activated, this, [getActiveMediaWidget]() {
            if (auto * mw = getActiveMediaWidget()) mw->zoomIn();
        });
    }
    if (ui->actionZoom_Out) {
        ui->actionZoom_Out->setShortcuts({});
        ui->actionZoom_Out->setShortcut(QKeySequence());
        ui->actionZoom_Out->setText("Zoom Out\tCtrl-");
        connect(ui->actionZoom_Out, &QAction::triggered, this, [getActiveMediaWidget]() {
            if (auto * mw = getActiveMediaWidget()) mw->zoomOut();
        });
        auto * s1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
        s1->setContext(Qt::ApplicationShortcut);
        connect(s1, &QShortcut::activated, this, [getActiveMediaWidget]() {
            if (auto * mw = getActiveMediaWidget()) mw->zoomOut();
        });
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
    progress.setMinimumDuration(0);   // Show immediately
    progress.setCancelButton(nullptr);// Remove cancel button
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

    // Load data with progress tracking using the utility function
    // This will:
    // 1. Load data into DataManager (triggers DataManager observers)
    // 2. Emit EditorRegistry::applyDataDisplayConfig (for UI configuration)
    auto data_info = loadDataAndBroadcastConfig(
        _data_manager.get(), 
        _editor_registry.get(), 
        filename.toStdString(), 
        progress_callback);

    // Set to 100% when complete
    progress.setValue(100);

    // Handle media-related updates (TimeScrollBar, Media_Widget refresh)
    // TODO: These should eventually be moved to widgets listening to DataManager observers
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
                // Set feature color on all MediaWidget instances via EditorRegistry
                auto states = _editor_registry->statesByType(EditorTypeId(QStringLiteral("MediaWidget")));
                for (auto const & state: states) {
                    auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(state);
                    if (media_state) {
                        // Use displayOptions registry to set the color
                        // This will be picked up by Media_Window on next UpdateCanvas
                        auto key_q = QString::fromStdString(data.key);
                        if (media_state->displayOptions().has<LineDisplayOptions>(key_q)) {
                            auto opts = *media_state->displayOptions().get<LineDisplayOptions>(key_q);
                            opts.hex_color() = data.color;
                            media_state->displayOptions().set(key_q, opts);
                        }
                    }
                }
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

    // Update media for all MediaWidget instances via dock manager
    // Each widget will refresh its view when time changes (via EditorRegistry::timeChanged)
    // but we also need to call updateMedia() to initialize the graphics view
    for (auto * dock: _m_DockManager->dockWidgetsMap()) {
        if (auto * media_widget = dynamic_cast<Media_Widget *>(dock->widget())) {
            media_widget->updateMedia();
        }
    }
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

void MainWindow::registerDockWidget(std::string const & key, QWidget * widget, ads::DockWidgetArea area) {
    auto dock_widget = new ads::CDockWidget(QString::fromStdString(key));
    //dock_widget->setWidget(widget, ads::CDockWidget::ForceScrollArea);
    dock_widget->setWidget(widget);
    _m_DockManager->addDockWidget(area, dock_widget);
}

void MainWindow::showDockWidget(std::string const & key) {
    auto * dockWidget = _m_DockManager->findDockWidget(QString::fromStdString(key));
    if (!dockWidget) {
        return;
    }

    // If the widget is not visible and is in a floating container, dock it back
    // This provides better UX on Linux where native title bars are used
    if (!dockWidget->isVisible()) {
        auto * dockContainer = dockWidget->dockContainer();
        // Check if it's in a floating container (not the main dock manager)
        if (dockContainer && dockContainer->isFloating()) {
            // Determine the appropriate dock area based on widget type
            ads::DockWidgetArea dockArea = ads::RightDockWidgetArea;// default

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
            return true;// Event handled
        }

        // Always handle Ctrl+Left/Right for frame navigation regardless of focus
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            if (keyEvent->key() == Qt::Key_Right) {
                _time_scrollbar->changeScrollBarValue(_time_scrollbar->getFrameJumpValue(), true);
                return true;// Event handled, don't pass it on
            } else if (keyEvent->key() == Qt::Key_Left) {
                _time_scrollbar->changeScrollBarValue(-_time_scrollbar->getFrameJumpValue(), true);
                return true;// Event handled, don't pass it on
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
                return true;// Event handled
            } else if (keyEvent->key() == Qt::Key_Left) {
                _time_scrollbar->changeScrollBarValue(-_time_scrollbar->getFrameJumpValue(), true);
                return true;// Event handled
            }
        }

        // === FIX HERE ===
        // DO NOT call QMainWindow::eventFilter(obj, event) for objects that are not 'this'.
        // Since this is a global filter (qApp), 'obj' could be anything (ADS widgets, buttons, etc).
        // Forwarding random objects to QMainWindow's logic is dangerous.

        if (obj == this) {
            // Only run QMainWindow logic if the event is actually for the Main Window
            return QMainWindow::eventFilter(obj, event);
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

//=================================
// Old Interface Widgets (No EditorRegistry)
//=================================

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

//=================================
// New Editor Instances
//=================================

void MainWindow::openTableDesignerWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("TableDesignerWidget"));
}

void MainWindow::openBatchProcessingWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("BatchProcessingWidget"));
}

void MainWindow::openDataViewer() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("DataViewerWidget"));
}

void MainWindow::openMLWidget() {
    // Use EditorCreationController for ML_Widget
    // This handles single-instance checking, state registration, and zone placement
    openEditor(QStringLiteral("MLWidget"));
}

void MainWindow::openTerminalWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("TerminalWidget"));
}

void MainWindow::openWhiskerTracking() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("WhiskerWidget"));
}

void MainWindow::openDataManager() {
    // Use the standard editor opening mechanism
    // DataManagerWidget is registered as a single-instance widget,
    // so openEditor will find and show the existing instance
    openEditor(QStringLiteral("DataManagerWidget"));
}

void MainWindow::openGroupManagement() {
    // Use the standard editor opening mechanism
    // GroupManagementWidget is registered as a single-instance widget,
    // so openEditor will find and show the existing instance
    openEditor(QStringLiteral("GroupManagementWidget"));
}

void MainWindow::openTestWidget() {
    // Delegate to generic openEditor using EditorRegistry
    openEditor(QStringLiteral("TestWidget"));
}

void MainWindow::openZoneLayoutManager() {
    // Delegate to generic openEditor using EditorRegistry
    openEditor(QStringLiteral("ZoneManagerWidget"));
}

void MainWindow::openDataTransforms() {
    openEditor(QStringLiteral("DataTransformWidget"));
}

void MainWindow::openDataImport() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("DataImportWidget"));
}

void MainWindow::openNewMediaWidget() {
    // Create a new media widget using EditorCreationController
    // The controller handles:
    // - Creating the editor via EditorRegistry
    // - Wrapping in dock widgets
    // - Placing in appropriate zones (view -> Center, properties -> Right)
    // - Connecting cleanup signals for state unregistration
    auto placed = _editor_creation_controller->createAndPlace(
            EditorLib::EditorTypeId(QStringLiteral("MediaWidget")),
            true);// raise_view

    if (!placed.isValid()) {
        std::cerr << "Failed to create new media widget" << std::endl;
        return;
    }

    std::cout << "Created new media widget: "
              << placed.state->getInstanceId().toStdString() << std::endl;
}

void MainWindow::openVideoExportWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("ExportVideoWidget"));
}

void MainWindow::openTongueTracking() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("TongueWidget"));
}

//=================================
// Editor Registration and Opening
//=================================

void MainWindow::_registerEditorTypes() {
    // === Module-based registration ===
    // Each module defines its own factory functions - MainWindow doesn't need
    // to know implementation details like MediaWidgetState, MediaViewWidget, etc.

    MediaWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    DataInspectorModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    DataTransformWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    DataImportWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TestWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    ExportVideoWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar);

    TongueWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    WhiskerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar);

    TimeScrollBarModule::registerTypes(_editor_registry.get(), _data_manager);

    DataManagerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar, _group_manager.get());

    GroupManagementWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    ZoneManagerWidgetRegistration::registerType(_editor_registry.get(), _zone_manager.get());

    TerminalWidgetModule::registerTypes(_editor_registry.get());

    BatchProcessingWidgetModule::registerTypes(_editor_registry.get());

    MLWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    DataViewerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar);

    TableDesignerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    // Future: Add more module registrations here
    // AnalysisDashboardModule::registerTypes(_editor_registry.get(), _data_manager);
}

void MainWindow::openEditor(QString const & type_id) {
    auto info = _editor_registry->typeInfo(EditorLib::EditorTypeId(type_id));

    if (info.type_id.isEmpty()) {
        std::cerr << "MainWindow::openEditor: Unknown editor type: "
                  << type_id.toStdString() << std::endl;
        return;
    }

    // For single-instance editors, check if already open
    if (!info.allow_multiple) {
        auto existing = _editor_registry->statesByType(EditorLib::EditorTypeId(type_id));
        if (!existing.empty()) {
            // Find the existing dock widget and show it
            // The dock widget title should contain the display name
            for (auto const & state: existing) {
                EditorLib::EditorInstanceId instance_id(state->getInstanceId());

                // Search all dock widgets for one containing this state's widget
                for (auto * dock: _m_DockManager->dockWidgetsMap()) {
                    // Check if this dock's widget matches
                    if (dock && dock->widget()) {
                        // Show and raise the existing dock
                        dock->show();
                        dock->raise();
                        dock->setAsCurrentTab();

                        // Set as active editor for PropertiesHost
                        _editor_registry->selectionContext()->setActiveEditor(instance_id);
                        return;
                    }
                }
            }
            // State exists but dock widget not found - clean up orphan state
            std::cerr << "MainWindow::openEditor: State exists but dock widget missing, recreating: "
                      << type_id.toStdString() << std::endl;
            _editor_registry->unregisterState(EditorLib::EditorInstanceId(existing[0]->getInstanceId()));
        }
    }

    // Create new instance via EditorCreationController
    // This handles:
    // - Creating editor via EditorRegistry::createEditor()
    // - Creating dock widgets for view and properties
    // - Placing in appropriate zones from EditorTypeInfo
    // - Connecting cleanup signals
    auto placed = _editor_creation_controller->createAndPlace(
            EditorLib::EditorTypeId(type_id),
            true);// raise_view

    if (!placed.isValid()) {
        std::cerr << "MainWindow::openEditor: Failed to create editor: "
                  << type_id.toStdString() << std::endl;
        return;
    }

    EditorLib::EditorInstanceId instance_id(placed.state->getInstanceId());

    // Set this editor as active so PropertiesHost shows its properties
    _editor_registry->selectionContext()->setActiveEditor(instance_id);

    std::cout << "Created " << info.display_name.toStdString()
              << " via EditorCreationController (instance: "
              << instance_id.toStdString() << ", zone: "
              << zoneToString(info.preferred_zone).toStdString() << ")" << std::endl;
}
