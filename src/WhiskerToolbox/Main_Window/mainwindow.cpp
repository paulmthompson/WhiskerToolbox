#include "mainwindow.hpp"

#include "ui_mainwindow.h"
#include "video_loader.hpp"

#include "DataManager.hpp"
#include "Media/Image_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"

#include "DataManager_Widget/DataManager_Widget.hpp"
#include "DataTransform_Widget/DataTransform_Widget.hpp"
#include "DockAreaWidget.h"
#include "DockSplitter.h"
#include "DockWidget.h"
#include "EditorCreationController.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/OperationContext.hpp"
#include "EditorState/SelectionContext.hpp"
#include "GroupManagementWidget/GroupManagementWidget.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/DisplayOptionsRegistry.hpp"
#include "Media_Widget/UI/Media_Widget.hpp"
#include "SplitButtonHandler.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "ZoneManager.hpp"
#include "ZoneManager/ZoneConfig.hpp"


// Module registration headers - each module defines its own factory functions
#include "BatchProcessing_Widget/BatchProcessingWidgetRegistration.hpp"
#include "CommandLog_Widget/CommandLogWidgetRegistration.hpp"
#include "KeybindingEditor/KeybindingEditorRegistration.hpp"
#include "DataImport_Widget/DataImportWidgetRegistration.hpp"
#include "DataInspector_Widget/DataInspectorWidgetRegistration.hpp"
#include "DataManager_Widget/DataManagerWidgetRegistration.hpp"
#include "DataSynthesizer_Widget/DataSynthesizerWidgetRegistration.hpp"
#include "DataTransform_Widget/DataTransformWidgetRegistration.hpp"
#include "DataViewer_Widget/DataViewerWidgetRegistration.hpp"
#include "DeepLearning_Widget/DeepLearningWidgetRegistration.hpp"
#include "Export_Widgets/Export_Video_Widget/ExportVideoWidgetRegistration.hpp"
#include "GroupManagementWidget/GroupManagementWidgetRegistration.hpp"
#include "MLCore_Widget/MLCoreWidgetRegistration.hpp"
#include "ML_Widget/MLWidgetRegistration.hpp"
#include "Media_Widget/MediaWidgetRegistration.hpp"

#include "Plots/3DPlot/3DPlotWidgetRegistration.hpp"
#include "Plots/ACFWidget/ACFWidgetRegistration.hpp"
#include "Plots/EventPlotWidget/EventPlotWidgetRegistration.hpp"
#include "Plots/HeatmapWidget/HeatmapWidgetRegistration.hpp"
#include "Plots/LinePlotWidget/LinePlotWidgetRegistration.hpp"
#include "Plots/OnionSkinViewWidget/OnionSkinViewWidgetRegistration.hpp"
#include "Plots/PSTHWidget/PSTHWidgetRegistration.hpp"
#include "Plots/ScatterPlotWidget/ScatterPlotWidgetRegistration.hpp"
#include "Plots/SpectrogramWidget/SpectrogramWidgetRegistration.hpp"
#include "Plots/TemporalProjectionViewWidget/TemporalProjectionViewWidgetRegistration.hpp"

#include "Python_Widget/PythonWidgetRegistration.hpp"
#include "TableDesignerWidget/TableDesignerWidget.hpp"
#include "TableDesignerWidget/TableDesignerWidgetRegistration.hpp"
#include "Terminal_Widget/TerminalWidgetRegistration.hpp"
#include "Test_Widget/TestWidgetRegistration.hpp"
#include "TimeScrollBar/TimeScrollBarRegistration.hpp"
#include "Tongue_Widget/TongueWidgetRegistration.hpp"
#include "TransformsV2_Widget/TransformsV2WidgetRegistration.hpp"
#include "TriageSession_Widget/TriageSessionWidgetRegistration.hpp"
#include "Whisker_Widget/WhiskerWidgetRegistration.hpp"
#include "ZoneManagerWidgetRegistration.hpp"

#include "TimeScrollBar/TimeScrollBar.hpp"
#include "TimeScrollBar/TimeScrollBarState.hpp"

#include "StateManagement/AppFileDialog.hpp"
#include "StateManagement/AppPreferences.hpp"
#include "StateManagement/SessionStore.hpp"
#include "StateManagement/StateManager.hpp"
#include "StateManagement/WorkspaceData.hpp"
#include "StateManagement/WorkspaceManager.hpp"

#include "Commands/Core/CommandRecorder.hpp"
#include "Commands/Core/register_core_commands.hpp"
#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "utils/DataLoadUtils.hpp"

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
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
      _editor_registry{std::make_unique<EditorRegistry>(this)},
      _zone_manager(nullptr),
      _state_manager{std::make_unique<StateManagement::StateManager>(this)},
      _group_manager(nullptr),
      _command_recorder{std::make_unique<commands::CommandRecorder>()}

