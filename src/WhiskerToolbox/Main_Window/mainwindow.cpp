#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QImage>
#include <QVideoWidget>
#include <QVideoFrameFormat>
#include <QVideoFrame>
#include <QGraphicsPixmapItem>
#include <QKeyEvent>

#include <QTimer>
#include <QElapsedTimer>

#include "Covariate_Widget/Covariate_Widget.h"
#include "Whisker_Widget.h"

#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    _data_manager{std::make_shared<DataManager>()}

{
    ui->setupUi(this);

    //This is necessary to accept keyboard events
    this->setFocusPolicy(Qt::StrongFocus);

    _scene = new Media_Window(_data_manager, this);

    _verbose = false;

    ui->time_scrollbar->setDataManager(_data_manager);

    _updateMedia();

    _createActions(); // Creates callback functions

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::_createActions()
{
    connect(ui->actionLoad_Video,SIGNAL(triggered()),this,SLOT(Load_Video()));

    connect(ui->actionLoad_Images,SIGNAL(triggered()),this,SLOT(Load_Images()));

    connect(ui->time_scrollbar, SIGNAL(timeChanged(int)),_scene,SLOT(LoadFrame(int)));

    connect(ui->pushButton,SIGNAL(clicked()),this,SLOT(addCovariate()));
    connect(ui->pushButton_2,SIGNAL(clicked()),this,SLOT(removeCovariate()));

    connect(ui->actionWhisker_Tracking,SIGNAL(triggered()),this,SLOT(openWhiskerTracking()));
    connect(ui->actionLabel_Maker,SIGNAL(triggered()),this,SLOT(openLabelMaker()));
    connect(ui->actionAnalog_Viewer,SIGNAL(triggered()),this,SLOT(openAnalogViewer()));

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
                "All files (*.*) ;; MP4 (*.mp4)");

    if (vid_name.isNull()) {
        return;
    }

    // Create video data object
    // Pass video data object to scene?

    _data_manager->createMedia(DataManager::Video);

    _updateMedia();

    _LoadData(vid_name.toStdString());

}

void MainWindow::Load_Images() {
    auto dir_name =  QFileDialog::getExistingDirectory(
        this,
        "Load Video File",
        QDir::currentPath());

    if (dir_name.isNull()) {
        return;
    }

    _data_manager->createMedia(DataManager::Images);

    _updateMedia();

    _LoadData(dir_name.toStdString());

}

void MainWindow::_LoadData(std::string filepath) {

    _data_manager->getMediaData()->LoadMedia(filepath);
    auto frame_count = _data_manager->getMediaData()->getTotalFrameCount() - 1;

    _data_manager->getTime()->updateTotalFrameCount(frame_count);

    ui->time_scrollbar->updateScrollBarNewMax(_data_manager->getTime()->getTotalFrameCount());

    ui->time_scrollbar->changeScrollBarValue(0);

}

//If we load new media, we need to update the references to it. Widgets that use that media need to be updated to it.
//these include whisker_widget and label_widget
//There are probably better ways to do this.
void MainWindow::_updateMedia() {

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

}

void MainWindow::openWhiskerTracking() {

    // We create a whisker widget. We only want to load this module one time,
    // so if we exit the window, it is not created again
    if (!_ww) {
        _ww = new Whisker_Widget(_scene, _data_manager);
        connect(ui->time_scrollbar, SIGNAL(timeChanged(int)),_ww,SLOT(LoadFrame(int)));
        std::cout << "Whisker Tracker Constructed" << std::endl;
    } else {
        std::cout << "Whisker Tracker already exists" << std::endl;
    }
    _ww->openWidget();
}

void MainWindow::openLabelMaker() {

    // We create a whisker widget. We only want to load this module one time,
    // so if we exit the window, it is not created again
    if (!_label_maker) {
        _label_maker = new Label_Widget(_scene,_data_manager);
        std::cout << "Label Maker Constructed" << std::endl;
    } else {
        std::cout << "Label Maker already exists" << std::endl;
    }
    _label_maker->openWidget();
}

void MainWindow::openAnalogViewer()
{
    if (!_analog_viewer) {
        _analog_viewer = new Analog_Viewer();
        std::cout << "Analog Viewer Constructed" << std::endl;
    } else {
        std::cout << "Analog Viewer already exists" << std::endl;
    }
    _analog_viewer->openWidget();
}

void MainWindow::addCovariate() {

    auto item = new QListWidgetItem(ui->listWidget);
    ui->listWidget->addItem(item);

    auto myWidget = new Covariate_Widget(this);

    item->setSizeHint(myWidget->frameSize());

    ui->listWidget->setItemWidget(item,myWidget);

}

void MainWindow::removeCovariate() {
    auto item = ui->listWidget->currentItem();
    ui->listWidget->removeItemWidget(item);
    delete item;
}

void MainWindow::keyPressEvent(QKeyEvent *event) {

    //Data manager should be responsible for loading new value of data object
    //Main window can update displays with new data object position
    //Frame label is also updated.

    if (event->key() == Qt::Key_Right) {
        ui->time_scrollbar->changeScrollBarValue(1,true);
    } else if (event->key() == Qt::Key_Left){
        ui->time_scrollbar->changeScrollBarValue(-1,true);
    } else {
        std::cout << "Key pressed but nothing to do" << std::endl;
        QMainWindow::keyPressEvent(event);
    }

}

