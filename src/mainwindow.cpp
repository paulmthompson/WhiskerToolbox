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

#include "Covariate_Widget.h"
#include "Whisker_Widget.h"

#include <stdio.h>
#include <functional>
#include <memory>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    frame_count = 0;
    play_speed = 1;

    //This is necessary to accept keyboard events
    this->setFocusPolicy(Qt::StrongFocus);

    this->scene = new Video_Window(this);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::vidLoop);

    play_mode = false;

    ui->graphicsView->setScene(this->scene);
    ui->graphicsView->show();

    createActions(); // Creates callback functions

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::vidLoop()
{
    auto loaded_frame = scene->AdvanceFrame(this->play_speed);
    ui->frame_label->setText(QString::number(loaded_frame));
}

void MainWindow::createActions()
{
    connect(ui->actionLoad_Video,SIGNAL(triggered()),this,SLOT(Load_Video()));

    //connect(ui->horizontalScrollBar,SIGNAL(actionTriggered(int)),this,SLOT(Slider_Scroll(int)));
    connect(ui->horizontalScrollBar,SIGNAL(valueChanged(int)),this,SLOT(Slider_Scroll(int)));
    connect(ui->horizontalScrollBar,SIGNAL(sliderMoved(int)),this,SLOT(Slider_Drag(int))); // For drag events
    connect(ui->horizontalScrollBar,SIGNAL(sliderReleased()),this,SLOT(updateDisplay()));

    connect(ui->play_button,SIGNAL(clicked()),this,SLOT(PlayButton()));
    connect(ui->rewind,SIGNAL(clicked()),this,SLOT(RewindButton()));
    connect(ui->fastforward,SIGNAL(clicked()),this,SLOT(FastForwardButton()));

    connect(ui->pushButton,SIGNAL(clicked()),this,SLOT(addCovariate()));
    connect(ui->pushButton_2,SIGNAL(clicked()),this,SLOT(removeCovariate()));

    connect(ui->actionWhisker_Tracking,SIGNAL(triggered()),this,SLOT(openWhiskerTracking()));
    connect(ui->actionLabel_Maker,SIGNAL(triggered()),this,SLOT(openLabelMaker()));

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
    this->frame_count = scene->LoadMedia(vid_name.toStdString()) - 1; // We are zero indexing so subtract 1
    ui->frame_count_label->setText(QString::number(this->frame_count));
    ui->horizontalScrollBar->setMaximum(this->frame_count);

    scene->LoadFrame(0);

}

void MainWindow::openWhiskerTracking() {

    // We create a whisker widget. We only want to load this module one time,
    // so if we exit the window, it is not created again
    if (!this->ww) {
        this->ww = new Whisker_Widget(this->scene);
        std::cout << "Whisker Tracker Constructed" << std::endl;
    } else {
        std::cout << "Whisker Tracker already exists" << std::endl;
    }
    this->ww->openWidget();
}

void MainWindow::openLabelMaker() {

    // We create a whisker widget. We only want to load this module one time,
    // so if we exit the window, it is not created again
    if (!this->label_maker) {
        this->label_maker = new Label_Widget(this->scene);
        std::cout << "Label Maker Constructed" << std::endl;
    } else {
        std::cout << "Label Maker already exists" << std::endl;
    }
    this->label_maker->openWidget();
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

/*
The play button starts the video automatically advancing frames
The timer runs every 40 ms, and the number of frames to advance will be dictated by the speed
selected with forward and reverse buttons.
The timer is fixed at 25 fps, so faster will result in some frames not being flashed to the screen.
*/
void MainWindow::PlayButton()
{

    const int timer_period_ms = 40;

    if (play_mode) {

        timer->stop();
        ui->play_button->setText(QString("Play"));
        play_mode = false;

        ui->horizontalScrollBar->blockSignals(true);
        ui->horizontalScrollBar->setValue(scene->getLastLoadedFrame());
        ui->horizontalScrollBar->blockSignals(false);

    } else {
        ui->play_button->setText(QString("Pause"));
        timer->start(timer_period_ms);
        play_mode = true;
    }
}

/*
Increases the speed of a playing video in increments of the base_fps (default = 25)
*/
void MainWindow::RewindButton()
{
    const int play_speed_base_fps = 25;
    if (this->play_speed > 1)
    {
        this->play_speed--;
        ui->fps_label->setText(QString::number(play_speed_base_fps * this->play_speed));
    }
}

/*
Decreases the speed of a playing video in increments of the base_fps (default = 25)
*/
void MainWindow::FastForwardButton()
{
    const int play_speed_base_fps = 25;

    this->play_speed++;
    ui->fps_label->setText(QString::number(play_speed_base_fps * this->play_speed));
}

void MainWindow::keyPressEvent(QKeyEvent *event) {

    if (event->key() == Qt::Key_Right) {
        auto loaded_frame = scene->AdvanceFrame(1);
        ui->frame_label->setText(QString::number(loaded_frame));
    } else if (event->key() == Qt::Key_Left){
        auto loaded_frame = scene->AdvanceFrame(-1);
        ui->frame_label->setText(QString::number(loaded_frame));
    } else {
        std::cout << "Key pressed but nothing to do" << std::endl;
        QMainWindow::keyPressEvent(event);
    }

}

/*
We can click and hold the slider to move to a new position
In the case that we are dragging the slider, to make this optimally smooth, we should not add any new decoding frames
until we have finished the most recent one.
 */

void MainWindow::Slider_Drag(int action)
{
    auto keyframe = this->scene->findNearestKeyframe(ui->horizontalScrollBar->sliderPosition());
    std::cout << "The slider position is " << ui->horizontalScrollBar->sliderPosition() << " and the nearest keyframe is " << keyframe << std::endl;
    ui->horizontalScrollBar->setSliderPosition(keyframe);
}


void MainWindow::Slider_Scroll(int newPos)
{
    std::cout << "The slider position is " << ui->horizontalScrollBar->sliderPosition() << std::endl;

    scene->LoadFrame(newPos);
    ui->frame_label->setText(QString::number(newPos));
}

void MainWindow::updateDisplay() {
    //this->scene->UpdateCanvas();
    //scene->LoadFrame(ui->horizontalScrollBar->sliderPosition());
    //this->selected_whisker = 0;
    //ui->frame_label->setText(QString::number(ui->horizontalScrollBar->sliderPosition()));
}