{
    ui->setupUi(this);

    // Load application preferences and session memory (before UI setup)
    _state_manager->loadAll();

    // Initialize the global file dialog wrapper with session path memory
    AppFileDialog::init(_state_manager->session());

    // Register Qt metatypes for TimeFrame types (required for signal/slot)
    qRegisterMetaType<TimeKey>("TimeKey");
    qRegisterMetaType<TimeFrameIndex>("TimeFrameIndex");
    qRegisterMetaType<TimePosition>("TimePosition");

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

    // Create SplitButtonHandler to add split buttons to dock area title bars
    // This enables VS Code-like editor splitting functionality
    _split_button_handler = std::make_unique<SplitButtonHandler>(_m_DockManager, this);

    // Connect split button signals (split implementation will be added later)
    connect(_split_button_handler.get(), &SplitButtonHandler::splitDockWidgetRequested,
            this, [this](ads::CDockWidget * dock_widget, SplitButtonHandler::SplitDirection direction) {
                // TODO: Implement split functionality
                // For now, just log the request
                std::cout << "Split requested for dock widget: "
                          << dock_widget->objectName().toStdString()
                          << " direction: "
                          << (direction == SplitButtonHandler::SplitDirection::Horizontal ? "horizontal" : "vertical")
                          << std::endl;
            });

    // Create EditorCreationController to handle unified editor creation and zone placement
    _editor_creation_controller = std::make_unique<EditorCreationController>(
            _editor_registry.get(), _zone_manager.get(), _m_DockManager, this);

    // Wire up WorkspaceManager with its collaborators (created after StateManager)
    _state_manager->workspace()->setEditorRegistry(_editor_registry.get());
    _state_manager->workspace()->setZoneManager(_zone_manager.get());
    _state_manager->workspace()->setDockStateCaptureCallback([this]() -> std::string {
        QByteArray const ads_state = _m_DockManager->saveState();
        return ads_state.toBase64().toStdString();
    });

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
    _time_scrollbar->setEditorRegistry(_editor_registry.get());

    // Register all commands with the CommandRegistry (before any command usage)
    commands::register_core_commands();

    // Wire the openEditor callback so ContextActions can open/focus editors
    _editor_registry->setOpenEditorCallback([this](EditorTypeId const & type_id)
                                                    -> std::shared_ptr<EditorState> {
        openEditor(type_id.value);
        auto states = _editor_registry->statesByType(type_id);
        return states.empty() ? nullptr : states.back();
    });

    // Create KeymapManager for configurable keyboard shortcuts
    // Created BEFORE _registerEditorTypes() so widget modules can register their actions
    _keymap_manager = new KeymapSystem::KeymapManager(this);
    _keymap_manager->setEditorRegistry(_editor_registry.get());

    // Register editor types with the factory
    // Must be called AFTER creating dependencies (TimeScrollBar, GroupManager, KeymapManager)
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

    // Install KeymapManager as event filter AFTER 'this' so it runs FIRST (Qt LIFO order)
    qApp->installEventFilter(_keymap_manager);

    // Load persisted keybinding overrides into KeymapManager
    _keymap_manager->importOverrides(_state_manager->preferences()->keybindingOverrides());

    // Wire the TimeScrollBar adapter so KeymapManager can dispatch timeline actions
    _time_scrollbar->setKeymapManager(_keymap_manager);

    // Auto-save keybinding overrides whenever bindings change
    connect(_keymap_manager, &KeymapSystem::KeymapManager::bindingsChanged,
            this, [this]() {
                _state_manager->preferences()->setKeybindingOverrides(
                        _keymap_manager->exportOverrides());
            });

    _buildInitialLayout();

    // Wire up provenance tracking for pipeline execution and table creation
    _connectProvenanceTracking();

    // Restore window geometry from previous session
    _state_manager->session()->restoreWindowGeometry(this);

    // Connect workspace dirty flag to title bar updates
    connect(_state_manager->workspace(), &StateManagement::WorkspaceManager::dirtyChanged,
            this, &MainWindow::_updateTitleBar);
    connect(_state_manager->workspace(), &StateManagement::WorkspaceManager::workspacePathChanged,
            this, &MainWindow::_updateTitleBar);

    // Enable crash-recovery auto-save
    _state_manager->workspace()->enableAutoSave();

    _updateTitleBar();

    // Check for crash recovery file from a previous unclean shutdown
    _checkCrashRecovery();
}

