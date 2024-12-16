#include "mainwindow.hpp"

#include "ui_mainwindow.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "analog_viewer.hpp"

#include "DataManager.hpp"
#include "DataManager/Media/HDF5_Data.hpp"
#include "DataManager/Media/Image_Data.hpp"
#include "DataManager/Media/Video_Data.hpp"

#include "DataViewer_Widget/DataViewer_Widget.hpp"
#include "DockAreaWidget.h"
#include "DockSplitter.h"
#include "Image_Processing_Widget/Image_Processing_Widget.hpp"
#include "Label_Widget.hpp"
#include "Media_Widget/Media_Widget.hpp"
#include "Media_Window.hpp"
#include "ML_Widget/ML_Widget.hpp"
#include "TimeFrame.hpp"
#include "Tongue_Widget/Tongue_Widget.hpp"
#include "Tracking_Widget/Tracking_Widget.hpp"
#include "Whisker_Widget.hpp"

#include "Points/Point_Data.hpp"

#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>

#include <QTimer>
#include <QElapsedTimer>

#include <filesystem>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
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

    ui->media_widget->setMainWindow(this);
    ui->media_widget->setScene(_scene);
    ui->media_widget->updateMedia();

    _createActions(); // Creates callback functions

    QWidget::grabKeyboard();

    _buildInitialLayout();

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::_buildInitialLayout()
{
    auto media_dock_widget = new ads::CDockWidget(QString::fromStdString("media"));
    media_dock_widget->setWidget(ui->media_widget);
    auto dockArea = _m_DockManager->addDockWidget(ads::TopDockWidgetArea, media_dock_widget);

    registerDockWidget("scrollbar", ui->time_scrollbar, ads::BottomDockWidgetArea);

    ads::CDockSplitter* splitter = ads::internal::findParent<ads::CDockSplitter*>(dockArea);
    int height = splitter->height();
    splitter->setSizes({height*85/100,height*15/100});
}

void MainWindow::_createActions()
{
    connect(ui->actionLoad_Video, &QAction::triggered, this, &MainWindow::Load_Video);

    connect(ui->actionLoad_Images, &QAction::triggered,this, &MainWindow::Load_Images);

    connect(ui->actionLoad_Analog_Time_Series_CSV, &QAction::triggered, this, &MainWindow::_loadAnalogTimeSeriesCSV);
    connect(ui->actionLoad_Digital_Time_Series_CSV, &QAction::triggered, this, &MainWindow::_loadDigitalTimeSeriesCSV);

    connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, _scene, &Media_Window::LoadFrame);

    connect(ui->actionWhisker_Tracking, &QAction::triggered, this, &MainWindow::openWhiskerTracking);
    connect(ui->actionLabel_Maker, &QAction::triggered, this, &MainWindow::openLabelMaker);
    connect(ui->actionAnalog_Viewer, &QAction::triggered, this, &MainWindow::openAnalogViewer);
    connect(ui->actionImage_Processing, &QAction::triggered, this, &MainWindow::openImageProcessing);
    connect(ui->actionTongue_Tracking, &QAction::triggered, this, &MainWindow::openTongueTracking);
    connect(ui->actionTracking_Widget, &QAction::triggered, this, &MainWindow::openTrackingWidget);
    connect(ui->actionMachine_Learning, &QAction::triggered, this, &MainWindow::openMLWidget);
    connect(ui->actionData_Viewer, &QAction::triggered, this, &MainWindow::openDataViewer);

    connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, _scene, &Media_Window::LoadFrame);
}

/*
The Load_Video callback is executed whenever a the load_video option is selected.
If a video is selected, that video will be loaded and the first frame will be
drawn on the video screen.

*/
void MainWindow::Load_Video()
{
    auto vid_name =  QFileDialog::getOpenFileName(
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
        _data_manager->setMedia(media);

    } else if (extension == ".h5" || extension == ".mat") {

        auto media = std::make_shared<HDF5Data>();
        media->LoadMedia(vid_name.toStdString());
        _data_manager->setMedia(media);

    } else {
        std::cout << "Video file with extension " << vid_path.extension() << " not supported" << std::endl;
        return;
    }

    _LoadData();
}

