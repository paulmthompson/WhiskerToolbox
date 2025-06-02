#include "mainwindow.hpp"

#include "ui_mainwindow.h"


#include "DataManager.hpp"
#include "DataManager/Media/HDF5_Data.hpp"
#include "DataManager/Media/Image_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Media/Video_Data.hpp"

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
#include "Media_Window.hpp"
#include "TimeFrame.hpp"
#include "Tongue_Widget/Tongue_Widget.hpp"
#include "Whisker_Widget.hpp"

#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>

#include <QElapsedTimer>
#include <QTimer>

#include <filesystem>
#include <iostream>
#include <variant>

MainWindow::MainWindow(QWidget * parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      _data_manager{std::make_shared<DataManager>()}

{
    ui->setupUi(this);

    _m_DockManager = new ads::CDockManager(this);

    //This is necessary to accept keyboard events
    this->setFocusPolicy(Qt::StrongFocus);

    _scene = new Media_Window(_data_manager, this);

    _verbose = false;

    ui->time_scrollbar->setDataManager(_data_manager);

    ui->media_widget->setScene(_scene);
    ui->media_widget->updateMedia();
    ui->media_widget->setDataManager(_data_manager);

    _createActions();// Creates callback functions

    QWidget::grabKeyboard();

    _buildInitialLayout();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::_buildInitialLayout() {
    auto media_dock_widget = new ads::CDockWidget(QString::fromStdString("media"));
    media_dock_widget->setWidget(ui->media_widget);
    auto dockArea = _m_DockManager->addDockWidget(ads::TopDockWidgetArea, media_dock_widget);

    registerDockWidget("scrollbar", ui->time_scrollbar, ads::BottomDockWidgetArea);

    auto * splitter = ads::internal::findParent<ads::CDockSplitter *>(dockArea);
    int const height = splitter->height();
    splitter->setSizes({height * 85 / 100, height * 15 / 100});
}

void MainWindow::_createActions() {
    connect(ui->actionLoad_Video, &QAction::triggered, this, &MainWindow::Load_Video);

    connect(ui->actionLoad_Images, &QAction::triggered, this, &MainWindow::Load_Images);

    connect(ui->actionLoad_JSON_Config, &QAction::triggered, this, &MainWindow::_loadJSONConfig);

    connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, ui->media_widget, &Media_Widget::LoadFrame);

    connect(ui->actionWhisker_Tracking, &QAction::triggered, this, &MainWindow::openWhiskerTracking);
    connect(ui->actionTongue_Tracking, &QAction::triggered, this, &MainWindow::openTongueTracking);
    connect(ui->actionMachine_Learning, &QAction::triggered, this, &MainWindow::openMLWidget);
    connect(ui->actionData_Viewer, &QAction::triggered, this, &MainWindow::openDataViewer);
    connect(ui->actionLoad_Points, &QAction::triggered, this, &MainWindow::openPointLoaderWidget);
    connect(ui->actionLoad_Masks, &QAction::triggered, this, &MainWindow::openMaskLoaderWidget);
    connect(ui->actionLoad_Lines, &QAction::triggered, this, &MainWindow::openLineLoaderWidget);
    connect(ui->actionLoad_Intervals, &QAction::triggered, this, &MainWindow::openIntervalLoaderWidget);
    connect(ui->actionLoad_Events, &QAction::triggered, this, &MainWindow::openEventLoaderWidget);
    connect(ui->actionLoad_Tensor, &QAction::triggered, this, &MainWindow::openTensorLoaderWidget);
    connect(ui->actionData_Manager, &QAction::triggered, this, &MainWindow::openDataManager);
    connect(ui->actionExport_Video, &QAction::triggered, this, &MainWindow::openVideoExportWidget);
    connect(ui->actionData_Transforms, &QAction::triggered, this, &MainWindow::openDataTransforms);
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

    auto vid_path = std::filesystem::path(vid_name.toStdString());
    auto extension = vid_path.extension();

    if (extension == ".mp4") {

        auto media = std::make_shared<VideoData>();
        media->LoadMedia(vid_name.toStdString());
        _data_manager->setData<VideoData>("media", media);

    } else if (extension == ".h5" || extension == ".mat") {

        auto media = std::make_shared<HDF5Data>();
        media->LoadMedia(vid_name.toStdString());
        _data_manager->setData<HDF5Data>("media", media);

    } else {
        std::cout << "Video file with extension " << vid_path.extension() << " not supported" << std::endl;
        return;
    }

    _LoadData();
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
    _data_manager->setData<ImageData>("media", media);

    _LoadData();
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

    auto data_info = load_data_from_json_config(_data_manager.get(), filename.toStdString());

    for (auto const & data: data_info) {
        if (data.data_class == "VideoData") {
            _LoadData();
        } else if (
                (data.data_class == "PointData") ||
                (data.data_class == "MaskData") ||
                (data.data_class == "LineData")) {
            ui->media_widget->setFeatureColor(data.key, data.color);
        }
    }
}

void MainWindow::_LoadData() {

    _updateFrameCount();

    ui->media_widget->updateMedia();
}

void MainWindow::_updateFrameCount() {
    auto media = _data_manager->getData<MediaData>("media");

    if (_data_manager->getTime()->getTotalFrameCount() != media->getTotalFrameCount()) {

        auto frame_count = media->getTotalFrameCount() - 1;

        std::cout << "There is a mismatch between the time in the time vector and number of samples in the video"
                  << "The video has " << frame_count
                  << " the time vector has " << _data_manager->getTime()->getTotalFrameCount() << std::endl;

        std::vector<int> t(frame_count);
        std::iota(std::begin(t), std::end(t), 0);

        auto new_timeframe = std::make_shared<TimeFrame>(t);

        _data_manager->removeTime("time");
        _data_manager->setTime("time", new_timeframe);
    }

    ui->time_scrollbar->updateScrollBarNewMax(_data_manager->getTime()->getTotalFrameCount());

    ui->time_scrollbar->changeScrollBarValue(0);
}

void MainWindow::openWhiskerTracking() {
    std::string const key = "whisker_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto whiskerWidget = std::make_unique<Whisker_Widget>(
                _scene,
                _data_manager,
                ui->time_scrollbar,
                this);
        connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, whiskerWidget.get(), &Whisker_Widget::LoadFrame);

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
    _m_DockManager->findDockWidget(QString::fromStdString(key))->toggleView();
}