void MainWindow::closeEvent(QCloseEvent * event) {
    // Capture window geometry before closing
    _state_manager->session()->captureWindowGeometry(this);

    // Persist all state to disk
    _state_manager->saveAll();

    // Clean shutdown — remove the crash-recovery file
    _state_manager->workspace()->disableAutoSave();
    _state_manager->workspace()->deleteRecoveryFile();

    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow() {
    // IMPORTANT: Destruction order matters here!
    // Widgets (owned by Qt's widget tree via CDockManager) may reference _editor_registry.
    // We must delete the dock manager (and thus all docked widgets) BEFORE
    // _editor_registry is destroyed by unique_ptr cleanup.

    // Delete the dock manager first - this destroys all docked widgets
    // which may try to unregister from EditorRegistry in their destructors
    delete _m_DockManager;
    _m_DockManager = nullptr;

    // Now it's safe for unique_ptr members to be destroyed in reverse order
    // (_editor_registry, _zone_manager, etc.)

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
        // Use a fixed object name so ADS restoreState() can find this widget across sessions
        placed_group.view_dock->setObjectName(QStringLiteral("builtin_group_manager"));
        // Mark as non-closable and non-dockable since it's a core navigation widget
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
        placed_group.view_dock->setFeature(ads::CDockWidget::DockWidgetPinnable, false);

        if (auto * area = placed_group.view_dock->dockAreaWidget()) {
            area->setDockAreaFlag(ads::CDockAreaWidget::HideSingleWidgetTitleBar, true);
        }
    }

    // Create DataManager_Widget - we need manual placement with split
    // Create via EditorRegistry but don't use controller's automatic placement
    auto dm_instance = _editor_registry->createEditor(EditorLib::EditorTypeId(QStringLiteral("DataManagerWidget")));
    if (dm_instance.state && dm_instance.view) {
        auto * dm_dock = new ads::CDockWidget(QStringLiteral("Data Manager"));
        dm_dock->setWidget(dm_instance.view);
        // Use a fixed object name so ADS restoreState() can find this widget across sessions
        dm_dock->setObjectName(QStringLiteral("builtin_data_manager"));
        dm_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
        dm_dock->setFeature(ads::CDockWidget::DockWidgetPinnable, false);

        // Add below GroupManagement with 30/70 split (0.3 = top widget gets 30%)
        _zone_manager->addBelowInZone(dm_dock, Zone::Left, 0.30f);

        if (auto * area = dm_dock->dockAreaWidget()) {
            area->setDockAreaFlag(ads::CDockAreaWidget::HideSingleWidgetTitleBar, true);
        }
    }

    // === CENTER ZONE: Primary visualization ===

    // Create and add media widget to center zone using EditorCreationController
    // This respects EditorTypeInfo zone preferences and handles dock widget creation
    auto placed_media = _editor_creation_controller->createAndPlaceWithTitle(
            EditorLib::EditorTypeId(QStringLiteral("MediaWidget")),
            QStringLiteral("Media Viewer"),
            true);// raise_view

    if (placed_media.view_dock) {
        placed_media.view_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    }

    // === BOTTOM ZONE: Timeline ===

    // Add time scrollbar to bottom zone
    // Note: TimeScrollBar is created in the constructor (with state) because other modules
    // need it during registration. Here we just wrap it in a dock widget.
    auto * scrollbar_dock = new ads::CDockWidget(QStringLiteral("Timeline"));
    scrollbar_dock->setWidget(_time_scrollbar);
    // Set stable object name for ADS state persistence
    scrollbar_dock->setObjectName(QStringLiteral("builtin_timeline"));
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, false);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetClosable, false);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetMovable, false);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, false);
    scrollbar_dock->setFeature(ads::CDockWidget::DockWidgetPinnable, false);
    _zone_manager->addToZone(scrollbar_dock, Zone::Bottom);

    // Hide the title bar (with its close/undock/split buttons) since the
    // Timeline is the only widget in the Bottom zone and should look clean.
    if (auto * area = scrollbar_dock->dockAreaWidget()) {
        area->setDockAreaFlag(ads::CDockAreaWidget::HideSingleWidgetTitleBar, true);
    }

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

    // Workspace save/load (re-purpose the old layout actions)
    ui->actionSave_Layout->setText(QStringLiteral("Save Workspace"));
    ui->actionSave_Layout->setEnabled(true);
    connect(ui->actionSave_Layout, &QAction::triggered, this, &MainWindow::_saveWorkspace);

    ui->actionLoad_Layout->setText(QStringLiteral("Open Workspace"));
    ui->actionLoad_Layout->setEnabled(true);
    connect(ui->actionLoad_Layout, &QAction::triggered, this, &MainWindow::_openWorkspace);

    // Recent Workspaces submenu (inserted after Open Workspace in the File menu)
    {
        auto * file_menu = ui->menuFile;
        _recent_workspaces_menu = new QMenu(QStringLiteral("Recent Workspaces"), file_menu);
        // Insert after the Open Workspace action
        auto * after_action = ui->actionLoad_Layout;
        file_menu->insertMenu(file_menu->actions().indexOf(after_action) < file_menu->actions().size() - 1
                                      ? file_menu->actions()[file_menu->actions().indexOf(after_action) + 1]
                                      : nullptr,
                              _recent_workspaces_menu);

        // Rebuild from session store
        _rebuildRecentWorkspacesMenu();

        // Rebuild when session changes (e.g., after save/open)
        connect(_state_manager->session(), &StateManagement::SessionStore::sessionChanged,
                this, &MainWindow::_rebuildRecentWorkspacesMenu);
    }

    // Connect TimeScrollBar to EditorRegistry for global time propagation
    connect(_time_scrollbar,
            qOverload<TimePosition>(&TimeScrollBar::timeChanged),
            _editor_registry.get(),
            qOverload<TimePosition const &>(&EditorRegistry::setCurrentTime));

    connect(ui->actionWhisker_Tracking, &QAction::triggered, this, &MainWindow::openWhiskerTracking);
    connect(ui->actionMachine_Learning, &QAction::triggered, this, &MainWindow::openMLWidget);
    connect(ui->actionDeep_Learning_Widget, &QAction::triggered, this, &MainWindow::openDeepLearningWidget);
    connect(ui->actionData_Viewer, &QAction::triggered, this, &MainWindow::openDataViewer);
    connect(ui->actionNew_Media_Widget, &QAction::triggered, this, &MainWindow::openNewMediaWidget);
    connect(ui->actionBatch_Processing, &QAction::triggered, this, &MainWindow::openBatchProcessingWidget);
    connect(ui->actionData_Inspector, &QAction::triggered, this, &MainWindow::openDataInspector);
    connect(ui->actionExport_Video, &QAction::triggered, this, &MainWindow::openVideoExportWidget);
    connect(ui->actionData_Transforms, &QAction::triggered, this, &MainWindow::openDataTransforms);
    connect(ui->actionTransforms_V2, &QAction::triggered, this, &MainWindow::openTransformsV2Widget);
    connect(ui->actionTriage_Session, &QAction::triggered, this, &MainWindow::openTriageSessionWidget);
    connect(ui->actionTerminal_Output, &QAction::triggered, this, &MainWindow::openTerminalWidget);
    connect(ui->actionTable_Designer, &QAction::triggered, this, &MainWindow::openTableDesignerWidget);
    connect(ui->actionTest_Widget, &QAction::triggered, this, &MainWindow::openTestWidget);
    connect(ui->actionPython_Console, &QAction::triggered, this, &MainWindow::openPythonWidget);
    connect(ui->actionZone_Layout_Manager, &QAction::triggered, this, &MainWindow::openZoneLayoutManager);
    connect(ui->actionData_Import, &QAction::triggered, this, &MainWindow::openDataImport);
    connect(ui->actionEvent_Plot, &QAction::triggered, this, &MainWindow::openEventPlotWidget);
    connect(ui->actionACF_Plot, &QAction::triggered, this, &MainWindow::openACFPlotWidget);
    connect(ui->actionPSTH_Plot, &QAction::triggered, this, &MainWindow::openPSTHPlotWidget);
    connect(ui->actionLine_Plot, &QAction::triggered, this, &MainWindow::openLinePlotWidget);
    connect(ui->actionHeatmap_Plot, &QAction::triggered, this, &MainWindow::openHeatmapPlotWidget);
    connect(ui->actionTemporal_Projection_View, &QAction::triggered, this, &MainWindow::openTemporalProjectionViewWidget);
    connect(ui->actionScatter_Plot, &QAction::triggered, this, &MainWindow::openScatterPlotWidget);
    connect(ui->action3D_Plot, &QAction::triggered, this, &MainWindow::open3DPlotWidget);
    connect(ui->actionOnion_Skin_View, &QAction::triggered, this, &MainWindow::openOnionSkinViewWidget);
    connect(ui->actionData_Synthesizer, &QAction::triggered, this, &MainWindow::openDataSynthesizerWidget);
    connect(ui->actionCommand_Log, &QAction::triggered, this, &MainWindow::openCommandLogWidget);
    connect(ui->actionKeybinding_Editor, &QAction::triggered, this, &MainWindow::openKeybindingEditor);
}

