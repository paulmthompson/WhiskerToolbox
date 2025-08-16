#include "TimeScrollBar.hpp"

#include "ui_TimeScrollBar.h"

#include "DataManager.hpp"
#include "Media/Video_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QFileDialog>
#include <QTimer>

#include <iostream>

TimeScrollBar::TimeScrollBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TimeScrollBar)
{
    ui->setupUi(this);

    _timer = new QTimer(this);

    connect(_timer, &QTimer::timeout, this, &TimeScrollBar::_vidLoop);

    connect(ui->horizontalScrollBar, &QScrollBar::valueChanged, this, &TimeScrollBar::Slider_Scroll);
    connect(ui->horizontalScrollBar, &QScrollBar::sliderMoved, this, &TimeScrollBar::Slider_Drag); // For drag events

    connect(ui->play_button, &QPushButton::clicked, this, &TimeScrollBar::PlayButton);
    connect(ui->rewind, &QPushButton::clicked, this, &TimeScrollBar::RewindButton);
    connect(ui->fastforward, &QPushButton::clicked, this, &TimeScrollBar::FastForwardButton);

    // Set up spin box with keyboard tracking disabled (only update on Enter key or arrow clicks)
    ui->frame_spinbox->setKeyboardTracking(false);
    connect(ui->frame_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimeScrollBar::FrameSpinBoxChanged);
};

TimeScrollBar::~TimeScrollBar() {
    delete ui;
    _timer->stop();
}

/*
We can click and hold the slider to move to a new position
In the case that we are dragging the slider, to make this optimally smooth, we should not add any new decoding frames
until we have finished the most recent one.
 */

void TimeScrollBar::Slider_Drag(int newPos)
{
    static_cast<void>(newPos);

    auto media = _data_manager->getData<MediaData>("media");
    if (dynamic_cast<VideoData*>(media.get())) {
        auto current_frame = ui->horizontalScrollBar->sliderPosition();
        auto keyframe = dynamic_cast<VideoData*>(media.get())->FindNearestSnapFrame(current_frame);
        ui->horizontalScrollBar->setSliderPosition(keyframe);
    }
}


void TimeScrollBar::Slider_Scroll(int newPos)
{
    if (_verbose) {
        std::cout << "The slider position is " << ui->horizontalScrollBar->sliderPosition() << std::endl;
    }

    auto frame_id = _data_manager->getTime()->checkFrameInbounds(newPos);
    ui->horizontalScrollBar->setSliderPosition(newPos);
    _data_manager->setCurrentTime(frame_id);
    _updateFrameLabels(frame_id);

    emit timeChanged(frame_id);
}


void TimeScrollBar::_updateFrameLabels(int frame_num) {

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));
    
    auto video_time = video_timeframe->getTimeAtIndex(TimeFrameIndex(frame_num));

    ui->time_label->setText(QString::number(video_time));

    // Update the spin box value without triggering valueChanged signal
    ui->frame_spinbox->blockSignals(true);
    ui->frame_spinbox->setValue(frame_num);
    ui->frame_spinbox->blockSignals(false);
}

void TimeScrollBar::updateScrollBarNewMax(int new_max) {

    ui->frame_count_label->setText(QString::number(new_max));
    ui->horizontalScrollBar->setMaximum(new_max);
    ui->frame_spinbox->setMaximum(new_max);

}


void TimeScrollBar::PlayButton()
{

    const int timer_period_ms = 40;

    if (_play_mode) {

        _timer->stop();
        ui->play_button->setText(QString("Play"));
        _play_mode = false;

        ui->horizontalScrollBar->blockSignals(true);
        ui->horizontalScrollBar->setValue(_data_manager->getCurrentTime());
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
    auto current_time = _data_manager->getCurrentTime() + _play_speed;
     ui->horizontalScrollBar->setSliderPosition(current_time);
}

void TimeScrollBar::changeScrollBarValue(int new_value, bool relative)
{
    auto min_value = ui->horizontalScrollBar->minimum();
    auto max_value = ui->horizontalScrollBar->maximum();

    if (relative)
    {
        new_value = _data_manager->getCurrentTime() + new_value;
    }

    if (new_value < min_value) {
        std::cout << "Cannot set value below minimum" << std::endl;
        return;
    }

    if (new_value > max_value) {
        std::cout << "Cannot set value above maximum" << std::endl;
        return;
    }

    Slider_Scroll(new_value);

}

void TimeScrollBar::FrameSpinBoxChanged(int new_frame)
{
    auto frame_id = _data_manager->getTime()->checkFrameInbounds(new_frame);
    _data_manager->setCurrentTime(frame_id);
    ui->horizontalScrollBar->setSliderPosition(frame_id);
    _updateFrameLabels(frame_id);

    emit timeChanged(frame_id);
}