void MainWindow::openTongueTracking() {
    std::string const key = "tongue_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto tongueWidget = std::make_unique<Tongue_Widget>(_scene, _data_manager, ui->time_scrollbar);
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

    if (_widgets.find(key) == _widgets.end()) {
        auto MLWidget = std::make_unique<ML_Widget>(
                _data_manager,
                ui->time_scrollbar);

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

    if (_widgets.find(key) == _widgets.end()) {
        auto DataViewerWidget = std::make_unique<DataViewer_Widget>(
                _data_manager,
                ui->time_scrollbar,
                this);

        DataViewerWidget->setObjectName(key);
        registerDockWidget(key, DataViewerWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(DataViewerWidget);
    }

    auto ptr = dynamic_cast<DataViewer_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::keyPressEvent(QKeyEvent * event) {
    static QWidget * lastSender = nullptr;

    auto handleEvent = [this, event](QWidget * sender) {
        if (sender == lastSender) {
            return;
        }
        lastSender = sender;

        if (sender->objectName().toStdString().find("dockWidget") != std::string::npos) {
            auto dockWidget = _m_DockManager->findDockWidget("whisker_widget");
            if (dockWidget && dockWidget->widget() != sender) {
                QApplication::sendEvent(dockWidget->widget(), event);
            }
        } else {
            QApplication::sendEvent(sender, event);
        }
    };

    if (event->key() == Qt::Key_Right) {
        ui->time_scrollbar->changeScrollBarValue(1, true);
    } else if (event->key() == Qt::Key_Left) {
        ui->time_scrollbar->changeScrollBarValue(-1, true);
    } else {
        auto focusedWidget = QApplication::focusWidget();
        if (focusedWidget && focusedWidget != this) {
            handleEvent(focusedWidget);
        }
    }

    lastSender = nullptr;
}

void MainWindow::openPointLoaderWidget() {
    std::string const key = "PointLoader_widget";

    if (_widgets.find(key) == _widgets.end()) {
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

    if (_widgets.find(key) == _widgets.end()) {
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

    if (_widgets.find(key) == _widgets.end()) {
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

    if (_widgets.find(key) == _widgets.end()) {
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

    if (_widgets.find(key) == _widgets.end()) {
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

    if (_widgets.find(key) == _widgets.end()) {
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
    std::string const key = "DataManager_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto dm_widget = std::make_unique<DataManager_Widget>(
                _scene,
                _data_manager,
                ui->time_scrollbar,
                this);

        dm_widget->setObjectName(key);
        registerDockWidget(key, dm_widget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(dm_widget);
    }

    auto ptr = dynamic_cast<DataManager_Widget *>(_widgets[key].get());
    //connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, ptr, &DataManager_Widget::LoadFrame);
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openVideoExportWidget() {
    std::string const key = "VideoExport_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto vid_widget = std::make_unique<Export_Video_Widget>(
                _data_manager,
                _scene,
                ui->time_scrollbar,
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

    if (_widgets.find(key) == _widgets.end()) {
        auto dt_widget = std::make_unique<DataTransform_Widget>(
                _data_manager,
                this);

        dt_widget->setObjectName(key);
        registerDockWidget(key, dt_widget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(dt_widget);
    }

    auto ptr = dynamic_cast<DataTransform_Widget *>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}