/*
The Load_Video callback is executed whenever a the load_video option is selected.
If a video is selected, that video will be loaded and the first frame will be
drawn on the video screen.

*/
void MainWindow::Load_Video() {
    auto const initial_dir = _state_manager->session()->lastUsedPath(
            QStringLiteral("load_video"), QDir::currentPath());
    auto vid_name = QFileDialog::getOpenFileName(
            this,
            "Load Video File",
            initial_dir,
            "All files (*.*) ;; MP4 (*.mp4); HDF5 (*.h5); MAT (*.mat)");

    if (vid_name.isNull()) {
        return;
    }

    _state_manager->session()->rememberPath(QStringLiteral("load_video"), vid_name);

    // Use the conditional video loader
    if (loadVideoData(vid_name.toStdString(), _data_manager.get())) {
        _state_manager->workspace()->recordVideoLoad(vid_name);
        loadData();
    }
}

void MainWindow::Load_Images() {
    auto const initial_dir = _state_manager->session()->lastUsedPath(
            QStringLiteral("load_images"), QDir::currentPath());
    auto dir_name = QFileDialog::getExistingDirectory(
            this,
            "Load Video File",
            initial_dir);

    if (dir_name.isNull()) {
        return;
    }

    _state_manager->session()->rememberPath(QStringLiteral("load_images"), dir_name);

    _state_manager->workspace()->recordImagesLoad(dir_name);

    auto media = std::make_shared<ImageData>();
    media->LoadMedia(dir_name.toStdString());
    _data_manager->setData<ImageData>("media", media, TimeKey("time"));

    loadData();
}

void MainWindow::_loadJSONConfig() {
    auto const initial_dir = _state_manager->session()->lastUsedPath(
            QStringLiteral("load_json_config"), QDir::currentPath());
    auto filename = QFileDialog::getOpenFileName(
            this,
            "Load JSON File",
            initial_dir,
            "All files (*.*) ;; JSON (*.json)");

    if (filename.isNull()) {
        return;
    }

    _state_manager->session()->rememberPath(QStringLiteral("load_json_config"), filename);

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
            int const percent = (current * 100) / total;
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

    // Record in workspace provenance
    _state_manager->workspace()->recordJsonConfigLoad(filename);

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
        // Timeline key handling (Space, arrows, Ctrl+arrows) has been migrated
        // to the KeymapSystem. KeymapManager's event filter intercepts those keys
        // before this filter runs (Qt LIFO event filter order).

        // === FIX HERE ===
        // DO NOT call QMainWindow::eventFilter(obj, event) for objects that are not 'this'.
        // Since this is a global filter (qApp), 'obj' could be anything (ADS widgets, buttons, etc).
        // Forwarding random objects to QMainWindow's logic is dangerous.

        if (obj == this) {
            // Only run QMainWindow logic if the event is actually for the Main Window
            return QMainWindow::eventFilter(obj, event);
        }

        // For all other keys, let them pass through to the focused widget
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
    // Use EditorCreationController for MLCoreWidget
    // This handles single-instance checking, state registration, and zone placement
    openEditor(QStringLiteral("MLCoreWidget"));
}

void MainWindow::openDeepLearningWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("DeepLearningWidget"));
}

void MainWindow::openTerminalWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("TerminalWidget"));
}

void MainWindow::openWhiskerTracking() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("WhiskerWidget"));
}

void MainWindow::openTestWidget() {
    // Delegate to generic openEditor using EditorRegistry
    openEditor(QStringLiteral("TestWidget"));
}

void MainWindow::openPythonWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("PythonWidget"));
}

void MainWindow::openZoneLayoutManager() {
    // Delegate to generic openEditor using EditorRegistry
    openEditor(QStringLiteral("ZoneManagerWidget"));
}

void MainWindow::openDataTransforms() {
    openEditor(QStringLiteral("DataTransformWidget"));
}

void MainWindow::openTransformsV2Widget() {
    openEditor(QStringLiteral("TransformsV2Widget"));
}

void MainWindow::openTriageSessionWidget() {
    openEditor(QStringLiteral("TriageSessionWidget"));
}

void MainWindow::openDataSynthesizerWidget() {
    openEditor(QStringLiteral("DataSynthesizerWidget"));
}