void MainWindow::Load_Images() {
    auto dir_name =  QFileDialog::getExistingDirectory(
        this,
        "Load Video File",
        QDir::currentPath());

    if (dir_name.isNull()) {
        return;
    }

    auto media = std::make_shared<ImageData>();
    media->LoadMedia(dir_name.toStdString());
    _data_manager->setMedia(media);

    _LoadData();

}

void MainWindow::_loadAnalogTimeSeriesCSV()
{
    auto filename =  QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; CSV (*.csv)");

    if (filename.isNull()) {
        return;
    }

    auto series = load_analog_series_from_csv(filename.toStdString());

    auto path = std::filesystem::path(filename.toStdString());
    auto key = path.filename().replace_extension("").string();

    _data_manager->createAnalogTimeSeries(key);

    _data_manager->getAnalogTimeSeries(key)->setData(series);

    std::cout << "Loaded series " << key << " with " <<
        _data_manager->getAnalogTimeSeries(key)->getAnalogTimeSeries().size() << " points " << std::endl;

    if (_widgets.find("analog_viewer") != _widgets.end()) {
        dynamic_cast<Analog_Viewer*>(_widgets["analog_viewer"].get())->plotAnalog(key);
    }
}

void MainWindow::_loadDigitalTimeSeriesCSV(){
    auto filename =  QFileDialog::getOpenFileName(
        this,
        "Load Video File",
        QDir::currentPath(),
        "All files (*.*) ;; CSV (*.csv)");

    if (filename.isNull()) {
        return;
    }

    auto series = load_digital_series_from_csv(filename.toStdString());

    auto path = std::filesystem::path(filename.toStdString());
    auto key = path.filename().replace_extension("").string();

    _data_manager->createDigitalTimeSeries(key);

    _data_manager->getDigitalTimeSeries(key)->setData(series);

    std::cout << "Loaded series " << key << " with " <<
                                                     _data_manager->getDigitalTimeSeries(
                                                             key)->getDigitalIntervalSeries().size() << " points " << std::endl;

    if (_widgets.find("analog_viewer") != _widgets.end()) {
        dynamic_cast<Analog_Viewer*>(_widgets["analog_viewer"].get())->plotDigital(key);
    }
}

void MainWindow::_LoadData() {

    _updateFrameCount();

    ui->media_widget->updateMedia();
}

void MainWindow::_updateFrameCount()
{
    auto frame_count = _data_manager->getMediaData()->getTotalFrameCount() - 1;

    _data_manager->getTime()->updateTotalFrameCount(frame_count);

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

    dynamic_cast<Whisker_Widget*>(_widgets[key].get())->openWidget();
    showDockWidget(key);
}

void MainWindow::registerDockWidget(std::string const & key, QWidget* widget, ads::DockWidgetArea area)
{
    auto dock_widget = new ads::CDockWidget(QString::fromStdString(key));
    dock_widget->setWidget(widget);
    _m_DockManager->addDockWidget(area, dock_widget);
}

void MainWindow::showDockWidget(std::string const & key)
{
    _m_DockManager->findDockWidget(QString::fromStdString(key))->toggleView();
}

