#include "MediaInterval_Widget.hpp"
#include "ui_MediaInterval_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Media_Window/Media_Window.hpp"

#include <iostream>

MediaInterval_Widget::MediaInterval_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaInterval_Widget),
      _data_manager{std::move(data_manager)},
      _scene{std::move(scene)} {
    ui->setupUi(this);

    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaInterval_Widget::_setIntervalAlpha);
    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaInterval_Widget::_setIntervalColor);

    // Connect new controls
    connect(ui->box_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaInterval_Widget::_setBoxSize);
    connect(ui->frame_range_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaInterval_Widget::_setFrameRange);
    connect(ui->location_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MediaInterval_Widget::_setLocation);
}

MediaInterval_Widget::~MediaInterval_Widget() {
    delete ui;
}

void MediaInterval_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color picker and controls to the current interval configuration if available
    if (!key.empty()) {
        auto config = _scene->getIntervalConfig(key);

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config.value()->hex_color));
            ui->color_picker->setAlpha(static_cast<int>(config.value()->alpha * 100));

            // Set the new control values
            ui->box_size_spinbox->setValue(config.value()->box_size);
            ui->frame_range_spinbox->setValue(config.value()->frame_range);
            ui->location_combobox->setCurrentIndex(static_cast<int>(config.value()->location));
        }
    }
}

void MediaInterval_Widget::_setIntervalAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->alpha = alpha_float;
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setIntervalColor(QString const & hex_color) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->hex_color = hex_color.toStdString();
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setBoxSize(int size) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->box_size = size;
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setFrameRange(int range) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->frame_range = range;
        }
        _scene->UpdateCanvas();
    }
}

void MediaInterval_Widget::_setLocation(int location_index) {
    if (!_active_key.empty()) {
        auto interval_opts = _scene->getIntervalConfig(_active_key);
        if (interval_opts.has_value()) {
            interval_opts.value()->location = static_cast<IntervalLocation>(location_index);
        }
        _scene->UpdateCanvas();
    }
}