void MainWindow::openCommandLogWidget() {
    openEditor(QStringLiteral("CommandLogWidget"));
}

void MainWindow::openKeybindingEditor() {
    openEditor(QStringLiteral("KeybindingEditor"));
}

void MainWindow::openDataImport() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("DataImportWidget"));
}

void MainWindow::openEventPlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("EventPlotWidget"));
}

void MainWindow::openACFPlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("ACFWidget"));
}

void MainWindow::openPSTHPlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("PSTHWidget"));
}

void MainWindow::openLinePlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("LinePlotWidget"));
}

void MainWindow::openHeatmapPlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("HeatmapWidget"));
}

void MainWindow::openTemporalProjectionViewWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("TemporalProjectionViewWidget"));
}

void MainWindow::openOnionSkinViewWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("OnionSkinViewWidget"));
}

void MainWindow::openScatterPlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("ScatterPlotWidget"));
}

void MainWindow::open3DPlotWidget() {
    // Use EditorCreationController pattern - delegate to openEditor
    openEditor(QStringLiteral("3DPlotWidget"));
}

void MainWindow::openDataInspector() {
    // Create a new Data Inspector using EditorCreationController
    // The controller handles:
    // - Creating the editor via EditorRegistry
    // - Wrapping in dock widgets
    // - Placing in appropriate zones (view -> Center, properties -> Right)
    // - Connecting cleanup signals for state unregistration
    // DataInspector supports multiple instances (allow_multiple = true)
    auto placed = _editor_creation_controller->createAndPlace(
            EditorLib::EditorTypeId(QStringLiteral("DataInspector")),
            true);// raise_view

    if (!placed.isValid()) {
        std::cerr << "Failed to create new data inspector" << std::endl;
        return;
    }

    std::cout << "Created new data inspector: "
              << placed.state->getInstanceId().toStdString() << std::endl;
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

//=================================
// Editor Registration and Opening
//=================================

void MainWindow::_registerEditorTypes() {
    // === Module-based registration ===
    // Each module defines its own factory functions - MainWindow doesn't need
    // to know implementation details like MediaWidgetState, MediaViewWidget, etc.

    MediaWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get(), _keymap_manager);

    DataInspectorModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get(), commandRecorder(), _keymap_manager);

    DataTransformWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TransformsV2WidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TriageSessionWidgetModule::registerTypes(_editor_registry.get(), _data_manager, commandRecorder(), _keymap_manager);

    DataImportWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TestWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    ExportVideoWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TongueWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    WhiskerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TimeScrollBarModule::registerTypes(_editor_registry.get(), _data_manager, _keymap_manager);

    DataManagerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    GroupManagementWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    ZoneManagerWidgetRegistration::registerType(_editor_registry.get(), _zone_manager.get());

    PythonWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _state_manager->preferences());

    TerminalWidgetModule::registerTypes(_editor_registry.get());

    BatchProcessingWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    MLWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    MLCoreWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    DeepLearningWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    DataViewerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TableDesignerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    EventPlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    ACFWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    PSTHWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    LinePlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    HeatmapWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    TemporalProjectionViewWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());

    ScatterPlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get(), _keymap_manager);

    SpectrogramWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    ThreeDPlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    OnionSkinViewWidgetModule::registerTypes(_editor_registry.get(), _data_manager);

    DataSynthesizerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, commandRecorder());

    CommandLogWidgetModule::registerTypes(_editor_registry.get(), commandRecorder());

    KeybindingEditorModule::registerTypes(_editor_registry.get(), _keymap_manager);

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
                EditorLib::EditorInstanceId const instance_id(state->getInstanceId());

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

    EditorLib::EditorInstanceId const instance_id(placed.state->getInstanceId());

    // Set this editor as active so PropertiesHost shows its properties
    _editor_registry->selectionContext()->setActiveEditor(instance_id);

    std::cout << "Created " << info.display_name.toStdString()
              << " via EditorCreationController (instance: "
              << instance_id.toStdString() << ", zone: "
              << zoneToString(info.preferred_zone).toStdString() << ")" << std::endl;
}

// ---------------------------------------------------------------------------
// Workspace save / load
// ---------------------------------------------------------------------------

void MainWindow::_saveWorkspace() {
    auto const initial_dir = _state_manager->session()->lastUsedPath(
            QStringLiteral("workspace"), QDir::currentPath());

    auto const path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save Workspace"),
            initial_dir,
            QStringLiteral("WhiskerToolbox Workspace (*.wtb)"));

    if (path.isEmpty()) {
        return;
    }

    _state_manager->session()->rememberPath(QStringLiteral("workspace"), path);

    if (_state_manager->workspace()->saveWorkspace(path)) {
        _state_manager->session()->addRecentWorkspace(path);
        std::cout << "Workspace saved to: " << path.toStdString() << std::endl;
    } else {
        std::cerr << "Failed to save workspace to: " << path.toStdString() << std::endl;
    }
}

