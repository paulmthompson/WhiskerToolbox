
#include "time_scrollbar.hpp"

#include "ui_time_scrollbar.h"

#include "Media/Video_Data.hpp"

#include <QFileDialog>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

TimeScrollBar::TimeScrollBar(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    _data_manager{data_manager},
    _verbose{false},
    _play_mode{false},
    _play_speed{1},
    ui(new Ui::TimeScrollBar)
{
    ui->setupUi(this);

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &TimeScrollBar::_vidLoop);

    connect(ui->horizontalScrollBar,SIGNAL(valueChanged(int)),this,SLOT(Slider_Scroll(int)));
    connect(ui->horizontalScrollBar,SIGNAL(sliderMoved(int)),this,SLOT(Slider_Drag(int))); // For drag events

    connect(ui->play_button,SIGNAL(clicked()),this,SLOT(PlayButton()));
    connect(ui->rewind,SIGNAL(clicked()),this,SLOT(RewindButton()));
    connect(ui->fastforward,SIGNAL(clicked()),this,SLOT(FastForwardButton()));

};

TimeScrollBar::~TimeScrollBar() {
    delete ui;
}

/*
We can click and hold the slider to move to a new position
In the case that we are dragging the slider, to make this optimally smooth, we should not add any new decoding frames
until we have finished the most recent one.
 */

void TimeScrollBar::Slider_Drag(int action)
{
    if (dynamic_cast<VideoData*>(_data_manager->getMediaData().get())) {
        auto current_frame = ui->horizontalScrollBar->sliderPosition();
        auto keyframe = dynamic_cast<VideoData*>(_data_manager->getMediaData().get())->FindNearestSnapFrame(current_frame);
        ui->horizontalScrollBar->setSliderPosition(keyframe);
    }
}


void TimeScrollBar::Slider_Scroll(int newPos)
{
    if (_verbose) {
        std::cout << "The slider position is " << ui->horizontalScrollBar->sliderPosition() << std::endl;
    }

    //_LoadFrame(newPos);
    //_updateFrameLabels(newPos);
}


void TimeScrollBar::_updateFrameLabels(int frame_num) {
    ui->frame_label->setText(QString::number(frame_num));
}

void TimeScrollBar::_updateScrollBarNewMax(int new_max) {

    ui->frame_count_label->setText(QString::number(new_max));
    ui->horizontalScrollBar->setMaximum(new_max);

}


void TimeScrollBar::PlayButton()
{

    const int timer_period_ms = 40;

    if (_play_mode) {

        _timer->stop();
        ui->play_button->setText(QString("Play"));
        _play_mode = false;

        ui->horizontalScrollBar->blockSignals(true);
        ui->horizontalScrollBar->setValue(_data_manager->getTime()->getLastLoadedFrame());
        ui->horizontalScrollBar->blockSignals(false);

    } else {
        ui->play_button->setText(QString("Pause"));
        _timer->start(timer_period_ms);
        _play_mode = true;
    }
}

/*
Increases the speed of a playing video in increments of the base_fps (default = 25)
*/
void TimeScrollBar::RewindButton()
{
    const int play_speed_base_fps = 25;
    if (_play_speed > 1)
    {
        _play_speed--;
        ui->fps_label->setText(QString::number(play_speed_base_fps * _play_speed));
    }
}

/*
Decreases the speed of a playing video in increments of the base_fps (default = 25)
*/
void TimeScrollBar::FastForwardButton()
{
    const int play_speed_base_fps = 25;

    _play_speed++;
    ui->fps_label->setText(QString::number(play_speed_base_fps * _play_speed));
}

void TimeScrollBar::_vidLoop()
{
    //_updateDataDisplays(_play_speed);
}