void MainWindow::openLabelMaker() {

    std::string const key = "label_maker";

    if (_widgets.find(key) == _widgets.end()) {
        auto labelMaker = std::make_unique<Label_Widget>(_scene, _data_manager);
        labelMaker->setObjectName(key);
        registerDockWidget(key, labelMaker.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(labelMaker);
    }

    auto ptr = dynamic_cast<Label_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openAnalogViewer()
{
    std::string const key = "analog_viewer";

    if (_widgets.find(key) == _widgets.end()) {
        auto analogViewer = std::make_unique<Analog_Viewer>(_data_manager, ui->time_scrollbar, this);
        analogViewer->setObjectName(key);
        registerDockWidget(key, analogViewer.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(analogViewer);

        connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, dynamic_cast<Analog_Viewer*>(_widgets[key].get()), &Analog_Viewer::SetFrame);
    }

    auto ptr = dynamic_cast<Analog_Viewer*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openImageProcessing()
{
    std::string const key = "image_processing";

    if (_widgets.find(key) == _widgets.end()) {
        auto imageProcessing = std::make_unique<Image_Processing_Widget>(_scene, _data_manager);
        imageProcessing->setObjectName(key);
        registerDockWidget(key, imageProcessing.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(imageProcessing);
    }

    auto ptr = dynamic_cast<Image_Processing_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openTongueTracking()
{
    std::string const key = "tongue_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto tongueWidget = std::make_unique<Tongue_Widget>(_scene, _data_manager, ui->time_scrollbar);
        tongueWidget->setObjectName(key);
        registerDockWidget(key, tongueWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(tongueWidget);
    }

    auto ptr = dynamic_cast<Tongue_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openTrackingWidget()
{
    std::string const key = "tracking_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto trackingWidget = std::make_unique<Tracking_Widget>(
            _scene,
            _data_manager,
            ui->time_scrollbar,
            this);
        connect(ui->time_scrollbar, &TimeScrollBar::timeChanged, trackingWidget.get(), &Tracking_Widget::LoadFrame);

        trackingWidget->setObjectName(key);
        registerDockWidget(key, trackingWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(trackingWidget);
    }

    auto ptr = dynamic_cast<Tracking_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openMLWidget()
{
    std::string const key = "ML_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto MLWidget = std::make_unique<ML_Widget>(
            _scene,
            _data_manager,
            ui->time_scrollbar,
            this);

        MLWidget->setObjectName(key);
        registerDockWidget(key, MLWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(MLWidget);
    }

    auto ptr = dynamic_cast<ML_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::openDataViewer()
{
    std::string const key = "DataViewer_widget";

    if (_widgets.find(key) == _widgets.end()) {
        auto DataViewerWidget = std::make_unique<DataViewer_Widget>(
            _scene,
            _data_manager,
            ui->time_scrollbar,
            this);

        DataViewerWidget->setObjectName(key);
        registerDockWidget(key, DataViewerWidget.get(), ads::RightDockWidgetArea);
        _widgets[key] = std::move(DataViewerWidget);
    }

    auto ptr = dynamic_cast<DataViewer_Widget*>(_widgets[key].get());
    ptr->openWidget();

    showDockWidget(key);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {

    if (event->key() == Qt::Key_Right) {
        ui->time_scrollbar->changeScrollBarValue(1,true);
    } else if (event->key() == Qt::Key_Left){
        ui->time_scrollbar->changeScrollBarValue(-1,true);
    } else {
        auto focusedWidget = QApplication::focusWidget();

        if (focusedWidget && focusedWidget != this) {
            // Check if the focused widget is a dock widget
            if (focusedWidget->objectName().toStdString().find("dockWidget") != std::string::npos) {
                auto dockWidget = _m_DockManager->findDockWidget("whisker_widget");
                if (dockWidget && dockWidget->widget() != focusedWidget) {
                    QApplication::sendEvent(dockWidget->widget(), event);
                }
            } else {
                // Pass the event to the focused widget if it is not the main window
                QApplication::sendEvent(focusedWidget, event);
            }
        }

        /*
        if (focusedWidget) {
            if (!this->isActiveWindow()) {
                std::cout << "Sending event to another window" << std::endl;
                QApplication::sendEvent(focusedWidget, event);
            } else if (focusedWidget->objectName().toStdString().find("dockWidget") != std::string::npos) {

                std::cout << "Dock widget is active" << std::endl;
                auto dock_widget = _m_DockManager->findDockWidget("whisker_widget");
                if (dock_widget)
                {
                    QApplication::sendEvent(dock_widget->widget(), event);
                }



            } else if (focusedWidget->objectName().toStdString().find("MainWindow") != std::string::npos) {
                // Main window is focus widget. Don't want to send infinite loop
            } else {

                std::cout << "Focus widget: " << focusedWidget->objectName().toStdString() << std::endl;
                QApplication::sendEvent(focusedWidget, event);

            }
        }
         */
    }
}