void MainWindow::_openWorkspace() {
    auto const initial_dir = _state_manager->session()->lastUsedPath(
            QStringLiteral("workspace"), QDir::currentPath());

    auto const path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Open Workspace"),
            initial_dir,
            QStringLiteral("WhiskerToolbox Workspace (*.wtb)"));

    if (path.isEmpty()) {
        return;
    }

    _state_manager->session()->rememberPath(QStringLiteral("workspace"), path);

    auto data = _state_manager->workspace()->readWorkspace(path);
    if (!data) {
        std::cerr << "Failed to read workspace from: " << path.toStdString() << std::endl;
        return;
    }

    // --- Replay data loads ---
    for (auto const & entry: data->data_loads) {
        if (entry.loader_type == "json_config") {
            auto const json_path = QString::fromStdString(entry.source_path);
            if (QFile::exists(json_path)) {
                auto data_info = loadDataAndBroadcastConfig(
                        _data_manager.get(),
                        _editor_registry.get(),
                        entry.source_path);
                processLoadedData(data_info);
            } else {
                std::cerr << "Workspace: JSON config not found: " << entry.source_path << std::endl;
            }
        } else if (entry.loader_type == "video") {
            auto const vid_path = QString::fromStdString(entry.source_path);
            if (QFile::exists(vid_path)) {
                if (loadVideoData(entry.source_path, _data_manager.get())) {
                    loadData();
                }
            } else {
                std::cerr << "Workspace: Video not found: " << entry.source_path << std::endl;
            }
        } else if (entry.loader_type == "images") {
            auto const dir_path = QString::fromStdString(entry.source_path);
            if (QDir(dir_path).exists()) {
                auto media = std::make_shared<ImageData>();
                media->LoadMedia(entry.source_path);
                _data_manager->setData<ImageData>("media", media, TimeKey("time"));
                loadData();
            } else {
                std::cerr << "Workspace: Image directory not found: " << entry.source_path << std::endl;
            }
        }
    }

    // --- Restore editor states (widget recreation) ---
    // Pause auto-save during the full restore sequence to prevent
    // captureCurrentConfig() from accessing freed dock areas.
    _state_manager->workspace()->disableAutoSave();

    _restoreEditorStates(*data);

    // --- Restore zone layout ---
    _restoreZoneLayout(*data);

    // Re-enable auto-save now that the layout is fully stable
    _state_manager->workspace()->enableAutoSave();

    // Restore data load provenance from the workspace
    _state_manager->workspace()->clearDataLoads();
    for (auto const & entry: data->data_loads) {
        _state_manager->workspace()->recordDataLoad(entry);
    }

    // Restore pipeline and table provenance
    _state_manager->workspace()->clearAppliedPipelines();
    for (auto const & pj: data->applied_pipelines) {
        _state_manager->workspace()->recordAppliedPipeline(pj);
    }
    _state_manager->workspace()->clearTableDefinitions();
    for (auto const & tj: data->table_definitions) {
        _state_manager->workspace()->recordTableDefinition(tj);
    }

    // Mark the workspace as successfully restored
    _state_manager->workspace()->markRestored(path);

    // Add to recent workspaces
    _state_manager->session()->addRecentWorkspace(path);

    std::cout << "Workspace loaded from: " << path.toStdString() << std::endl;
}

void MainWindow::_updateTitleBar() {
    QString title = QStringLiteral("WhiskerToolbox");

    auto const ws_path = _state_manager->workspace()->currentWorkspacePath();
    if (!ws_path.isEmpty()) {
        auto const file_name = QFileInfo(ws_path).fileName();
        title = file_name + QStringLiteral(" — ") + title;
    }

    if (_state_manager->workspace()->hasUnsavedChanges()) {
        title.prepend(QStringLiteral("● "));
    }

    setWindowTitle(title);
}

// ---------------------------------------------------------------------------
// Crash Recovery
// ---------------------------------------------------------------------------

void MainWindow::_checkCrashRecovery() {
    auto * ws = _state_manager->workspace();
    if (!ws->hasRecoveryFile()) {
        return;
    }

    auto recovery_data = ws->readRecoveryFile();
    if (!recovery_data) {
        // Corrupt recovery file — delete it silently
        ws->deleteRecoveryFile();
        return;
    }

    auto const answer = QMessageBox::question(
            this,
            QStringLiteral("Session Recovery"),
            QStringLiteral("A previous session was not saved properly.\n\n"
                           "Would you like to restore it?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

    if (answer == QMessageBox::Yes) {
        // Replay data loads
        for (auto const & entry: recovery_data->data_loads) {
            if (entry.loader_type == "json_config") {
                auto const json_path = QString::fromStdString(entry.source_path);
                if (QFile::exists(json_path)) {
                    auto data_info = loadDataAndBroadcastConfig(
                            _data_manager.get(),
                            _editor_registry.get(),
                            entry.source_path);
                    processLoadedData(data_info);
                }
            } else if (entry.loader_type == "video") {
                if (QFile::exists(QString::fromStdString(entry.source_path))) {
                    if (loadVideoData(entry.source_path, _data_manager.get())) {
                        loadData();
                    }
                }
            } else if (entry.loader_type == "images") {
                if (QDir(QString::fromStdString(entry.source_path)).exists()) {
                    auto media = std::make_shared<ImageData>();
                    media->LoadMedia(entry.source_path);
                    _data_manager->setData<ImageData>("media", media, TimeKey("time"));
                    loadData();
                }
            }
        }

        // Restore editor states
        _state_manager->workspace()->disableAutoSave();
        _restoreEditorStates(*recovery_data);

        // Restore zone layout
        _restoreZoneLayout(*recovery_data);
        _state_manager->workspace()->enableAutoSave();

        // Restore provenance
        ws->clearDataLoads();
        for (auto const & entry: recovery_data->data_loads) {
            ws->recordDataLoad(entry);
        }
        ws->clearAppliedPipelines();
        for (auto const & pj: recovery_data->applied_pipelines) {
            ws->recordAppliedPipeline(pj);
        }
        ws->clearTableDefinitions();
        for (auto const & tj: recovery_data->table_definitions) {
            ws->recordTableDefinition(tj);
        }

        // Mark dirty since this came from a recovery (not an explicit save)
        ws->markDirty();
        _updateTitleBar();

        std::cout << "Session restored from recovery file" << std::endl;
    }

    // Delete the recovery file regardless of choice
    ws->deleteRecoveryFile();
}

// ---------------------------------------------------------------------------
// Recent Workspaces Menu
// ---------------------------------------------------------------------------

void MainWindow::_rebuildRecentWorkspacesMenu() {
    if (!_recent_workspaces_menu) {
        return;
    }

    _recent_workspaces_menu->clear();

    auto const recent = _state_manager->session()->recentWorkspaces();
    if (recent.isEmpty()) {
        auto * empty_action = _recent_workspaces_menu->addAction(QStringLiteral("(No recent workspaces)"));
        empty_action->setEnabled(false);
        return;
    }

    for (auto const & ws_path: recent) {
        auto const display = QFileInfo(ws_path).fileName();
        auto * action = _recent_workspaces_menu->addAction(display);
        action->setToolTip(ws_path);
        connect(action, &QAction::triggered, this, [this, ws_path]() {
            if (!QFile::exists(ws_path)) {
                QMessageBox::warning(this,
                                     QStringLiteral("File Not Found"),
                                     QStringLiteral("Workspace file not found:\n%1").arg(ws_path));
                _state_manager->session()->removeRecentWorkspace(ws_path);
                return;
            }

            _state_manager->session()->rememberPath(QStringLiteral("workspace"), ws_path);

            auto data = _state_manager->workspace()->readWorkspace(ws_path);
            if (!data) {
                QMessageBox::warning(this,
                                     QStringLiteral("Load Error"),
                                     QStringLiteral("Failed to read workspace:\n%1").arg(ws_path));
                return;
            }

            // Replay data loads
            for (auto const & entry: data->data_loads) {
                if (entry.loader_type == "json_config") {
                    if (QFile::exists(QString::fromStdString(entry.source_path))) {
                        auto data_info = loadDataAndBroadcastConfig(
                                _data_manager.get(), _editor_registry.get(), entry.source_path);
                        processLoadedData(data_info);
                    }
                } else if (entry.loader_type == "video") {
                    if (QFile::exists(QString::fromStdString(entry.source_path))) {
                        if (loadVideoData(entry.source_path, _data_manager.get())) {
                            loadData();
                        }
                    }
                } else if (entry.loader_type == "images") {
                    if (QDir(QString::fromStdString(entry.source_path)).exists()) {
                        auto media = std::make_shared<ImageData>();
                        media->LoadMedia(entry.source_path);
                        _data_manager->setData<ImageData>("media", media, TimeKey("time"));
                        loadData();
                    }
                }
            }

            _state_manager->workspace()->disableAutoSave();
            _restoreEditorStates(*data);
            _restoreZoneLayout(*data);
            _state_manager->workspace()->enableAutoSave();

            _state_manager->workspace()->clearDataLoads();
            for (auto const & entry: data->data_loads) {
                _state_manager->workspace()->recordDataLoad(entry);
            }
            _state_manager->workspace()->clearAppliedPipelines();
            for (auto const & pj: data->applied_pipelines) {
                _state_manager->workspace()->recordAppliedPipeline(pj);
            }
            _state_manager->workspace()->clearTableDefinitions();
            for (auto const & tj: data->table_definitions) {
                _state_manager->workspace()->recordTableDefinition(tj);
            }

            _state_manager->workspace()->markRestored(ws_path);
            _state_manager->session()->addRecentWorkspace(ws_path);
            std::cout << "Workspace loaded from recent: " << ws_path.toStdString() << std::endl;
        });
    }

    // Separator + clear action
    _recent_workspaces_menu->addSeparator();
    auto * clear_action = _recent_workspaces_menu->addAction(QStringLiteral("Clear Recent"));
    connect(clear_action, &QAction::triggered, this, [this]() {
        auto const recent = _state_manager->session()->recentWorkspaces();
        for (auto const & ws: recent) {
            _state_manager->session()->removeRecentWorkspace(ws);
        }
    });
}

// ---------------------------------------------------------------------------
// Editor State & Zone Layout Restore
// ---------------------------------------------------------------------------

void MainWindow::_closeDynamicEditors() {
    // Collect dock widgets to close. We must collect first, then close,
    // because closing can modify the dock manager's internal map.
    std::vector<ads::CDockWidget *> to_close;

    for (auto * dock: _m_DockManager->dockWidgetsMap()) {
        if (!dock || !dock->widget()) {
            continue;
        }

        // Skip non-closable widgets (built-in: DataManager, GroupManager, Timeline)
        if (!dock->features().testFlag(ads::CDockWidget::DockWidgetClosable)) {
            continue;
        }

        to_close.push_back(dock);
    }

    for (auto * dock: to_close) {
        dock->closeDockWidget();
    }
}

void MainWindow::_restoreEditorStates(StateManagement::WorkspaceData const & data) {
    if (data.editor_states_json.empty()) {
        return;
    }

    // Close all dynamically-created editor widgets so restore is a full replacement
    _closeDynamicEditors();

    // Use EditorRegistry::fromJson() which:
    // 1. Clears existing states
    // 2. Creates new states from the serialized JSON
    // 3. Registers them in the registry
    if (!_editor_registry->fromJson(data.editor_states_json)) {
        std::cerr << "Workspace: Failed to restore editor states from JSON" << std::endl;
        return;
    }

    // Now recreate view/properties widgets for each restored state
    // and place them in their preferred zones
    auto const restored_states = _editor_registry->allStates();
    for (auto const & state: restored_states) {
        auto const type_id = EditorLib::EditorTypeId(state->getTypeName());
        auto const info = _editor_registry->typeInfo(type_id);

        if (info.type_id.isEmpty()) {
            std::cerr << "Workspace: Unknown editor type during restore: "
                      << state->getTypeName().toStdString() << std::endl;
            continue;
        }

        // Skip types that only have create_editor_custom (no create_view factory).
        // These are built-in widgets always created by _buildInitialLayout and cannot
        // be recreated from state alone (they need DataManager, GroupManager, etc.).
        // Their state is still restored by fromJson() above; we just skip view creation.
        if (!info.create_view) {
            continue;
        }

        // Use placeExistingEditor to create views and place in zones
        auto placed = _editor_creation_controller->placeExistingEditor(
                state,
                info.preferred_zone,
                info.properties_zone,
                {},    // use default title
                false);// don't raise

        if (!placed.isValid()) {
            std::cerr << "Workspace: Failed to place restored editor: "
                      << state->getInstanceId().toStdString() << std::endl;
        }
    }

    std::cout << "Restored " << restored_states.size() << " editor states" << std::endl;
}

void MainWindow::_restoreZoneLayout(StateManagement::WorkspaceData const & data) {
    // Prefer ADS dock state for exact widget positions (splits, tabs, sizes).
    // Fall back to the zone-ratio-only config for older workspace files.
    if (!data.dock_state_base64.empty()) {
        QByteArray const ads_state = QByteArray::fromBase64(
                QByteArray::fromStdString(data.dock_state_base64));
        if (_m_DockManager->restoreState(ads_state)) {
            // ADS restoreState() may destroy and recreate CDockAreaWidget
            // objects, leaving ZoneManager with dangling pointers. Refresh
            // them now so that captureCurrentConfig() (called by autosave)
            // operates on valid pointers.
            _zone_manager->refreshZoneAreas();

            // Ensure built-in (non-closable) widgets are visible and docked.
            // ADS restoreState() may hide widgets whose objectName changed
            // between sessions (e.g. the first save used a UUID name but the
            // code now uses a fixed name like "builtin_group_manager").
            // Simply calling toggleView(true) would open them as floating
            // windows, so we re-dock them into their proper zones instead.
            for (auto * dock: _m_DockManager->dockWidgetsMap()) {
                if (!dock || dock->features().testFlag(ads::CDockWidget::DockWidgetClosable)) {
                    continue;
                }
                if (dock->isClosed() || dock->isFloating()) {
                    auto const obj_name = dock->objectName();
                    if (obj_name == QStringLiteral("builtin_group_manager")) {
                        _zone_manager->addToZone(dock, Zone::Left, false);
                    } else if (obj_name == QStringLiteral("builtin_data_manager")) {
                        _zone_manager->addToZone(dock, Zone::Left, false);
                    } else if (obj_name == QStringLiteral("builtin_timeline")) {
                        _zone_manager->addToZone(dock, Zone::Bottom, false);
                    }
                }
            }

            std::cout << "Restored exact dock layout from ADS state" << std::endl;
            return;
        }
        std::cerr << "Workspace: ADS restoreState failed, falling back to zone ratios" << std::endl;
    }

    if (data.zone_layout_json.empty()) {
        return;
    }

    auto config_result = ZoneConfig::loadFromJson(data.zone_layout_json);
    if (!config_result) {
        std::cerr << "Workspace: Failed to parse zone layout JSON" << std::endl;
        return;
    }

    _zone_manager->applyConfig(*config_result);

    // Defer splitter size reapplication to after layout stabilizes
    _zone_manager->reapplySplitterSizes(200);

    std::cout << "Restored zone layout" << std::endl;
}

// ---------------------------------------------------------------------------
// Provenance Tracking (Pipeline + Table)
// ---------------------------------------------------------------------------

void MainWindow::_connectProvenanceTracking() {
    // This is called after the initial layout is built.
    // We connect to signals from DataTransform_Widget and TableDesignerWidget
    // instances as they are created.

    // Wire OperationContext: when any widget requests an operation from a producer
    // type, auto-open/focus the producer widget so the user can configure it.
    // This enables e.g. TensorDesigner's ColumnConfigDialog to request a pipeline
    // from TransformsV2Widget without the user manually opening it first.
    if (auto * op_ctx = _editor_registry->operationContext()) {
        connect(op_ctx, &EditorLib::OperationContext::operationRequested,
                this, [this](EditorLib::PendingOperation const & op) {
                    openEditor(op.producer_type.toString());
                });
    }

    // Connect to EditorRegistry's editorCreated signal to wire up
    // provenance tracking for new widget instances
    connect(_editor_registry.get(), &EditorRegistry::editorCreated,
            this, [this](EditorInstanceId const & instance_id, EditorTypeId const & type_id) {
                Q_UNUSED(instance_id);

                // Find the dock widget for this instance and connect signals
                if (type_id.toString() == QStringLiteral("DataTransformWidget")) {
                    for (auto * dock: _m_DockManager->dockWidgetsMap()) {
                        if (auto * dtw = dynamic_cast<DataTransform_Widget *>(dock->widget())) {
                            connect(dtw, &DataTransform_Widget::pipelineExecuted, this, [this](QString const & json) { _state_manager->workspace()->recordAppliedPipeline(
                                                                                                                               json.toStdString()); }, Qt::UniqueConnection);
                        }
                    }
                } else if (type_id.toString() == QStringLiteral("TableDesignerWidget")) {
                    for (auto * dock: _m_DockManager->dockWidgetsMap()) {
                        if (auto * tdw = dynamic_cast<TableDesignerWidget *>(dock->widget())) {
                            connect(tdw, &TableDesignerWidget::tableCreated, this, [this](QString const & table_id) { _state_manager->workspace()->recordTableDefinition(
                                                                                                                              table_id.toStdString()); }, Qt::UniqueConnection);
                        }
                    }
                }
            });
}
